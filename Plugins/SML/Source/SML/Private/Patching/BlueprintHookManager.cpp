#include "Patching/BlueprintHookManager.h"
#include "Patching/BlueprintHookHelper.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Toolkit/KismetBytecodeDisassemblerJson.h"

// Whenever to debug blueprint hooking. When enabled, JSON files with script bytecode before and after installing hook will be generated
#define DEBUG_BLUEPRINT_HOOKING 0

DEFINE_LOG_CATEGORY(LogBlueprintHookManager);

#define WRITE_UNALIGNED(Arr, Type, Value) \
    Arr.AddUninitialized(sizeof(Type)); \
    FPlatformMemory::WriteUnaligned<Type>(&AppendedCode[Arr.Num() - sizeof(Type)], (Type) Value);

void UBlueprintHookManager::HandleHookedFunctionCall(FFrame& Stack, int32 HookOffset) {
    FFunctionHookInfo& FunctionHookInfo = HookedFunctions.FindChecked(Stack.Node);
    FunctionHookInfo.InvokeBlueprintHook(Stack, HookOffset);
}

#if DEBUG_BLUEPRINT_HOOKING
void DebugDumpFunctionScriptCode(UFunction* Function, int32 HookOffset, const FString& Postfix) {
    const FString FileLocation = FPaths::RootDir() + FString::Printf(TEXT("BlueprintHookingDebug_%s_%s_at_%d_%s.json"), *Function->GetOuter()->GetName(), *Function->GetName(), HookOffset, *Postfix);

    FKismetBytecodeDisassemblerJson Disassembler;
    const TArray<TSharedPtr<FJsonValue>> Statements = Disassembler.SerializeFunction(Function);

    FString OutJsonString;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
    FJsonSerializer::Serialize(Statements, Writer);

    FFileHelper::SaveStringToFile(OutJsonString, *FileLocation);
    UE_LOG(LogBlueprintHookManager, Display, TEXT("Dumped hooked function bytecode to %s (%s)"), *FileLocation, *Postfix);
}
#endif

void UBlueprintHookManager::InstallBlueprintHook(UFunction* Function, int32 HookOffset) {
    TArray<uint8>& OriginalCode = Function->Script;
    checkf(OriginalCode.Num() > HookOffset, TEXT("Invalid hook: HookOffset > Script.Num()"));

#if DEBUG_BLUEPRINT_HOOKING
    DebugDumpFunctionScriptCode(Function, HookOffset, TEXT("BeforeHook"));
#endif

    // Minimum amount of bytes required to insert unconditional jump with code offset
    const int32 MinBytesRequired = 1 + sizeof(CodeSkipSizeType);

    FKismetBytecodeDisassemblerJson Disassembler;
    int32 BytesAvailable = 0;

    // Walk over statements until we collect enough bytes for a replacement
    // (or until we consumed all statements in the function's code)
    while (BytesAvailable < MinBytesRequired && (HookOffset + BytesAvailable) < OriginalCode.Num()) {
        const int32 CurrentStatementIndex = HookOffset + BytesAvailable;
        int32 OutStatementLength;

        const bool bValid = Disassembler.GetStatementLength(Function, CurrentStatementIndex, OutStatementLength);
        checkf(bValid, TEXT("Provided hook offset is not a valid statement index: %d"), HookOffset);
        BytesAvailable += OutStatementLength;
    }

    // Check that we collected enough bytes
    if (BytesAvailable < MinBytesRequired) {
        // If we are here, it means we consumed all the statements in the function's code
        // And still don't have enough space for inserting a jump. In that case, we append additional
        // EX_EndOfScript instructions until we have enough place
        const int32 BytesToAppend = MinBytesRequired - BytesAvailable;
        OriginalCode.AddUninitialized(BytesToAppend);
        FPlatformMemory::Memset(&OriginalCode[OriginalCode.Num() - BytesToAppend], EX_EndOfScript, BytesToAppend);
        BytesAvailable = MinBytesRequired;
    }

    // Generate code required for calling our hook
    // We use EX_CallMath for speed since our inserted function doesn't need context, and is fine with being called on CDO
    TArray<uint8> AppendedCode;

    // Make sure hook function is not NULL, otherwise we may experience weird crashes later
    UFunction* HookCallFunction = UBlueprintHookManager::StaticClass()->FindFunctionByName(TEXT("ExecuteBPHook"));
    check(HookCallFunction);

    // We use EX_CallMath for speed since our inserted function doesn't need context, and is fine with being called on CDO
    // EX_CallMath requires just UFunction object pointer and argument list
    AppendedCode.Add(EX_CallMath);
    WRITE_UNALIGNED(AppendedCode, ScriptPointerType, HookCallFunction);

    // Begin writing function parameters - we have just hook offset constant
    AppendedCode.Add(EX_IntConst);
    WRITE_UNALIGNED(AppendedCode, int32, HookOffset);
    AppendedCode.Add(EX_EndFunctionParms);


    // Append original code that we replaced earlier with unconditional jump
    AppendedCode.AddUninitialized(BytesAvailable);
    FPlatformMemory::Memcpy(&AppendedCode[AppendedCode.Num() - BytesAvailable], &OriginalCode[HookOffset], BytesAvailable);

    // Insert jump to original location for running code after hook
    AppendedCode.Add(EX_Jump);
    const int32 JumpDestination = HookOffset + BytesAvailable;
    WRITE_UNALIGNED(AppendedCode, CodeSkipSizeType, JumpDestination);

    // Finish generated code with EX_EndOfScript to avoid any surprises
    AppendedCode.Add(EX_EndOfScript);


    // Append generated code to the end of the function's original code now
    const int32 StartOfAppendedCode = OriginalCode.Num();
    OriginalCode.Append(AppendedCode);

    // Fill space with EX_EndOfScript before replacement for safety
    FPlatformMemory::Memset(&OriginalCode[HookOffset], EX_EndOfScript, BytesAvailable);

    // Actually insert jump to the start of appended code to original hook location
    OriginalCode[HookOffset] = EX_Jump;
    FPlatformMemory::WriteUnaligned<CodeSkipSizeType>(&OriginalCode[HookOffset + 1], StartOfAppendedCode);

#if DEBUG_BLUEPRINT_HOOKING
    DebugDumpFunctionScriptCode(Function, HookOffset, TEXT("AfterHook"));
#endif
}

