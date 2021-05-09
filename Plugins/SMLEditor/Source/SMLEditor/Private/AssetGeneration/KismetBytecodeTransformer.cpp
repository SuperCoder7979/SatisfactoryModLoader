#include "AssetGeneration/KismetBytecodeTransformer.h"
#include "BPTerminal.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/LatentActionManager.h"
#include "toolkit/PropertyTypeHandler.h"

FKismetBytecodeTransformer::FKismetBytecodeTransformer(UBlueprint* Blueprint) {
    this->OwnerBlueprint = Blueprint;
    this->ExecuteUbergraphFunctionName = UEdGraphSchema_K2::FN_ExecuteUbergraphBase.ToString() + TEXT("_") + Blueprint->GetName();
}

void FKismetBytecodeTransformer::SetUberGraphTransformer(TSharedPtr<FKismetBytecodeTransformer> Transformer) {
    check(Transformer->IsUberGraphFunction());
    this->UberGraphTransformer = Transformer;
}

void FKismetBytecodeTransformer::SetSourceStatements(const FString& FunctionName, const TArray<TSharedPtr<FJsonObject>>& Statements) {
    this->CurrentFunctionName = FunctionName;
    this->ResultStatements.Reserve(Statements.Num());

    for (const TSharedPtr<FJsonObject> StatementObject : Statements) {
        const uint32 StatementIndex = StatementObject->GetIntegerField(TEXT("StatementIndex"));
        TSharedPtr<FKismetCompiledStatement> Statement = ProcessStatement(StatementObject);

        this->StatementsByOffset.Add(StatementIndex, Statement);
        this->ResultStatements.Add(Statement);
    }
}

TArray<TSharedPtr<FKismetCompiledStatement>> FKismetBytecodeTransformer::FinishGeneration() {
    // Apply patch-ups to jump statements
    for (const TPair<TSharedPtr<FKismetCompiledStatement>, int32>& Pair : JumpPatchUpTable) {
        const TSharedPtr<FKismetCompiledStatement> JumpStatement = Pair.Key;
        const TSharedPtr<FKismetCompiledStatement> JumpTarget = StatementsByOffset.FindChecked(Pair.Value);
        JumpStatement->TargetLabel = JumpTarget;

        // Replace jump statement type if we are jumping to the return statement
        if (JumpTarget->Type == ECompiledStatementType::KCST_Return) {
            // Replace KCST_UnconditionalGoto with KCST_GotoReturn
            if (JumpStatement->Type == ECompiledStatementType::KCST_UnconditionalGoto) {
                JumpStatement->Type = ECompiledStatementType::KCST_GotoReturn;
            }
            // Replace KCST_GotoIfNot with KCST_GotoReturnIfNot
            if (JumpStatement->Type == ECompiledStatementType::KCST_GotoIfNot) {
                JumpStatement->Type = ECompiledStatementType::KCST_GotoReturnIfNot;
            }
        }
    }

    return ResultStatements;
}

bool FKismetBytecodeTransformer::IsContextInstruction(const FString& InstructionName) {
    return InstructionName == TEXT("Context") ||
        InstructionName == TEXT("Context_FailSilent") ||
        InstructionName == TEXT("ClassContext");
}

bool FKismetBytecodeTransformer::IsCallFunctionInstruction(const FString& InstructionName) {
    return InstructionName == TEXT("CallMath") ||
        InstructionName == TEXT("LocalFinalFunction") ||
        InstructionName == TEXT("FinalFunction") ||
        InstructionName == TEXT("LocalVirtualFunction") ||
        InstructionName == TEXT("VirtualFunction");
}

bool FKismetBytecodeTransformer::IsVariableInstruction(const FString& InstructionName) {
    return InstructionName == TEXT("DefaultVariable") ||
        InstructionName == TEXT("InstanceVariable") ||
        InstructionName == TEXT("LocalVariable") ||
        InstructionName == TEXT("LocalOutVariable");
}

