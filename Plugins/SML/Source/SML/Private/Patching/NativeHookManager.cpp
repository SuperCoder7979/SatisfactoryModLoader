#include "Patching/NativeHookManager.h"
#include "CoreMinimal.h"
#include "funchook.h"
#include "AssemblyAnalyzer.h"

DEFINE_LOG_CATEGORY(LogNativeHookManager);

// since templates are actually compiled for each module separately,
// we need to have a global handler map which will be shared by all hook invoker templates available in all modules
// to keep single hook instance for each method
static TMap<void*, void*> RegisteredListenerMap;

// Map of the function implementation pointer to the trampoline function pointer. Used to ensure one hook per function installed
static TMap<void*, void*> InstalledHookMap;

void* FNativeHookManagerInternal::GetHandlerListInternal(void* RealFunctionAddress) {
    void** ExistingMapEntry = RegisteredListenerMap.Find(RealFunctionAddress);
    return ExistingMapEntry ? *ExistingMapEntry : nullptr;
}

void FNativeHookManagerInternal::SetHandlerListInstanceInternal(void* RealFunctionAddress, void* HandlerList) {
    RegisteredListenerMap.Add(RealFunctionAddress, HandlerList);
}

#define CHECK_FUNCHOOK_ERR(arg) \
    if (arg != FUNCHOOK_ERROR_SUCCESS) UE_LOG(LogNativeHookManager, Fatal, TEXT("Hooking function %s failed: funchook failed: %hs"), *DebugSymbolName, funchook_error_message(funchook));

void LogDebugAssemblyAnalyzer(const ANSICHAR* Message) {
    UE_LOG(LogNativeHookManager, Display, TEXT("AssemblyAnalyzer Debug: %hs"), Message);
}

bool HookStandardFunction(const FString& DebugSymbolName, void* OriginalFunctionPointer, void* HookFunctionPointer, void** OutTrampolineFunction) {
    if (InstalledHookMap.Contains(OriginalFunctionPointer)) {
        // Hook already installed, set trampoline function and return
        *OutTrampolineFunction = InstalledHookMap.FindChecked(OriginalFunctionPointer);
        return false;
    }
    funchook* funchook = funchook_create();
    if (funchook == nullptr) {
        UE_LOG(LogNativeHookManager, Fatal, TEXT("Hooking function %s failed: funchook_create() returned NULL"), *DebugSymbolName);
        return false;
    }
    *OutTrampolineFunction = OriginalFunctionPointer;
    CHECK_FUNCHOOK_ERR(funchook_prepare(funchook, OutTrampolineFunction, HookFunctionPointer));
    CHECK_FUNCHOOK_ERR(funchook_install(funchook, 0));
    InstalledHookMap.Add(OriginalFunctionPointer, *OutTrampolineFunction);
    return true;
}

SML_API void* FNativeHookManagerInternal::RegisterHookFunction(const FString& DebugSymbolName, void* OriginalFunctionPointer, void* SampleObjectInstance, int ThisAdjustment, void* HookFunctionPointer, void** OutTrampolineFunction) {
    SetDebugLoggingHook(&LogDebugAssemblyAnalyzer);
    FunctionInfo FunctionInfo = DiscoverFunction((uint8*)OriginalFunctionPointer);
    checkf(FunctionInfo.bIsValid, TEXT("Attempt to hook invalid function %s: Provided code pointer %p is not valid"), *DebugSymbolName, OriginalFunctionPointer);

    if (FunctionInfo.bIsVirtualFunction) {
        checkf(SampleObjectInstance, TEXT("Attempt to hook virtual function override without providing object instance for implementation resolution"));
        UE_LOG(LogNativeHookManager, Display, TEXT("Attempting to resolve virtual function %s. This adjustment: %d, virtual function table offset: %d"), *DebugSymbolName, ThisAdjustment, FunctionInfo.VirtualTableFunctionOffset);

        // Target Function Address = (this + ThisAdjustment)->vftable[VirtualFunctionOffset]
        void* AdjustedThisPointer = ((uint8*)SampleObjectInstance) + ThisAdjustment;
        uint8** VirtualFunctionTableBase = *((uint8***)AdjustedThisPointer);
        // Offset is in bytes from the start of the virtual table, we need to convert it to pointer array index
        uint8* FunctionImplementationPointer = VirtualFunctionTableBase[FunctionInfo.VirtualTableFunctionOffset / 8];

        FunctionInfo = DiscoverFunction(FunctionImplementationPointer);

        // Perform basic checking to make sure calculation was correct, or at least seems to be so
        checkf(FunctionInfo.bIsValid, TEXT("Failed to resolve virtual function for thunk %s at %p, reuslting address contains no executable code"), *DebugSymbolName, OriginalFunctionPointer);
        checkf(!FunctionInfo.bIsVirtualFunction, TEXT("Failed to resolve virtual function for thunk %s at %p, resulting function still points to a thunk"), *DebugSymbolName, OriginalFunctionPointer);

        UE_LOG(LogNativeHookManager, Display, TEXT("Successfully resolved virtual function thunk %s at %p to function implementation at %p"), *DebugSymbolName, OriginalFunctionPointer, FunctionInfo.RealFunctionAddress);
    }

    // Log debugging information just in case
    void* ResolvedHookingFunctionPointer = FunctionInfo.RealFunctionAddress;
    UE_LOG(LogNativeHookManager, Display, TEXT("Hooking function %s: Provided address: %p, resolved address: %p"), *DebugSymbolName, OriginalFunctionPointer, ResolvedHookingFunctionPointer);

    HookStandardFunction(DebugSymbolName, ResolvedHookingFunctionPointer, HookFunctionPointer, OutTrampolineFunction);
    UE_LOG(LogNativeHookManager, Display, TEXT("Successfully hooked function %s at %p"), *DebugSymbolName, ResolvedHookingFunctionPointer);
    return ResolvedHookingFunctionPointer;
}