int32 UBlueprintHookManager::PreProcessHookOffset(UFunction* Function, int32 HookOffset) {
    if (HookOffset == EPredefinedHookOffset::Return) {
        // For now Kismet Compiler will always generate only one Return node, so all
        // execution paths will end up either with executing it directly or jumping to it
        // So we need to hook only in one place to handle all possible execution paths
        FKismetBytecodeDisassemblerJson Disassembler;
        int32 ReturnOffset;
        const bool bIsValid = Disassembler.FindFirstStatementOfType(Function, 0, EX_Return, ReturnOffset);
        checkf(bIsValid, TEXT("EX_Return not found for function %s"), *Function->GetPathName());
        return ReturnOffset;
    }
    return HookOffset;
}

void FFunctionHookInfo::InvokeBlueprintHook(FFrame& Frame, int32 HookOffset) {
    FBlueprintHookHelper HookHelper{Frame, ReturnStatementOffset};
    const TArray<TFunction<HookFunctionSignature>>& Hooks = this->CodeOffsetByHookList.FindChecked(HookOffset);
    for (const TFunction<HookFunctionSignature>& Hook : Hooks) {
        Hook(HookHelper);
    }
}

void FFunctionHookInfo::RecalculateReturnStatementOffset(UFunction* Function) {
    FKismetBytecodeDisassemblerJson Disassembler;
    int32 ReturnInstructionOffset;
    Disassembler.FindFirstStatementOfType(Function, 0, EX_Return, ReturnInstructionOffset);
    this->ReturnStatementOffset = ReturnInstructionOffset;
}

void UBlueprintHookManager::HookBlueprintFunction(UFunction* Function, const TFunction<HookFunctionSignature>& Hook, int32 HookOffset) {
#if !WITH_EDITOR
    checkf(Function->Script.Num(), TEXT("HookBPFunction: Function provided is not implemented in BP"));

    // Make sure to add outer UClass to root set to avoid it being Garbage Collected
    // Because otherwise after GC script byte code will be reloaded, without our hooks applied
    UClass* OuterUClass = Function->GetTypedOuter<UClass>();
    check(OuterUClass);
    HookedClasses.AddUnique(OuterUClass);

    HookOffset = PreProcessHookOffset(Function, HookOffset);

#if UE_BLUEPRINT_EVENTGRAPH_FASTCALLS
    if (Function->EventGraphFunction != nullptr) {
        UE_LOG(LogBlueprintHookManager, Warning, TEXT("Attempt to hook event graph call stub function with fast-call enabled, disabling fast call for that function"));
        UE_LOG(LogBlueprintHookManager, Warning, TEXT("It may result in performance regression for called function, if you need highest performance possible, consider hooking event graph function"));
        UE_LOG(LogBlueprintHookManager, Warning, TEXT("Event graph function: %s, From Offset: %d"), *Function->EventGraphFunction->GetPathName(), Function->EventGraphCallOffset);
        Function->EventGraphFunction = nullptr;
    }
#endif

    FFunctionHookInfo& FunctionHookInfo = HookedFunctions.FindOrAdd(Function);
    TArray<TFunction<HookFunctionSignature>>& InstalledHooks = FunctionHookInfo.CodeOffsetByHookList.FindOrAdd(HookOffset);

    if (InstalledHooks.Num() == 0) {
        // First time function is hooked at this offset, call InstallBlueprintHook
        InstallBlueprintHook(Function, HookOffset);
        // Update cached return instruction offset
        FunctionHookInfo.RecalculateReturnStatementOffset(Function);
    }
    // Add provided hook into the array
    InstalledHooks.Add(Hook);
#endif
}