TSharedPtr<FKismetCompiledStatement> FKismetBytecodeTransformer::ProcessStatement(TSharedPtr<FJsonObject> Statement) {
    const FString InstructionName = Statement->GetStringField(TEXT("Inst"));

    if (InstructionName == TEXT("Nothing")) {
        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_Nop;
        return Result;
    }

    if (InstructionName == TEXT("SetSet")) {
        TArray<TSharedPtr<FJsonValue>> Values = Statement->GetArrayField(TEXT("Values"));
        const TSharedPtr<FJsonObject> LeftSideExpression = Statement->GetObjectField(TEXT("LeftSideExpression"));

        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_CreateSet;
        Result->LHS = ProcessExpression(LeftSideExpression);

        for (const TSharedPtr<FJsonValue>& Value : Values) {
            Result->RHS.Add(ProcessExpression(Value->AsObject()));
        }
        return Result;
    }

    if (InstructionName == TEXT("SetArray")) {
        TArray<TSharedPtr<FJsonValue>> Values = Statement->GetArrayField(TEXT("Values"));
        const TSharedPtr<FJsonObject> LeftSideExpression = Statement->GetObjectField(TEXT("LeftSideExpression"));

        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_CreateArray;
        Result->LHS = ProcessExpression(LeftSideExpression);

        for (const TSharedPtr<FJsonValue>& Value : Values) {
            Result->RHS.Add(ProcessExpression(Value->AsObject()));
        }
        return Result;
    }

    if (InstructionName == TEXT("SetMap")) {
        TArray<TSharedPtr<FJsonValue>> Values = Statement->GetArrayField(TEXT("Values"));
        const TSharedPtr<FJsonObject> LeftSideExpression = Statement->GetObjectField(TEXT("LeftSideExpression"));

        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_CreateMap;
        Result->LHS = ProcessExpression(LeftSideExpression);

        // Maps are represented by (key, value) terms being followed by each other
        for (const TSharedPtr<FJsonValue>& Value : Values) {
            const TSharedPtr<FJsonObject> PairObject = Value->AsObject();
            Result->RHS.Add(ProcessExpression(PairObject->GetObjectField(TEXT("Key"))));
            Result->RHS.Add(ProcessExpression(PairObject->GetObjectField(TEXT("Value"))));
        }
        return Result;
    }

    // All of the assignment instructions are similar, it's just that they are type-specific for runtime to have easier time handling them
    if (InstructionName == TEXT("Let") || InstructionName == TEXT("LetObj") ||
        InstructionName == TEXT("LetWeakObjPtr") || InstructionName == TEXT("LetBool") ||
        InstructionName == TEXT("LetMulticastDelegate") || InstructionName == TEXT("LetDelegate")) {

        const TSharedPtr<FJsonObject> Variable = Statement->GetObjectField(TEXT("Variable"));
        const TSharedPtr<FJsonObject> Expression = Statement->GetObjectField(TEXT("Expression"));
        const FString ExpressionInstName = Expression->GetStringField(TEXT("Inst"));

        // Function calls cannot be expressions in compiled statement representation, so we need to handle case of RHS being CallFunction here
        // They often happen to be prefixed with a context expression (except math call, which doesn't need context)
        // So we need to check for both context expression and call function instruction
        // ProcessFunctionCallStatement will handle context expression itself just fine too, so just call it regardless
        if (IsCallFunctionInstruction(ExpressionInstName) || IsContextInstruction(ExpressionInstName)) {
            TSharedPtr<FKismetCompiledStatement> FunctionCall = ProcessFunctionCallStatement(Expression);

            // Set LHS so that function call result will be saved into the variable
            FunctionCall->LHS = ProcessExpression(Variable);
            return FunctionCall;
        }

        // Casts are compiled down to variable assignments, so we need to handle them there

        // These two cases below handle FScriptInterface <-> UObject pointer conversions primarily
        if (ExpressionInstName == TEXT("ObjToInterfaceCast") || ExpressionInstName == TEXT("CrossInterfaceCast")) {
            TSharedPtr<FKismetCompiledStatement> InterfaceCast = MakeShareable(new FKismetCompiledStatement());

            // Wraps UObject pointer into the FScriptInterface with provided interface class
            if (ExpressionInstName == TEXT("ObjToInterfaceCast")) {
                InterfaceCast->Type = ECompiledStatementType::KCST_CastObjToInterface;
                // Converts FScriptInterface pointer to one interface into another at the same object
            } else if (ExpressionInstName == TEXT("CrossInterfaceCast")) {
                InterfaceCast->Type = ECompiledStatementType::KCST_CrossInterfaceCast;
            } else {
                checkf(0, TEXT("Unknown interface cast instruction %s"), *ExpressionInstName);
            }

            const FString InterfaceClassPath = Expression->GetStringField(TEXT("InterfaceClass"));
            const TSharedPtr<FJsonObject> InnerExpression = Expression->GetObjectField(TEXT("Expression"));

            UClass* InterfaceClass = LoadObject<UClass>(NULL, *InterfaceClassPath);
            check(InterfaceClass && InterfaceClass->IsChildOf<UInterface>());

            TSharedPtr<FKismetTerminal> ClassLiteral = MakeShareable(new FKismetTerminal());

            ClassLiteral->bIsLiteral = true;
            ClassLiteral->Type.PinCategory = UEdGraphSchema_K2::PC_Class;
            ClassLiteral->Type.PinSubCategoryObject = InterfaceClass;
            ClassLiteral->ObjectLiteral = InterfaceClass;

            InterfaceCast->LHS = ProcessExpression(Variable);
            InterfaceCast->RHS.Add(ClassLiteral);
            InterfaceCast->RHS.Add(ProcessExpression(InnerExpression));
            return InterfaceCast;
        }

        // Converts FScriptInterface object to raw UObject pointer
        if (ExpressionInstName == TEXT("InterfaceToObjCast")) {
            TSharedPtr<FKismetCompiledStatement> InterfaceToObjCast = MakeShareable(new FKismetCompiledStatement());
            InterfaceToObjCast->Type = ECompiledStatementType::KCST_CastInterfaceToObj;

            const FString ObjectClassPath = Expression->GetStringField(TEXT("ObjectClass"));
            const TSharedPtr<FJsonObject> InnerExpression = Expression->GetObjectField(TEXT("Expression"));

            UClass* ObjectClass = LoadObject<UClass>(NULL, *ObjectClassPath);
            check(ObjectClass);

            TSharedPtr<FKismetTerminal> ClassLiteral = MakeShareable(new FKismetTerminal());
            ClassLiteral->bIsLiteral = true;
            ClassLiteral->Type.PinCategory = UEdGraphSchema_K2::PC_Class;
            ClassLiteral->Type.PinSubCategoryObject = ObjectClass;
            ClassLiteral->ObjectLiteral = ObjectClass;

            InterfaceToObjCast->LHS = ProcessExpression(Variable);
            InterfaceToObjCast->RHS.Add(ClassLiteral);
            InterfaceToObjCast->RHS.Add(ProcessExpression(InnerExpression));
            return InterfaceToObjCast;
        }

        // These casts handle conversion between UObject pointer types and do not invole interfaces
        if (ExpressionInstName == TEXT("DynamicCast") || ExpressionInstName == TEXT("MetaCast")) {
            TSharedPtr<FKismetCompiledStatement> CastStatement = MakeShareable(new FKismetCompiledStatement());

            // DynamicCast handles conversions between UObject types (except UClasses)
            if (ExpressionInstName == TEXT("DynamicCast")) {
                CastStatement->Type = ECompiledStatementType::KCST_DynamicCast;
                // MetaCast handles conversion between UClass types (actually it only does class type checking)
            } else if (ExpressionInstName == TEXT("MetaCast")) {
                CastStatement->Type = ECompiledStatementType::KCST_MetaCast;
            } else {
                checkf(0, TEXT("Unsupported cast instruction: %s"), *ExpressionInstName);
            }

            const FString DestinationClassPath = Expression->GetStringField(TEXT("Class"));
            const TSharedPtr<FJsonObject> InnerExpression = Expression->GetObjectField(TEXT("Expression"));

            UClass* DestinationClass = LoadObject<UClass>(NULL, *DestinationClassPath);
            check(DestinationClass);

            TSharedPtr<FKismetTerminal> ClassLiteral = MakeShareable(new FKismetTerminal());
            ClassLiteral->bIsLiteral = true;
            ClassLiteral->Type.PinCategory = UEdGraphSchema_K2::PC_Class;
            ClassLiteral->Type.PinSubCategoryObject = DestinationClass;
            ClassLiteral->ObjectLiteral = DestinationClass;

            CastStatement->LHS = ProcessExpression(Variable);
            CastStatement->RHS.Add(ClassLiteral);
            CastStatement->RHS.Add(ProcessExpression(InnerExpression));
            return CastStatement;
        }

        // PrimitiveCast basically convert UObject pointers and FScriptInterface objects into boolean
        if (ExpressionInstName == TEXT("PrimitiveCast")) {
            TSharedPtr<FKismetCompiledStatement> PrimitiveCast = MakeShareable(new FKismetCompiledStatement());
            PrimitiveCast->Type = ECompiledStatementType::KCST_ObjectToBool;

            const TSharedPtr<FJsonObject> InnerExpression = Expression->GetObjectField(TEXT("Expression"));
            const FString CastType = Expression->GetStringField(TEXT("CastType"));
            // We do not support legacy CST_ObjectToInterface, because it is never generated by VM backend anymore
            check(CastType == TEXT("InterfaceToBool") || CastType == TEXT("ObjectToBool"));

            PrimitiveCast->LHS = ProcessExpression(Variable);
            PrimitiveCast->RHS.Add(ProcessExpression(InnerExpression));
            return PrimitiveCast;
        }

        // Otherwise it is a normal assignment operation, so right side should be a simple expression
        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_Assignment;
        Result->LHS = ProcessExpression(Variable);
        Result->RHS.Add(ProcessExpression(Expression));
        return Result;
    }

    if (InstructionName == TEXT("LetValueOnPersistentFrame")) {
        // It is variable assignment on a persistent frame, just like a normal assignment, but dest property is a bit special
        const TSharedPtr<FJsonObject> Expression = Statement->GetObjectField(TEXT("Expression"));
        const TSharedPtr<FJsonObject> PropertyType = Statement->GetObjectField(TEXT("PropertyType"));
        const FString PropertyName = Statement->GetStringField(TEXT("PropertyName"));

        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_AssignmentOnPersistentFrame;

        // Deserialize type from instruction data directly, because figuring out persistent frame layout is hard
        // from this point by variable name inside it. It's much easier just to read pre-recorded information
        Result->LHS = MakeShareable(new FKismetTerminal());
        Result->LHS->Type = FPropertyTypeHelper::DeserializeGraphPinType(PropertyType.ToSharedRef(), OwnerBlueprint->SkeletonGeneratedClass);
        Result->LHS->AssociatedVarProperty = PropertyName;

        Result->RHS.Add(ProcessExpression(Expression));
        return Result;
    }

    // This is a function call statement, possibly prefixed with context expressions
    // Just call process function call, it will handle the case when actual statement passed
    // is a context and not the function call node itself gracefully
    if (IsCallFunctionInstruction(InstructionName) || IsContextInstruction(InstructionName)) {
        return ProcessFunctionCallStatement(Statement);
    }

    // EX_CallMulticastDelegate will never be prefixed with context,
    // because it always accepts context as an separate argument and does not need it passed implicitly
    if (InstructionName == TEXT("CallMulticastDelegate")) {
        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_CallDelegate;
        Result->FunctionContext = ProcessExpression(Statement->GetObjectField(TEXT("Delegate")));

        const TSharedPtr<FJsonObject> DelegateSignatureFunctionRef = Statement->GetObjectField(TEXT("DelegateSignatureFunction"));

        const bool bIsSelfContext = DelegateSignatureFunctionRef->GetBoolField(TEXT("IsSelfContext"));
        const FString MemberName = DelegateSignatureFunctionRef->GetStringField(TEXT("MemberName"));

        UClass* MemberParentClass;

        // Resolve MemberParent class in respect to Self Context
        if (!bIsSelfContext) {
            const FString MemberParentPath = DelegateSignatureFunctionRef->GetStringField(TEXT("MemberParent"));
            MemberParentClass = LoadObject<UClass>(NULL, *MemberParentPath);
            checkf(MemberParentClass, TEXT("Couldn't resolve delegate signature parent class %s"), *MemberParentPath);

        } else {
            // In self context, we should use skeleton blueprint class as a member parent
            MemberParentClass = OwnerBlueprint->SkeletonGeneratedClass;
        }

        UFunction* Function = MemberParentClass->FindFunctionByName(*MemberName);
        checkf(Function, TEXT("Cannot resolve delegate signature member %s in parent scope %s"), *MemberName, *MemberParentClass->GetPathName());
        checkf(Function->HasAnyFunctionFlags(FUNC_Delegate), TEXT("Delegate signature function %s is not marked as delegate"), *MemberName);

        Result->FunctionToCall = Function;

        // Do argument processing that is common for all function call instructions
        TArray<TSharedPtr<FJsonValue>> Parameters = Statement->GetArrayField(TEXT("Parameters"));

        int32 NumParams = 0;
        for (TFieldIterator<UProperty> PropIt(Result->FunctionToCall); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt) {
            UProperty* FuncParamProperty = *PropIt;

            // Skip return values because they are handled in a special way when we happened to be an expression in EX_Let statement
            if (!FuncParamProperty->HasAnyPropertyFlags(CPF_ReturnParm)) {
                const TSharedPtr<FJsonObject> ParameterExpression = Parameters[NumParams++]->AsObject();
                TSharedPtr<FKismetTerminal> ParameterTerminal = ProcessFunctionParameter(ParameterExpression);

                Result->RHS.Add(ParameterTerminal);
            }
        }
        return Result;
    }

    // This is a computed jump statement, offset expression should be stored in the LHS
    if (InstructionName == TEXT("ComputedJump")) {
        const TSharedPtr<FJsonObject> Expression = Statement->GetObjectField(TEXT("Expression"));

        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_ComputedGoto;
        Result->LHS = ProcessExpression(Expression);
        return Result;
    }

    // This is an unconditional jump, offset is stored in the TargetLabel
    // But we cannot really resolve it until we finished processing all statements, so store it in the patch-up map
    if (InstructionName == TEXT("Jump")) {
        const int32 JumpOffset = Statement->GetIntegerField(TEXT("Offset"));

        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_UnconditionalGoto;
        JumpPatchUpTable.Add(Result, JumpOffset);
        return Result;
    }

    if (InstructionName == TEXT("JumpIfNot")) {
        const TSharedPtr<FJsonObject> Condition = Statement->GetObjectField(TEXT("Condition"));
        const int32 JumpOffset = Statement->GetIntegerField(TEXT("Offset"));

        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_GotoIfNot;
        Result->LHS = ProcessExpression(Condition);
        JumpPatchUpTable.Add(Result, JumpOffset);
        return Result;
    }

    // Adds the location at which code will jump once PopExecutionFlow is encountered
    if (InstructionName == TEXT("PushExecutionFlow")) {
        const int32 JumpOffset = Statement->GetIntegerField(TEXT("Offset"));

        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_PushState;
        JumpPatchUpTable.Add(Result, JumpOffset);
        return Result;
    }

    // Pops execution flow to the last stored location (or to return statement) if condition is zero
    if (InstructionName == TEXT("PopExecutionFlowIfNot")) {
        const TSharedPtr<FJsonObject> Condition = Statement->GetObjectField(TEXT("Condition"));

        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_EndOfThreadIfNot;
        Result->LHS = ProcessExpression(Condition);
        return Result;
    }

    // Pops execution flow to the last stored location (or to return statement) unconditionally
    if (InstructionName == TEXT("PopExecutionFlow")) {
        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_EndOfThread;
        return Result;
    }

    // Registers a delegate object into the multicast delegate subscription list
    if (InstructionName == TEXT("AddMulticastDelegate")) {
        const TSharedPtr<FJsonObject> MulticastDelegate = Statement->GetObjectField(TEXT("MulticastDelegate"));
        const TSharedPtr<FJsonObject> Delegate = Statement->GetObjectField(TEXT("Delegate"));

        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_AddMulticastDelegate;
        Result->LHS = ProcessExpression(MulticastDelegate);
        Result->RHS.Add(ProcessExpression(Delegate));
        return Result;
    }

    // Removes a single delegate object out of multicast delegate subscription list
    if (InstructionName == TEXT("RemoveMulticastDelegate")) {
        const TSharedPtr<FJsonObject> MulticastDelegate = Statement->GetObjectField(TEXT("MulticastDelegate"));
        const TSharedPtr<FJsonObject> Delegate = Statement->GetObjectField(TEXT("Delegate"));

        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_RemoveMulticastDelegate;
        Result->LHS = ProcessExpression(MulticastDelegate);
        Result->RHS.Add(ProcessExpression(Delegate));
        return Result;
    }

    // Clears a multicast delegate subscription list
    if (InstructionName == TEXT("ClearMulticastDelegate")) {
        const TSharedPtr<FJsonObject> MulticastDelegate = Statement->GetObjectField(TEXT("MulticastDelegate"));

        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_ClearMulticastDelegate;
        Result->LHS = ProcessExpression(MulticastDelegate);
        return Result;
    }

    // Populates delegate object expression with a reference to the function of the object passed
    if (InstructionName == TEXT("BindDelegate")) {
        const TSharedPtr<FJsonObject> Delegate = Statement->GetObjectField(TEXT("Delegate"));
        const TSharedPtr<FJsonObject> Object = Statement->GetObjectField(TEXT("Object"));
        const FString FunctionName = Statement->GetStringField(TEXT("FunctionName"));

        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_BindDelegate;
        Result->LHS = ProcessExpression(Delegate);

        // Create string literal with function name and add it as the first argument
        TSharedPtr<FKismetTerminal> FunctionNameTerminal = MakeShareable(new FKismetTerminal());
        FunctionNameTerminal->bIsLiteral = true;
        FunctionNameTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Name;
        FunctionNameTerminal->StringLiteral = FunctionName;
        Result->RHS.Add(FunctionNameTerminal);

        Result->RHS.Add(ProcessExpression(Object));
        return Result;
    }

    // Returns from the currently running function, optionally passing back returned value (or nothing)
    if (InstructionName == TEXT("Return")) {
        TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
        Result->Type = ECompiledStatementType::KCST_Return;
        return Result;
    }

    checkf(0, TEXT("Cannot transform instruction into compiled statement: %s"), *InstructionName);
    return NULL;
}

TSharedPtr<FKismetTerminal> FKismetBytecodeTransformer::ProcessExpression(TSharedPtr<FJsonObject> Expression) {
    const FString InstructionName = Expression->GetStringField(TEXT("Inst"));

    // InlineGeneratedParameter will always be NULL for terminals returned by ProcessExpression,
    // since we just don't have enough context to process statements here, and
    // it can only be encountered inside of the CallFunction statements - so we handle it there

    // EX_StructMemberContext retrieves value of struct property from struct value passed as context
    if (InstructionName == TEXT("StructMemberContext")) {
        const FString PropertyName = Expression->GetStringField(TEXT("PropertyName"));
        const TSharedPtr<FJsonObject> PropertyType = Expression->GetObjectField(TEXT("PropertyType"));
        const TSharedPtr<FJsonObject> ContextExpression = Expression->GetObjectField(TEXT("StructExpression"));

        const TSharedPtr<FKismetTerminal> ContextTerminal = ProcessExpression(ContextExpression);
        ContextTerminal->ContextType = FKismetTerminal::EContextType_Struct;

        TSharedPtr<FKismetTerminal> StructMemberTerminal = MakeShareable(new FKismetTerminal());
        StructMemberTerminal->Type = FPropertyTypeHelper::DeserializeGraphPinType(PropertyType.ToSharedRef(), OwnerBlueprint->SkeletonGeneratedClass);
        StructMemberTerminal->AssociatedVarProperty = PropertyName;

        StructMemberTerminal->Context = ContextTerminal;

        return StructMemberTerminal;
    }

    // EX_Context, EX_Context_FailSilent and EX_ClassContext are all indicate nested expression run inside of the context
    if (IsContextInstruction(InstructionName)) {
        const bool bIsClassContext = InstructionName == TEXT("ClassContext");
        const TSharedPtr<FJsonObject> Context = Expression->GetObjectField(TEXT("Context"));
        const TSharedPtr<FJsonObject> InnerExpression = Expression->GetObjectField(TEXT("Expression"));

        TSharedPtr<FKismetTerminal> ContextTerminal = ProcessExpression(Context);
        ContextTerminal->ContextType = bIsClassContext ? FKismetTerminal::EContextType_Class : FKismetTerminal::EContextType_Object;

        TSharedPtr<FKismetTerminal> InnerExpressionTerminal = ProcessExpression(InnerExpression);
        InnerExpressionTerminal->Context = ContextTerminal;
        return InnerExpressionTerminal;
    }

    // EX_LocalVariable, EX_LocalOutVariable, EX_InstanceVariable and EX_DefaultVariable are all simple terminals with different VarType values
    if (IsVariableInstruction(InstructionName)) {
        const FString VariableName = Expression->GetStringField(TEXT("VariableName"));
        const TSharedPtr<FJsonObject> VariableType = Expression->GetObjectField(TEXT("VariableType"));

        // We can resolve type of variable by simply looking up through properties associated with UFunction, but it's much easier to read pre-recorded information
        TSharedPtr<FKismetTerminal> VariableTerminal = MakeShareable(new FKismetTerminal());
        VariableTerminal->Type = FPropertyTypeHelper::DeserializeGraphPinType(VariableType.ToSharedRef(), OwnerBlueprint->SkeletonGeneratedClass);
        VariableTerminal->AssociatedVarProperty = VariableName;

        if (InstructionName == TEXT("DefaultVariable")) {
            VariableTerminal->VarType = FKismetTerminal::EVarType_Default;
        } else if (InstructionName == TEXT("InstanceVariable")) {
            VariableTerminal->VarType = FKismetTerminal::EVarType_Instanced;
        } else if (InstructionName == TEXT("LocalVariable") || InstructionName == TEXT("LocalOutVariable")) {
            // LocalOutVariable will be generated if Property in question has CPF_Out flag and is an out property
            VariableTerminal->VarType = FKismetTerminal::EVarType_Local;
        } else {
            checkf(0, TEXT("Unhandled variable instruction: %s"), *InstructionName);
        }
        return VariableTerminal;
    }

    // Should be a literal now
    return ProcessLiteralExpression(Expression, false);
}

TSharedPtr<FKismetTerminal> FKismetBytecodeTransformer::ProcessLiteralExpression(TSharedPtr<FJsonObject> Expression, bool bIsDelimited) {
    const FString InstructionName = Expression->GetStringField(TEXT("Inst"));

    TSharedPtr<FKismetTerminal> LiteralTerminal = MakeShareable(new FKismetTerminal());
    LiteralTerminal->bIsLiteral = true;

    // Basically represents the current Context UObject and allows retrieval of it
    // We record just as the type meta information, actual string and object literals will be empty
    if (InstructionName == TEXT("Self")) {
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Object;
        LiteralTerminal->Type.PinSubCategory = UEdGraphSchema_K2::PSC_Self;
        return LiteralTerminal;
    }

    // Text literals, pretty complicated in serialization because of existence of multiple types
    if (InstructionName == TEXT("TextConst")) {
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Text;
        const FString LiteralType = Expression->GetStringField(TEXT("TextLiteralType"));

        if (LiteralType == TEXT("Empty")) {
            LiteralTerminal->TextLiteral = FText::GetEmpty();

        } else if (LiteralType == TEXT("LocalizedText")) {
            const FString SourceString = Expression->GetStringField(TEXT("SourceString"));
            const FString KeyString = Expression->GetStringField(TEXT("LocalizationKey"));
            const FString Namespace = Expression->GetStringField(TEXT("LocalizationNamespace"));
            const FText Text = FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*SourceString, *Namespace, *KeyString);
            LiteralTerminal->TextLiteral = Text;

        } else if (LiteralType == TEXT("InvariantText")) {
            const FString SourceString = Expression->GetStringField(TEXT("SourceString"));
            LiteralTerminal->TextLiteral = FText::AsCultureInvariant(SourceString);

        } else if (LiteralType == TEXT("LiteralString")) {
            const FString SourceString = Expression->GetStringField(TEXT("SourceString"));
            LiteralTerminal->TextLiteral = FText::FromString(SourceString);

        } else if (LiteralType == TEXT("StringTableEntry")) {
            const FString TableId = Expression->GetStringField(TEXT("TableId"));
            const FString TableKey = Expression->GetStringField(TEXT("TableKey"));
            LiteralTerminal->TextLiteral = FText::FromStringTable(*TableId, *TableKey);
        }

        // Also serialize text in UTextProperty format and fill StringLiteral, just to support uniform ImportText calls on FKismetTerminal objects
        FString StringLiteralValue;
        FTextStringHelper::WriteToBuffer(StringLiteralValue, LiteralTerminal->TextLiteral, bIsDelimited);
        LiteralTerminal->StringLiteral = StringLiteralValue;

        return LiteralTerminal;
    }

    // String Literal encoded as ASCII, containing only characters of ASCII range
    if (InstructionName == TEXT("StringConst")) {
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_String;
        LiteralTerminal->StringLiteral = Expression->GetStringField(TEXT("Value"));

        if (bIsDelimited) {
            LiteralTerminal->StringLiteral = FString::Printf(TEXT("\"%s\""), *(LiteralTerminal->StringLiteral.ReplaceCharWithEscapedChar()));
        }
        return LiteralTerminal;
    }

    // String Literal containing unicode characters
    if (InstructionName == TEXT("UnicodeStringConst")) {
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_String;
        LiteralTerminal->StringLiteral = Expression->GetStringField(TEXT("Value"));

        if (bIsDelimited) {
            LiteralTerminal->StringLiteral = FString::Printf(TEXT("\"%s\""), *(LiteralTerminal->StringLiteral.ReplaceCharWithEscapedChar()));
        }
        return LiteralTerminal;
    }

    // Floating point number constant (in terminal form, it is stored in string literal field)
    if (InstructionName == TEXT("FloatConst")) {
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Float;
        LiteralTerminal->StringLiteral = FString::Printf(TEXT("%f"), Expression->GetNumberField(TEXT("Value")));
        return LiteralTerminal;
    }

    // Integer number constant (in terminal form, it is stored in string literal field)
    if (InstructionName == TEXT("IntConst")) {
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Int;
        LiteralTerminal->StringLiteral = FString::FromInt(Expression->GetIntegerField(TEXT("Value")));
        return LiteralTerminal;
    }

    // 64-bit integer number constant, recorded as string in JSON because double cannot represent entire value range on 64-bit integer
    if (InstructionName == TEXT("Int64Const")) {
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Int64;
        LiteralTerminal->StringLiteral = Expression->GetStringField(TEXT("Value"));
        return LiteralTerminal;
    }

    // Unsigned 64-bit integer number constant, never emitted because there is no Kismet graph type for it
    // No way to represent UInt64 in Kismet pin type hierarchy, so record it as int64 which will be converted back if needed
    if (InstructionName == TEXT("UInt64Const")) {
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Int64;
        LiteralTerminal->StringLiteral = Expression->GetStringField(TEXT("Value"));
        return LiteralTerminal;
    }

    // TODO Enums in Kismet are represented as byte constants, since Kismet only supports byte enums
    // TODO we just don't have information to reconstruct original kismet type there, so we record it as byte
    if (InstructionName == TEXT("ByteConst")) {
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Byte;
        LiteralTerminal->StringLiteral = FString::FromInt(Expression->GetIntegerField(TEXT("Value")));
        return LiteralTerminal;
    }

    // Convert boolean constant instructions EX_False and EX_True
    if (InstructionName == TEXT("False")) {
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        LiteralTerminal->StringLiteral = TEXT("0");
        return LiteralTerminal;
    }
    if (InstructionName == TEXT("True")) {
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        LiteralTerminal->StringLiteral = TEXT("1");
        return LiteralTerminal;
    }

    // Name constant, mostly similar to string literal but represents an FName object instead
    if (InstructionName == TEXT("NameConst")) {
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Name;
        LiteralTerminal->StringLiteral = Expression->GetStringField(TEXT("Value"));

        if (bIsDelimited) {
            LiteralTerminal->StringLiteral = FString::Printf(TEXT("\"%s\""), *LiteralTerminal->StringLiteral.ReplaceCharWithEscapedChar());
        }
        return LiteralTerminal;
    }

    // Vector constant, basically a specialized version of StructConst with the same format
    if (InstructionName == TEXT("VectorConst")) {
        FVector VectorConst(Expression->GetNumberField(TEXT("X")), Expression->GetNumberField(TEXT("Y")), Expression->GetNumberField(TEXT("Z")));
        UScriptStruct* VectorStruct = TBaseStructure<FVector>::Get();

        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Struct;
        LiteralTerminal->Type.PinSubCategoryObject = VectorStruct;
        int32 PortFlags = bIsDelimited ? PPF_Delimited : PPF_None;
        VectorStruct->ExportText(LiteralTerminal->StringLiteral, &VectorConst, NULL, NULL, PortFlags, NULL);
        return LiteralTerminal;
    }

    // Rotation constant, basically a specialized version of StructConst with the same format
    if (InstructionName == TEXT("RotationConst")) {
        FRotator RotatorConst(Expression->GetNumberField(TEXT("Pitch")), Expression->GetNumberField(TEXT("Yaw")), Expression->GetNumberField(TEXT("Roll")));
        UScriptStruct* RotatorStruct = TBaseStructure<FRotator>::Get();

        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Struct;
        LiteralTerminal->Type.PinSubCategoryObject = RotatorStruct;
        int32 PortFlags = bIsDelimited ? PPF_Delimited : PPF_None;
        RotatorStruct->ExportText(LiteralTerminal->StringLiteral, &RotatorConst, NULL, NULL, PortFlags, NULL);
        return LiteralTerminal;
    }

    // Transform constant, a combination of rotation, translation and scale components
    if (InstructionName == TEXT("TransformConst")) {
        const TSharedPtr<FJsonObject> RotationObj = Expression->GetObjectField(TEXT("Rotation"));
        FQuat Rotation = FQuat(RotationObj->GetNumberField(TEXT("X")), RotationObj->GetNumberField(TEXT("Y")), RotationObj->GetNumberField(TEXT("Z")), RotationObj->GetNumberField(TEXT("W")));

        const TSharedPtr<FJsonObject> TranslationObj = Expression->GetObjectField(TEXT("Translation"));
        FVector Translation = FVector(TranslationObj->GetNumberField(TEXT("X")), TranslationObj->GetNumberField(TEXT("Y")), TranslationObj->GetNumberField(TEXT("Z")));

        const TSharedPtr<FJsonObject> ScaleObj = Expression->GetObjectField(TEXT("Scale"));
        FVector Scale = FVector(ScaleObj->GetNumberField(TEXT("X")), ScaleObj->GetNumberField(TEXT("Y")), ScaleObj->GetNumberField(TEXT("Z")));

        FTransform ResultTransform(Rotation, Translation, Scale);
        UScriptStruct* TransformStruct = TBaseStructure<FTransform>::Get();

        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Struct;
        LiteralTerminal->Type.PinSubCategoryObject = TransformStruct;
        int32 PortFlags = bIsDelimited ? PPF_Delimited : PPF_None;
        TransformStruct->ExportText(LiteralTerminal->StringLiteral, &ResultTransform, NULL, NULL, PortFlags, NULL);
        return LiteralTerminal;
    }

    // Struct constant, needs to be serialized as string
    if (InstructionName == TEXT("StructConst")) {
        const FString StructPathName = Expression->GetStringField(TEXT("Struct"));
        TSharedPtr<FJsonObject> Properties = Expression->GetObjectField(TEXT("Properties"));

        UScriptStruct* ScriptStruct = LoadObject<UScriptStruct>(NULL, *StructPathName);
        checkf(ScriptStruct, TEXT("Cannot resolve struct %s"), *StructPathName);

        // Create temporary struct to hold deserialized values before serialization
        void* AllocatedStructInstance = FMemory::Malloc(ScriptStruct->GetPropertiesSize());
        ScriptStruct->InitializeStruct(AllocatedStructInstance);

        // Deserialize properties recorded in the struct constant by evaluating expressions
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Properties->Values) {
            const TArray<TSharedPtr<FJsonValue>> PropertyValueArray = Pair.Value->AsArray();
            const FString& PropertyName = Pair.Key;
            UProperty* Property = ScriptStruct->FindPropertyByName(*PropertyName);
            checkf(Property, TEXT("Cannot resolve struct property %s in %s"), *PropertyName, *ScriptStruct->GetPathName());

            for (int32 ArrayIter = 0; ArrayIter < Property->ArrayDim; ++ArrayIter) {
                const TSharedPtr<FJsonObject> PropertyValueExpression = PropertyValueArray[ArrayIter]->AsObject();
                const TSharedPtr<FKismetTerminal> PropertyValue = ProcessLiteralExpression(PropertyValueExpression, false);

                // Thing is, all of the constant values are serialized in the way compatible in ImportText/ExportText
                // methods implementation on common property types. So we can just call ImportText with StringLiteral
                // and handle other types of constants manually
                if (UTextProperty* TextProperty = Cast<UTextProperty>(Property)) {
                    const FText& TextPropertyValue = PropertyValue->TextLiteral;
                    TextProperty->SetPropertyValue_InContainer(AllocatedStructInstance, TextPropertyValue, ArrayIter);

                } else if (UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property)) {
                    UObject* ObjectPropertyValue = PropertyValue->ObjectLiteral;
                    ObjectProperty->SetPropertyValue_InContainer(AllocatedStructInstance, ObjectPropertyValue, ArrayIter);
                } else {
                    const TCHAR* LiteralPropertyValue = *PropertyValue->StringLiteral;
                    void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(AllocatedStructInstance, ArrayIter);
                    Property->ImportText(LiteralPropertyValue, PropertyValuePtr, PPF_None, NULL);
                }
            }
        }

        // Call destructor or struct we allocated earlier and free memory
        FString ExportedStructureText;
        int32 PortFlags = bIsDelimited ? PPF_Delimited : PPF_None;
        ScriptStruct->ExportText(ExportedStructureText, AllocatedStructInstance, NULL, NULL, PortFlags, NULL);
        ScriptStruct->DestroyStruct(AllocatedStructInstance);
        FMemory::Free(AllocatedStructInstance);

        // Serialize struct back as text and set correct kismet type
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Struct;
        LiteralTerminal->Type.PinSubCategoryObject = ScriptStruct;
        LiteralTerminal->StringLiteral = ExportedStructureText;
        return LiteralTerminal;
    }

    // Delegate property, only possible constant value of delegate property is function bound to self
    // TODO we don't have enough information to determine type of delegate here, primarily because context is not available by the time we are called
    // TODO but even with context, best thing we could do is obtain function object for bound function, which does not automatically let us obtain reference to
    // TODO delegate signature function. Best thing we could do is correct the type when assigning delegate to variables/using it during function calls
    if (InstructionName == TEXT("InstanceDelegate")) {
        const FString FunctionName = Expression->GetStringField(TEXT("FunctionName"));

        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Delegate;
        LiteralTerminal->StringLiteral = FunctionName;
        return LiteralTerminal;
    }

    // Soft object constant. Expression will always be a string literal path to the object in question
    if (InstructionName == TEXT("SoftObjectConst")) {
        TSharedPtr<FJsonObject> ObjectPathExpression = Expression->GetObjectField(TEXT("Value"));
        TSharedPtr<FKismetTerminal> ObjectPathLiteral = ProcessLiteralExpression(ObjectPathExpression, false);
        check(ObjectPathLiteral->Type.PinCategory == UEdGraphSchema_K2::PC_String);
        const FString SoftObjectPath = ObjectPathLiteral->StringLiteral;

        // Attempt to set PinSubCategoryObject by loading asset and figuring out it's class
        UClass* LoadedObjectClass = UObject::StaticClass();
        if (!SoftObjectPath.IsEmpty()) {
            UObject* LoadedObject = LoadObject<UObject>(NULL, *SoftObjectPath);
            if (LoadedObject) {
                LoadedObjectClass = LoadedObject->GetClass();
            }
        }

        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
        LiteralTerminal->Type.PinSubCategoryObject = LoadedObjectClass;
        LiteralTerminal->StringLiteral = ObjectPathLiteral->StringLiteral;
        return LiteralTerminal;
    }

    // EX_NoObject basically indicates NULL UObject pointer constant. We fill PinSubCategoryObject with UObject::StaticClass()
    // because we cannot really deduce type of generic NULL value without a context
    if (InstructionName == TEXT("NoObject")) {
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Object;
        LiteralTerminal->Type.PinSubCategoryObject = UObject::StaticClass();
        LiteralTerminal->ObjectLiteral = NULL;
        LiteralTerminal->StringLiteral = TEXT("None");
        return LiteralTerminal;
    }

    // Object literal, points to the existing UObject either inside or outside of this asset
    if (InstructionName == TEXT("ObjectConst")) {
        const FString ObjectPath = Expression->GetStringField(TEXT("Object"));

        // Resolve referenced asset object
        UObject* ReferencedObject = LoadObject<UObject>(NULL, *ObjectPath);
        checkf(ReferencedObject, TEXT("Failed to load UObject referenced by Kismet bytecode: %s"), *ObjectPath);

        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Object;
        LiteralTerminal->Type.PinSubCategoryObject = ReferencedObject->GetClass();
        LiteralTerminal->ObjectLiteral = ReferencedObject;

        // Export object both as object literal and as text, needed for array serialization
        // Code below is copied from UObjectPropertyBase::ExportText, because provided literal will be used with ImportText later
        FString ObjectExportName = ReferencedObject->GetPathName();
        if (bIsDelimited) {
            ObjectExportName = FString::Printf(TEXT("\"%s\""), *ObjectExportName.ReplaceQuotesWithEscapedQuotes());
        }

        const FString ObjectTextPathName = FString::Printf(TEXT("%s'%s'"), *ReferencedObject->GetClass()->GetName(), *ObjectExportName);
        LiteralTerminal->StringLiteral = ObjectTextPathName;

        return LiteralTerminal;
    }

    // FScriptInterface literal, currently only NULL and Self objects can be represented as interface literals
    // TODO interface pin type is incomplete, because we cannot infer interface class type from NULL value
    if (InstructionName == TEXT("NoInterface")) {
        LiteralTerminal->Type.PinCategory = UEdGraphSchema_K2::PC_Interface;
        LiteralTerminal->ObjectLiteral = NULL;
        LiteralTerminal->StringLiteral = TEXT("None");
    }

    // Set and array constants are serialized in exactly the same way, by  using (X, Y, Z) format where X/Y/Z are delimited literals
    if (InstructionName == TEXT("ArrayConst") || InstructionName == TEXT("SetConst")) {
        // We know exact element type because it is recorded in the bytecode
        const TSharedPtr<FJsonObject> InnerPropertyObject = Expression->GetObjectField(TEXT("InnerProperty"));
        const TArray<TSharedPtr<FJsonValue>> Values = Expression->GetArrayField(TEXT("Values"));

        // Set array type from recorded instruction, container type is either a set or an array (depending on the opcode)
        FEdGraphPinType ArrayPropertyType = FPropertyTypeHelper::DeserializeGraphPinType(InnerPropertyObject.ToSharedRef(), OwnerBlueprint->SkeletonGeneratedClass);
        if (InstructionName == TEXT("ArrayConst")) {
            ArrayPropertyType.ContainerType = EPinContainerType::Array;
        } else {
            ArrayPropertyType.ContainerType = EPinContainerType::Set;
        }
        LiteralTerminal->Type = ArrayPropertyType;

        // Build a literal with all elements inside
        FString ArrayBuilderString = TEXT("(");
        int32 CurrentIndex = 0;

        for (const TSharedPtr<FJsonValue>& ValueExpression : Values) {
            // We need value constant delimited, because we are building the array literal string
            const TSharedPtr<FKismetTerminal> ValueConstant = ProcessLiteralExpression(ValueExpression->AsObject(), true);
            check(ValueConstant->bIsLiteral);

            if (CurrentIndex++ != 0) {
                ArrayBuilderString.Append(TEXT(","));
            }
            ArrayBuilderString.Append(ValueConstant->StringLiteral);
        }

        ArrayBuilderString += TEXT(")");
        LiteralTerminal->StringLiteral = ArrayBuilderString;
        return LiteralTerminal;
    }

    // Map constants are very similar to set and array constants, we have specific types for them too,
    // and generally they have the same format, with entries recorded as pairs of (key, value)
    if (InstructionName == TEXT("MapConst")) {
        const TSharedPtr<FJsonObject> KeyPropertyObject = Expression->GetObjectField(TEXT("KeyProperty"));
        const TSharedPtr<FJsonObject> ValuePropertyObject = Expression->GetObjectField(TEXT("ValueProperty"));
        const TArray<TSharedPtr<FJsonValue>> Values = Expression->GetArrayField(TEXT("Values"));

        // Set map type from two recorded key and value pin types
        FEdGraphPinType ValuePropertyType = FPropertyTypeHelper::DeserializeGraphPinType(ValuePropertyObject.ToSharedRef(), OwnerBlueprint->SkeletonGeneratedClass);
        FEdGraphPinType KeyPropertyType = FPropertyTypeHelper::DeserializeGraphPinType(KeyPropertyObject.ToSharedRef(), OwnerBlueprint->SkeletonGeneratedClass);

        FEdGraphPinType MapPropertyType = KeyPropertyType;
        MapPropertyType.PinValueType = FEdGraphTerminalType::FromPinType(ValuePropertyType);
        MapPropertyType.ContainerType = EPinContainerType::Map;
        LiteralTerminal->Type = MapPropertyType;

        // Build a literal with all elements inside
        FString MapPairBuilderString = TEXT("(");
        int32 CurrentIndex = 0;

        // Serialize values into the delimited string
        for (const TSharedPtr<FJsonValue>& PairObject : Values) {
            // Object will contain key and value expressions, both of which should be literals
            const TSharedPtr<FJsonObject> KeyExpressionObject = PairObject->AsObject()->GetObjectField(TEXT("Key"));
            const TSharedPtr<FJsonObject> ValueExpressionObject = PairObject->AsObject()->GetObjectField(TEXT("Value"));

            const TSharedPtr<FKismetTerminal> KeyTerminal = ProcessLiteralExpression(KeyExpressionObject, true);
            const TSharedPtr<FKismetTerminal> ValueTerminal = ProcessLiteralExpression(ValueExpressionObject, true);
            check(KeyTerminal->bIsLiteral);
            check(ValueTerminal->bIsLiteral);

            if (CurrentIndex++ != 0) {
                MapPairBuilderString.Append(TEXT(","));
            }

            MapPairBuilderString += TEXT("(");
            MapPairBuilderString.Append(KeyTerminal->StringLiteral);

            MapPairBuilderString += TEXT(", ");

            MapPairBuilderString.Append(ValueTerminal->StringLiteral);
            MapPairBuilderString += TEXT(")");
        }

        MapPairBuilderString += TEXT(")");
        LiteralTerminal->StringLiteral = MapPairBuilderString;
        return LiteralTerminal;
    }

    // Unknown literal type, report it and abort
    checkf(0, TEXT("Found unsupported literal instruction while parsing expression: %s"), *InstructionName);
    return NULL;
}

TSharedPtr<FKismetCompiledStatement> FKismetBytecodeTransformer::ProcessFunctionCallStatement(TSharedPtr<FJsonObject> Statement) {
    TSharedPtr<FKismetTerminal> Context;
    bool bIsInterfaceContext = false;

    // If expression we got is actually a context expression and not a direct function call, unwrap it and record context expression
    const FString InitialInstName = Statement->GetStringField(TEXT("Inst"));
    if (IsContextInstruction(InitialInstName)) {
        const bool bIsClassContext = InitialInstName == TEXT("ClassContext");
        TSharedPtr<FJsonObject> ContextObject = Statement->GetObjectField(TEXT("Context"));

        // Context expression will be prefixed with EX_InterfaceContext if bIsInterfaceContext
        // is set on function call statement. We need to handle it here, otherwise information will be lost
        if (ContextObject->GetStringField(TEXT("Inst")) == TEXT("InterfaceContext")) {
            bIsInterfaceContext = true;
            ContextObject = ContextObject->GetObjectField(TEXT("Expression"));
        }

        Context = ProcessExpression(ContextObject);
        Context->ContextType = bIsClassContext ? FKismetTerminal::EContextType_Class : FKismetTerminal::EContextType_Object;
        Statement = Statement->GetObjectField(TEXT("Expression"));
    }

    const FString InstructionName = Statement->GetStringField(TEXT("Inst"));

    // At this point we should always end up with one of function call instructions
    check(IsCallFunctionInstruction(InstructionName));

    TSharedPtr<FKismetCompiledStatement> Result = MakeShareable(new FKismetCompiledStatement());
    Result->Type = ECompiledStatementType::KCST_CallFunction;
    Result->FunctionContext = Context;
    Result->bIsInterfaceContext = bIsInterfaceContext;

    // Resolve UFunction object by name and provided context
    const FString FunctionName = Statement->GetStringField(TEXT("Function"));

    // Resolve context class for querying function by name
    UClass* ContextObjectClass;

    if (InstructionName != TEXT("CallMath")) {
        // All function calls will have proper context object set, and it will be either an object or class
        // Either way, PSC_Self handling is equal for both of these cases, and resulting class will be
        // class of the object on which function will be called (for static functions, it will be class CDO)

        // Resolve Context class, for Self, use skeleton generated class
        if (Context->Type.PinSubCategory == UEdGraphSchema_K2::PSC_Self) {
            ContextObjectClass = OwnerBlueprint->SkeletonGeneratedClass;
        } else {
            ContextObjectClass = CastChecked<UClass>(Context->Type.PinSubCategoryObject);
        }
    } else {
        // CallMath is special - it does not have any kind of context, so to resolve
        // function we have to look up context class recorded in instruction data
        const FString ContextClassPath = Statement->GetStringField(TEXT("ContextClass"));

        ContextObjectClass = LoadObject<UClass>(NULL, *ContextClassPath);
        checkf(ContextObjectClass, TEXT("Failed to resolve context class for math function, class: %s"), *ContextClassPath);
    }

    UFunction* ResolvedFunction = ContextObjectClass->FindFunctionByName(*FunctionName);
    checkf(ResolvedFunction, TEXT("Couldn't resolve function %s in context of class %s"), *FunctionName, *ContextObjectClass->GetPathName());

    if (InstructionName == TEXT("LocalVirtualFunction") || InstructionName == TEXT("VirtualFunction")) {
        // Obviously it is not a parent context for a virtual function
        Result->FunctionToCall = ResolvedFunction;
        Result->bIsParentContext = false;

    } else if (InstructionName == TEXT("LocalFinalFunction") || InstructionName == TEXT("FinalFunction")) {
        // Final function call, possibly in the parent context
        // We set parent context flag if function in question is not final, but is called with FinalFunction opcode
        Result->FunctionToCall = ResolvedFunction;
        Result->bIsParentContext = !ResolvedFunction->HasAnyFunctionFlags(EFunctionFlags::FUNC_Final);

    } else if (InstructionName == TEXT("CallMath")) {
        Result->FunctionToCall = ResolvedFunction;
        Result->bIsParentContext = false;

    } else {
        checkf(0, TEXT("Unhandled function call opcode: %s"), *InstructionName);
    }

    // Do argument processing that is common for all function call instructions
    TArray<TSharedPtr<FJsonValue>> Parameters = Statement->GetArrayField(TEXT("Parameters"));

    int32 NumParams = 0;
    int32 LatentInfoParameterIndex = -1;

    for (TFieldIterator<UProperty> PropIt(Result->FunctionToCall); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt) {
        UProperty* FuncParamProperty = *PropIt;

        // Skip return values because they are handled in a special way when we happened to be an expression in EX_Let statement
        if (!FuncParamProperty->HasAnyPropertyFlags(CPF_ReturnParm)) {

            // Record LatentInfo parameter which we will resolve later
            if (FuncParamProperty->GetName() == Result->FunctionToCall->GetMetaData("LatentInfo")) {
                LatentInfoParameterIndex = NumParams;
            }

            const TSharedPtr<FJsonObject> ParameterExpression = Parameters[NumParams++]->AsObject();
            TSharedPtr<FKismetTerminal> ParameterTerminal = ProcessFunctionParameter(ParameterExpression);

            Result->RHS.Add(ParameterTerminal);
        }
    }

    // Detect calls into ubergraph. They need special handling because first parameter
    // Is the offset to jump on to start execution of the given event this function is the stub for,
    // and since we do not really expose offsets in compiled statement representation, we need to find
    // compiled statement inside of the ubergraph object and set is as target for this statement
    if (FunctionName == ExecuteUbergraphFunctionName) {
        check(ContextObjectClass == OwnerBlueprint->SkeletonGeneratedClass);

        // Calls into ubergraph should never appear inside of the ubergraph itself
        checkf(!IsUberGraphFunction(), TEXT("Encountered a call into ubergraph inside of the ubergraph itself. This should never happen"));
        check(UberGraphTransformer.IsValid());

        const int32 OffsetIntoUbergraph = FCString::Atoi(*Result->RHS[0]->StringLiteral);
        const TSharedPtr<FKismetCompiledStatement> UberGraphStatement = UberGraphTransformer->StatementsByOffset.FindChecked(OffsetIntoUbergraph);

        Result->TargetLabel = UberGraphStatement;
        Result->bIsCallIntoUbergraph = true;
    }

    // Detect latent function calls that have next statement index to execute in this function
    // After completing async operation inside of the LatentInfo structure parameter. We need to extract it and set as TargetLabel
    if (LatentInfoParameterIndex != -1) {
        // Latent Actions can only appear inside of the uber graph function
        checkf(IsUberGraphFunction(), TEXT("Encountered latent action outside of the uber graph function: %s"), *CurrentFunctionName);

        UScriptStruct* LatentActionInfoStruct = FLatentActionInfo::StaticStruct();
        const TSharedPtr<FKismetTerminal> LatentInfoTerminal = Result->RHS[LatentInfoParameterIndex];

        // Export textual representation of latent action info into the proper struct
        FLatentActionInfo LatentActionInfo;
        LatentActionInfoStruct->ImportText(*LatentInfoTerminal->StringLiteral, &LatentActionInfo, NULL, 0, GError, TEXT("LatentActionInfo"));

        // Latent actions should never point outside of the function they are appearing in
        check(LatentActionInfo.ExecutionFunction.ToString() == CurrentFunctionName);

        // We cannot really resolve pointed compiled statement right now since we haven't finished transforming of the current function yet
        // Kind of patch up we need is essentially the same as for normal goto, so we just schedule a patch up in jump patch up map
        const int32 ResumeOffsetInUberGraph = LatentActionInfo.Linkage;
        this->JumpPatchUpTable.Add(Result, ResumeOffsetInUberGraph);
        Result->bIsCallIntoUbergraph = false;
    }

    return Result;
}

TSharedPtr<FKismetTerminal> FKismetBytecodeTransformer::ProcessFunctionParameter(TSharedPtr<FJsonObject> ParameterExpression) {
    const FString ParameterInstructionName = ParameterExpression->GetStringField(TEXT("Inst"));

    TSharedPtr<FKismetTerminal> ParameterTerminal;

    // Functions can have statements passed as parameters inline sometimes,
    // so we need to handle this gracefully here, because ProcessExpression has no idea how to deal with statements

    // GetArrayItem is one of the only 3 statements that can appear as function arguments. Noticeably, ArrayGetByRef
    // will actually never appear as a dedicated statement outside of inlined generated parameter context
    if (ParameterInstructionName == TEXT("ArrayGetByRef")) {
        TSharedPtr<FKismetCompiledStatement> ArrayItemStatement = MakeShareable(new FKismetCompiledStatement());
        ArrayItemStatement->Type = ECompiledStatementType::KCST_ArrayGetByRef;
        TSharedPtr<FKismetTerminal> ArrayExpression = ProcessExpression(ParameterExpression->GetObjectField(TEXT("ArrayExpression")));
        ArrayItemStatement->RHS.Add(ArrayExpression);
        ArrayItemStatement->RHS.Add(ProcessExpression(ParameterExpression->GetObjectField(TEXT("IndexExpression"))));

        // Inner element type is basically the same as an array expression type, but container type is None and it is always a reference
        // because corresponding byte code sets MostRecentProperty/PropertyValue as well as RESULT_PARAM
        FEdGraphPinType InnerElementType = ArrayExpression->Type;
        check(InnerElementType.ContainerType == EPinContainerType::Array);
        InnerElementType.ContainerType = EPinContainerType::None;
        InnerElementType.bIsReference = true;

        ParameterTerminal = MakeShareable(new FKismetTerminal());
        ParameterTerminal->Type = InnerElementType;
        ParameterTerminal->InlineGeneratedParameter = ArrayItemStatement;
    }

        // Select is also the other node that will never appear on it's own as a dedicated statement,
        // because actually it is a perfectly pure node just returning value depending on the inputs
    else if (ParameterInstructionName == TEXT("SwitchValue")) {
        TSharedPtr<FKismetCompiledStatement> SwitchValue = MakeShareable(new FKismetCompiledStatement());
        SwitchValue->Type = ECompiledStatementType::KCST_SwitchValue;
        // First RHS terminal is an index
        SwitchValue->RHS.Add(ProcessExpression(ParameterExpression->GetObjectField(TEXT("Expression"))));

        // Record value type of the first case, it will be the type of entire case statement as well, falling back to default statement
        // Technically it is incorrect because cases can return more specific types than the base type,
        // but we are okay about it, since types are really only used for property/function resolution, which will work for more specific types
        TSharedPtr<FKismetTerminal> FirstValueTerminal;

        // Append pairs of (case value, case result) cases in order
        TArray<TSharedPtr<FJsonValue>> Cases = ParameterExpression->GetArrayField(TEXT("Cases"));
        for (const TSharedPtr<FJsonValue>& CaseValue : Cases) {
            TSharedPtr<FJsonObject> CasePairObject = CaseValue->AsObject();
            TSharedPtr<FKismetTerminal> CaseTestValue = ProcessExpression(CasePairObject->GetObjectField(TEXT("CaseValue")));
            TSharedPtr<FKismetTerminal> CaseResult = ProcessExpression(CasePairObject->GetObjectField(TEXT("CaseResult")));

            if (!FirstValueTerminal.IsValid()) {
                FirstValueTerminal = CaseResult;
            }

            // Case test values should *always* be literals, because so far only node currently generating KCST_SwitchValue is Select,
            // and select allows selecting between literals by passed value only
            check(CaseTestValue->bIsLiteral);

            SwitchValue->RHS.Add(CaseTestValue);
            SwitchValue->RHS.Add(CaseResult);
        }

        // Default value is appended at the end
        TSharedPtr<FKismetTerminal> DefaultValue = ProcessExpression(ParameterExpression->GetObjectField(TEXT("DefaultResult")));
        SwitchValue->RHS.Add(DefaultValue);

        if (!FirstValueTerminal.IsValid()) {
            FirstValueTerminal = DefaultValue;
        }

        ParameterTerminal = MakeShareable(new FKismetTerminal());
        ParameterTerminal->Type = FirstValueTerminal->Type;
        ParameterTerminal->InlineGeneratedParameter = SwitchValue;
    }

        // Function calls can be inlined too, but as far as I am aware, this is only used for MathExpression node
        // I must admit though, that these are MUCH more powerful than most users think -
        // they can not only use standard mathematical operators and functions, but also ANY
        // static, pure, native final functions exposed by blueprint libraries
        // These functions, obviously, cannot have any context though.
        // They, in fact, will be called with a context object that is logically wrong,
        // but it still works, because native static functions don't use context object at all.
    else if (IsCallFunctionInstruction(ParameterInstructionName)) {
        TSharedPtr<FKismetCompiledStatement> InlineFunctionCall = ProcessFunctionCallStatement(ParameterExpression);
        check(InlineFunctionCall->FunctionContext.IsValid() == false);
        check(InlineFunctionCall->FunctionToCall->HasAllFunctionFlags(FUNC_Final | FUNC_Static | FUNC_Native));

        // We cannot really read pre-recorded information because for virtual functions we have function name only,
        // so we can retrieve return value type only after performing UFunction resolution of virtual function
        // We *could* do it during the bytecode disassembly, but it would be more difficult provided
        // that Kismet VM bytecode is much more flexible than compiled statement representation
        UProperty* ReturnProperty = InlineFunctionCall->FunctionToCall->GetReturnProperty();
        check(ReturnProperty);

        ParameterTerminal = MakeShareable(new FKismetTerminal());
        FPropertyTypeHelper::ConvertPropertyToPinType(ReturnProperty, ParameterTerminal->Type);
        ParameterTerminal->InlineGeneratedParameter = InlineFunctionCall;
    } else {
        // Otherwise it is a simple expression that we need to parse
        ParameterTerminal = ProcessExpression(ParameterExpression);
    }

    check(ParameterTerminal.IsValid());
    return ParameterTerminal;
}
