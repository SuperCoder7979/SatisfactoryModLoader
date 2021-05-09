#include "AssetGeneration/KismetGraphDecompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_ClassDynamicCast.h"
#include "K2Node_ClearDelegate.h"
#include "K2Node_Copy.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_VariableSet.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeMap.h"
#include "K2Node_MakeSet.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_SetFieldsInStruct.h"
#include "K2Node_StructMemberGet.h"
#include "K2Node_VariableSetRef.h"

FKismetGraphDecompiler::FKismetGraphDecompiler(UFunction* Function, UEdGraph* Graph) {
    this->Function = Function;
    this->EditorGraph = Graph;
}

void FKismetGraphDecompiler::Initialize(const TArray<TSharedPtr<FKismetCompiledStatement>>& Statements) {
    this->CompiledStatements = Statements;
}

UEdGraphNode* FKismetGraphDecompiler::CreateMakeMapNode() {
    TSharedPtr<FKismetCompiledStatement> Statement = PopStatement();
    const FVector2D NodePosition = EditorGraph->GetGoodPlaceForNewNode();

    // Spawn node in question and links it's terms with children nodes
    UK2Node_MakeMap* MakeMapNode = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_MakeMap>(
        EditorGraph,
        NodePosition,
        EK2NewNodeFlags::None,
        [&Statement](UK2Node_MakeMap* MakeMapNode) {
            // Setup number of outputs so AllocateDefaultPins will create correct amount of pins
            // We divide number of right hand inputs by two because NumInputs is actually a number of (key, value) pairs
            MakeMapNode->NumInputs = Statement->RHS.Num() / 2;
        }
    );

    // Connect inputs terminals and generate nodes for them
    // First input pin is offset by one because first pin allocated by MakeMap is an output
    for (int32 i = 0; i < Statement->RHS.Num(); i++) {
        UEdGraphPin* MakeMapInputPin = MakeMapNode->Pins[1 + i];
        check(MakeMapInputPin->Direction == EEdGraphPinDirection::EGPD_Input);

        const TSharedPtr<FKismetTerminal> InputTerminalValue = Statement->RHS[i];
        this->TerminalPatchUpMap.Add(MakeMapInputPin, InputTerminalValue);
    }

    // Link output pin with intermediate variable value for replacement
    UEdGraphPin* OutputMakeMapPin = MakeMapNode->Pins[0];
    this->IntermediateVariableHandles.Add(Statement->LHS, OutputMakeMapPin);
    return MakeMapNode;
}

UEdGraphNode* FKismetGraphDecompiler::CreateMakeSetNode() {
    TSharedPtr<FKismetCompiledStatement> Statement = PopStatement();
    const FVector2D NodePosition = EditorGraph->GetGoodPlaceForNewNode();

    // Spawn node in question and links it's terms with children nodes
    UK2Node_MakeSet* MakeSetNode = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_MakeSet>(
        EditorGraph,
        NodePosition,
        EK2NewNodeFlags::None,
        [&Statement](UK2Node_MakeSet* MakeSetNode) {
            // Setup number of outputs so AllocateDefaultPins will create correct amount of pins
            MakeSetNode->NumInputs = Statement->RHS.Num();
        }
    );

    // Connect inputs terminals and generate nodes for them
    // First input pin is offset by one because first pin allocated by MakeMap is an output
    for (int32 i = 0; i < Statement->RHS.Num(); i++) {
        UEdGraphPin* MakeSetInputPin = MakeSetNode->Pins[1 + i];
        check(MakeSetInputPin->Direction == EEdGraphPinDirection::EGPD_Input);

        const TSharedPtr<FKismetTerminal> InputTerminalValue = Statement->RHS[i];
        this->TerminalPatchUpMap.Add(MakeSetInputPin, InputTerminalValue);
    }

    // Link output pin with intermediate variable value for replacement
    UEdGraphPin* OutputMakeSetPin = MakeSetNode->Pins[0];
    this->IntermediateVariableHandles.Add(Statement->LHS, OutputMakeSetPin);
    return MakeSetNode;
}

UEdGraphNode* FKismetGraphDecompiler::CreateMakeArrayNode() {
    TSharedPtr<FKismetCompiledStatement> Statement = PopStatement();
    const FVector2D NodePosition = EditorGraph->GetGoodPlaceForNewNode();

    // Spawn node in question and links it's terms with children nodes
    UK2Node_MakeArray* MakeArrayNode = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_MakeArray>(
        EditorGraph,
        NodePosition,
        EK2NewNodeFlags::None,
        [&Statement](UK2Node_MakeArray* MakeArrayNode) {
            // Setup number of outputs so AllocateDefaultPins will create correct amount of pins
            MakeArrayNode->NumInputs = Statement->RHS.Num();
        }
    );

    // Connect inputs terminals and generate nodes for them
    // First input pin is offset by one because first pin allocated by MakeMap is an output
    for (int32 i = 0; i < Statement->RHS.Num(); i++) {
        UEdGraphPin* MakeArrayInputPin = MakeArrayNode->Pins[1 + i];
        check(MakeArrayInputPin->Direction == EEdGraphPinDirection::EGPD_Input);

        const TSharedPtr<FKismetTerminal> InputTerminalValue = Statement->RHS[i];
        this->TerminalPatchUpMap.Add(MakeArrayInputPin, InputTerminalValue);
    }

    // Link output pin with intermediate variable value for replacement
    UEdGraphPin* OutputMakeArrayPin = MakeArrayNode->Pins[0];
    this->IntermediateVariableHandles.Add(Statement->LHS, OutputMakeArrayPin);
    return MakeArrayNode;
}

UEdGraphNode* FKismetGraphDecompiler::CreateCastNode(bool bIsMetaCast) {
    TSharedPtr<FKismetCompiledStatement> Statement = PopStatement();
    // First RHS terminal will always be a class constant specifying type of class to cast to
    const TSharedPtr<FKismetTerminal> ClassToCastTerminal = Statement->RHS[0];
    check(ClassToCastTerminal->bIsLiteral);
    UClass* ClassObjectToCast = CastChecked<UClass>(ClassToCastTerminal->ObjectLiteral);
    check(ClassObjectToCast);

    const TSharedPtr<FKismetTerminal> InputObjectTerminal = Statement->RHS[1];
    const TSharedPtr<FKismetTerminal> CastedObjectOutVariableName = Statement->LHS;
    const FVector2D NodePosition = EditorGraph->GetGoodPlaceForNewNode();

    // K2Node_DynamicCast will always emit KCST_ObjectToBool as the next statement
    // Variable name generated by it should be wired up with DynamicCast bSuccess pin output instead
    const TSharedPtr<FKismetCompiledStatement> ObjectToBool = PopStatement();
    check(ObjectToBool->Type == ECompiledStatementType::KCST_ObjectToBool);
    const TSharedPtr<FKismetTerminal> CastSucceedOutVariableName = ObjectToBool->LHS;

    // Next statement decides whenever cast is pure or not
    // When cast is not pure, next statements will be KCST_GotoIfNot followed by KCST_UnconditionalGoto
    // with GotoIfNot LHS being equal to CastSucceedOutVariableName
    const TSharedPtr<FKismetCompiledStatement> GotoIfNot = PeekStatement();

    bool bIsCastCompletelyPure = true;
    TSharedPtr<FKismetCompiledStatement> StatementToJumpOnFailure;
    TSharedPtr<FKismetCompiledStatement> StatementToJumpOnSuccess;

    if (GotoIfNot->Type == ECompiledStatementType::KCST_GotoIfNot && GotoIfNot->LHS->operator==(*CastSucceedOutVariableName)) {
        // Pop GotoIfNot from statement list
        PopStatement();

        // This is an impure cast, next statement checks if bSuccess is false and then jumps to false execution statements!
        bIsCastCompletelyPure = false;

        // Next statement generated will be UnconditionalGoto to the success branch
        // But it can be removed during optimization if it jumps just to the next statement
        // so if the next statement is UnconditionalGoto, we Pop it and wire up directly
        // otherwise we just peek it and wire up next statement
        const TSharedPtr<FKismetCompiledStatement> UnconditionalGoto = PeekStatement();

        // Next statement is unconditional goto, Statement to jump on success is it's label
        if (UnconditionalGoto->Type == ECompiledStatementType::KCST_UnconditionalGoto) {
            PopStatement(); // Pop unconditional goto because it is unnecessary
            StatementToJumpOnSuccess = UnconditionalGoto->TargetLabel;
        } else {
            // Next statement is not an unconditional goto, so success branch is just following
            // statement list directly without any jumps
            StatementToJumpOnSuccess = UnconditionalGoto;
        }

        StatementToJumpOnFailure = GotoIfNot->TargetLabel;
    }

    UK2Node_DynamicCast* DynamicCastNode;
    auto NodeInitializerLambda = [bIsCastCompletelyPure, ClassObjectToCast](UK2Node_DynamicCast* DynamicCastNode) {
        // We know at this point whenever cast is going to be pure or impure
        DynamicCastNode->TargetType = ClassObjectToCast;
        DynamicCastNode->SetPurity(bIsCastCompletelyPure);
    };

    if (bIsMetaCast) {
        // For MetaCast, spawn ClassDynamicCast node
        DynamicCastNode = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_ClassDynamicCast>(EditorGraph, NodePosition, EK2NewNodeFlags::None, NodeInitializerLambda);
    } else {
        // Otherwise spawn normal K2Node_DynamicCast
        DynamicCastNode = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_DynamicCast>(EditorGraph, NodePosition, EK2NewNodeFlags::None, NodeInitializerLambda);
    }

    // Connect input object pin with the terminal type passed at RHS
    UEdGraphPin* InputObject = DynamicCastNode->GetCastSourcePin();
    this->TerminalPatchUpMap.Add(InputObject, InputObjectTerminal);

    // Add output object pin into the variable map so it will be connected later
    UEdGraphPin* OutputObjectPin = DynamicCastNode->GetCastResultPin();
    this->IntermediateVariableHandles.Add(CastedObjectOutVariableName, OutputObjectPin);

    // Future actions depend on whenever we are working with pure or an impure cast
    if (bIsCastCompletelyPure) {
        // For pure casts, we also need to add bSuccess pin into the intermediate variable map
        UEdGraphPin* CastSucceedOutputPin = DynamicCastNode->GetBoolSuccessPin();
        this->IntermediateVariableHandles.Add(CastSucceedOutVariableName, CastSucceedOutputPin);
    } else {
        // For impure casts, we need to connect input pin and add output pins to fixup map
        UEdGraphPin* InputExecPin = DynamicCastNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute, EEdGraphPinDirection::EGPD_Input);
        this->NodeExecPinInputMap.Add(Statement, InputExecPin);

        // Automatically connect success and failure execution pins later
        UEdGraphPin* OutputSuccessExecPin = DynamicCastNode->GetValidCastPin();
        UEdGraphPin* OutputFailureExecPin = DynamicCastNode->GetInvalidCastPin();

        this->ExecPinPatchUpMap.Add(OutputSuccessExecPin, StatementToJumpOnSuccess);
        this->ExecPinPatchUpMap.Add(OutputFailureExecPin, StatementToJumpOnFailure);
    }

    return DynamicCastNode;
}

UEdGraphNode* FKismetGraphDecompiler::CreateAssignmentNode() {
    const TSharedPtr<FKismetCompiledStatement> Statement = PeekStatement();

    // KCST_Assignment can be generated when:
    // 1) through FKismetCompilerUtilities::CreateObjectAssignmentStatement call
    // (which can also generate KCST_CastObjToInterface/KCST_CastInterfaceToObj instead, but they both will end up here too)
    // Can be also called by:
    // a) by K2Node_MakeStruct for actually filling struct fields X
    // - Actually MakeStruct has a child node - StructMemberSet, this node is impure and can set individual struct values of local variable directly
    // b) by K2Node_VariableSet for setting user defined variable value
    // c) by K2Node_Copy for copying provided value into the local generated variable X
    // 2) by K2Node_MakeStruct for doing very awkward stuff with EditCondition properties X
    // 3) by K2Node_VariableSetRef to assign external variable passed by reference to the given value
    // 4) by K2Node_SetFieldsInStruct if input struct is not passed by reference to copy it to the returned variable X
    // 5) by K2Node_MultiGate for implementing god knows what N
    // 6) by K2Node_DelegateSet for initializing delegate object with InstanceDelegate on self with provided function name
    // And we need to handle all of these cases... oh well...

    const TSharedPtr<FKismetTerminal> VariableAssigned = Statement->LHS;

    // Only and only K2Node_SetFieldsAndStruct and K2Node_MakeStruct can set struct variables directly
    // there is no way to modify them around these nodes in blueprints through assignment (except MAYBE through BreakStruct + SetVariableByRef)
    // but that case should be handled below if it is even possible
    if (VariableAssigned->Context && VariableAssigned->Context->ContextType == FKismetTerminal::EContextType_Struct) {
        const TSharedPtr<FKismetTerminal> Context = VariableAssigned->Context;
        check(Context->Type.PinCategory == UEdGraphSchema_K2::PC_Struct);

        // If it is a local variable, it should not have a context
        if (IsLocalVariable(Context)) {
            // Local variables of struct type can be set directly using SetFieldsInStruct kismet node
            // This node does not have any data inputs for specifying self, so it will only work for local variables
            // and obviously you cannot connect it to generated local variable
            if (IsUserCreatedLocalVariable(Context)) {
                return CreateStructMemberSetNode(Context);
            }

            // It is a generated variable, try to check for MakeStruct i guess
            const FString ContextPropertyName = VariableAssigned->Context->AssociatedVarProperty;

            // MakeStruct nodes will always spawn a dedicated local variable with recognizable name
            if (ContextPropertyName.StartsWith(TEXT("K2Node_MakeStruct"))) {
                // Struct variable in MakeStruct will always be a local variable, unlike in SetFieldsInStruct
                const TSharedPtr<FKismetTerminal> StructContext = VariableAssigned->Context;

                // Also variable in question will never be user defined, only generated implicitly
                check(!IsUserCreatedLocalVariable(StructContext));

                return CreateMakeStructNode(StructContext);
            }
        }

        // We have a context, and only thing able to handle this so far is SetFieldsInStruct node,
        // which can basically accept any external Terminal as an input pin and will perform operations on it
        // so theoretically it can handle something like SetFieldsInStruct(Object->Object2->Variable)
        return CreateSetFieldsInStructNode(VariableAssigned);
    }

    // Local variables are pretty easy to handle
    if (IsLocalVariable(VariableAssigned)) {
        const FString LocalVariableName = VariableAssigned->AssociatedVarProperty;

        // If we are working with a local variable with copy name, we are definitely working with a K2Node_Copy
        if (LocalVariableName.StartsWith(TEXT("K2Node_Copy_ReturnValue"))) {
            return CreateCopyNode();
        }
        // K2Node_DelegateSet will result in a local variable with a recognisable name too
        if (LocalVariableName.StartsWith(TEXT("K2Node_DelegateSet"))) {
            return CreateDelegateSetNode();
        }
    }

    // Only remaining nodes are K2Node_VariableSet and K2Node_VariableSetRef
    // Difference between them is very simple: VariableSet sets "static" variable predefined earlier,
    // while VariableSetRef can set any unknown value passed by reference to the provided value
    // Solution here would be using VariableSet when possible, while falling back to
    // VariableSetRef when variable expression in question cannot be represented as a static variable reference

    // User-defined local variable assignments can always be expressed through VariableSet node
    if (IsUserCreatedLocalVariable(VariableAssigned)) {
        return CreateVariableSetNode();
    }

    // Variables in object context where object type is known can be expressed by VariableSet too
    if (VariableAssigned->Context && VariableAssigned->Context->ContextType == FKismetTerminal::EContextType_Object) {
        const TSharedPtr<FKismetTerminal> Context = VariableAssigned->Context;
        check(Context->Type.PinCategory == UEdGraphSchema_K2::PC_Object);

        return CreateVariableSetNode();
    }

    // No idea otherwise, try to use VariableSetByRef node to set non-local variable or variable
    return CreateVariableSetByRefNode();
}

void FKismetGraphDecompiler::ConnectMakeStructNodePinsWithTerminals(UK2Node* MakeStructNode, TSharedPtr<FKismetTerminal> StructTerminal) {

    // Checks whenever given statement is basically an assignment in the generated struct context
    const TFunction<bool(TSharedPtr<FKismetCompiledStatement>)> IsMakeStructAssignStatement = [&](const TSharedPtr<FKismetCompiledStatement> Statement) {
        // Casts can replace normal assignments node if object is implicitly casted to interface or vice versa
        // They will still have all properties of the normal assignment statement though
        if (Statement->Type == ECompiledStatementType::KCST_Assignment ||
            Statement->Type == ECompiledStatementType::KCST_CastObjToInterface ||
            Statement->Type == ECompiledStatementType::KCST_CastInterfaceToObj) {
            const TSharedPtr<FKismetTerminal> LeftHandOperand = Statement->LHS;

            // Left hand terminal should have a context and it should be a struct type
            if (LeftHandOperand->Context && LeftHandOperand->Context->ContextType == FKismetTerminal::EContextType_Struct) {
                const TSharedPtr<FKismetTerminal> LeftHandContext = LeftHandOperand->Context;
                // Context of the call should be equal to the struct terminal we are using
                return LeftHandContext->operator==(*StructTerminal);
            }
        }

        return false;
    };

    // Iterate through all following assignment statements
    while (true) {
        // Peek pins as long as we encounter assignments to the struct terminal
        const TSharedPtr<FKismetCompiledStatement> NextStatement = PeekStatement();
        const bool IsMakeStructAssignmentBool = IsMakeStructAssignStatement(NextStatement);

        // This is a MakeStruct assignment, read property name from left side and expression from the right side
        if (IsMakeStructAssignmentBool) {
            const FString StructPropertyName = NextStatement->LHS->AssociatedVarProperty;
            const TSharedPtr<FKismetTerminal> RightHandValue = NextStatement->RHS[0];
            UEdGraphPin* PropertyPin = MakeStructNode->FindPinChecked(*StructPropertyName, EGPD_Input);
            check(PropertyPin);

            // Connect terminal to the property pin of the structure
            this->TerminalPatchUpMap.Add(PropertyPin, RightHandValue);

            // Pop statement that we just handled from the list
            PopStatement();
        } else {
            // That statement is not related to the struct, remove it
            // All struct set statements will be generated in a row, followed by simple goto pulse to the next node
            break;
        }
    }
}

UEdGraphNode* FKismetGraphDecompiler::CreateMakeStructNode(TSharedPtr<FKismetTerminal> StructTerminal) {
    const FVector2D NodePosition = EditorGraph->GetGoodPlaceForNewNode();

    // Struct terminal should always have a struct context and a well-defined struct type
    check(StructTerminal->ContextType == FKismetTerminal::EContextType_Struct);
    check(StructTerminal->Type.PinCategory == UEdGraphSchema_K2::PC_Struct);

    // Determine ScriptStruct object by looking at the type of the struct terminal.
    UScriptStruct* ScriptStruct = CastChecked<UScriptStruct>(StructTerminal->Type.PinSubCategoryObject);
    check(ScriptStruct);

    // Allocate node and connect pins that have statements associated with them
    UK2Node_MakeStruct* MakeStructNode = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_MakeStruct>(
        EditorGraph,
        NodePosition,
        EK2NewNodeFlags::None,
        [ScriptStruct](UK2Node_MakeStruct* MakeStructNode) {
            MakeStructNode->StructType = ScriptStruct;
        }
    );

    // Connect input pins with terminals associated with them
    ConnectMakeStructNodePinsWithTerminals(MakeStructNode, StructTerminal);

    // MakeStruct is a pure node, connect output structure parameter to the generated variable map
    UEdGraphPin* OutputStructPin = MakeStructNode->FindPinChecked(ScriptStruct->GetFName(), EGPD_Output);
    check(OutputStructPin);

    // Make sure we do not contain existing mapping for this terminal, it might happen due to decompiler bugs
    check(!IntermediateVariableHandles.Contains(StructTerminal));

    this->IntermediateVariableHandles.Add(StructTerminal, OutputStructPin);
    return MakeStructNode;
}

UEdGraphNode* FKismetGraphDecompiler::CreateSetFieldsInStructNode(TSharedPtr<FKismetTerminal> StructTerminal) {
    const FVector2D NodePosition = EditorGraph->GetGoodPlaceForNewNode();

    check(StructTerminal->ContextType == FKismetTerminal::EContextType_Struct);
    check(StructTerminal->Type.PinCategory == UEdGraphSchema_K2::PC_Struct);
    UScriptStruct* StructType = CastChecked<UScriptStruct>(StructTerminal->Type.PinSubCategoryObject);

    // Allocate node and connect pins that have statements associated with them
    UK2Node_SetFieldsInStruct* SetFieldsInStruct = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_SetFieldsInStruct>(
        EditorGraph,
        NodePosition,
        EK2NewNodeFlags::None,
        [StructType](UK2Node_SetFieldsInStruct* MemberSetNode) {
            MemberSetNode->StructType = StructType;
        }
    );

    // Record first statement related to struct member set - it will be jumped to from previous node
    const TSharedPtr<FKismetCompiledStatement> FirstStatement = PeekStatement();

    // Connect input pins with terminals associated with them
    // It will work for SetFieldsInStruct because pin names are exactly the same between all struct handling nodes
    ConnectMakeStructNodePinsWithTerminals(SetFieldsInStruct, StructTerminal);

    const TSharedPtr<FKismetCompiledStatement> LastStatement = PeekStatement();

    // Associate input pin with the connected terminal
    UEdGraphPin* InputStructPin = SetFieldsInStruct->FindPinChecked(TEXT("StructRef"), EGPD_Input);
    UEdGraphPin* OutputStructPin = SetFieldsInStruct->FindPinChecked(TEXT("StructOut"), EGPD_Output);

    // Connect input pin to the input terminal
    this->TerminalPatchUpMap.Add(InputStructPin, StructTerminal);

    // SetFieldsInStruct can end with KCST_Assignment statement assigning value to output terminal
    // It will contain struct input terminal as the right side expression, so we can detect it
    // Left side will also contain local variable named in a special way
    if (LastStatement->Type == ECompiledStatementType::KCST_Assignment) {
        const TSharedPtr<FKismetTerminal> LeftSideTerminal = LastStatement->LHS;

        if (IsLocalVariable(LeftSideTerminal) && LeftSideTerminal->AssociatedVarProperty.StartsWith(TEXT("K2Node_SetFieldsInStruct_StructOut"))) {
            // Make sure it has our struct as the right side expression
            check(LastStatement->RHS[0]->operator==(*StructTerminal));
            check(!IsUserCreatedLocalVariable(LeftSideTerminal));

            // Connect output pin to the leftside terminal we are handling
            this->IntermediateVariableHandles.Add(LeftSideTerminal, OutputStructPin);

            // Pop statement
            PopStatement();
        }
    }

    // Since this is an impure node, we need to record input and output pins
    UEdGraphPin* InputExecPin = SetFieldsInStruct->FindPinChecked(UEdGraphSchema_K2::PN_Execute, EEdGraphPinDirection::EGPD_Input);
    this->NodeExecPinInputMap.Add(FirstStatement, InputExecPin);

    // This node does not perform any jumps on it's own, so it will just execute next statement in the instruction list
    UEdGraphPin* OutputExecPin = SetFieldsInStruct->FindPinChecked(UEdGraphSchema_K2::PN_Then, EEdGraphPinDirection::EGPD_Output);
    const TSharedPtr<FKismetCompiledStatement> NextStatement = PeekStatement();

    this->ExecPinPatchUpMap.Add(OutputExecPin, NextStatement);

    return SetFieldsInStruct;
}

UEdGraphNode* FKismetGraphDecompiler::CreateStructMemberSetNode(TSharedPtr<FKismetTerminal> StructTerminal) {
    const FVector2D NodePosition = EditorGraph->GetGoodPlaceForNewNode();
    check(IsUserCreatedLocalVariable(StructTerminal));
    check(StructTerminal->ContextType == FKismetTerminal::EContextType_Struct);
    check(StructTerminal->Type.PinCategory == UEdGraphSchema_K2::PC_Struct);

    const FString LocalVariableName = StructTerminal->AssociatedVarProperty;
    UScriptStruct* StructTypeByTerminal = CastChecked<UScriptStruct>(StructTerminal->Type.PinSubCategoryObject);

    FBPVariableDescription* LocalVariable = FBlueprintEditorUtils::FindLocalVariable(OwnerBlueprint, EditorGraph, *LocalVariableName);
    checkf(LocalVariable, TEXT("Local variable in question not found: %s"), *LocalVariableName);

    check(LocalVariable->VarType.PinCategory == UEdGraphSchema_K2::PC_Struct);
    UScriptStruct* StructTypeByLocalVariable = CastChecked<UScriptStruct>(LocalVariable->VarType.PinSubCategoryObject);

    // Structure types must match between local variable declaration and node context
    check(StructTypeByTerminal == StructTypeByLocalVariable);

    // Allocate node and connect pins that have statements associated with them
    UK2Node_StructMemberSet* MemberSetK2Node = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_StructMemberSet>(
        EditorGraph,
        NodePosition,
        EK2NewNodeFlags::None,
        [&](UK2Node_StructMemberSet* MemberSetNode) {
            MemberSetNode->StructType = StructTypeByLocalVariable;
            MemberSetNode->VariableReference.SetLocalMember(LocalVariable->VarName, Function, LocalVariable->VarGuid);
        }
    );

    // Record first statement related to struct member set - it will be jumped to from previous node
    const TSharedPtr<FKismetCompiledStatement> FirstStatement = PeekStatement();

    // Connect input pins with terminals associated with them
    // It will work because technically MakeStruct is a child of StructMemberSet node
    ConnectMakeStructNodePinsWithTerminals(MemberSetK2Node, StructTerminal);

    // Since this is an impure node, we need to record input and output pins
    UEdGraphPin* InputExecPin = MemberSetK2Node->FindPinChecked(UEdGraphSchema_K2::PN_Execute, EEdGraphPinDirection::EGPD_Input);
    this->NodeExecPinInputMap.Add(FirstStatement, InputExecPin);

    // This node does not perform any jumps on it's own, so it will just execute next statement in the instruction list
    UEdGraphPin* OutputExecPin = MemberSetK2Node->FindPinChecked(UEdGraphSchema_K2::PN_Then, EEdGraphPinDirection::EGPD_Output);
    const TSharedPtr<FKismetCompiledStatement> NextStatement = PeekStatement();
    this->ExecPinPatchUpMap.Add(OutputExecPin, NextStatement);

    return MemberSetK2Node;
}

UEdGraphNode* FKismetGraphDecompiler::CreateCopyNode() {
    const FVector2D NodePosition = EditorGraph->GetGoodPlaceForNewNode();
    const TSharedPtr<FKismetCompiledStatement> AssignmentStatement = PopStatement();
    const TSharedPtr<FKismetTerminal> InputValue = AssignmentStatement->RHS[0];
    const TSharedPtr<FKismetTerminal> LeftHandOperand = AssignmentStatement->LHS;
    check(LeftHandOperand.IsValid());

    // Allocate node and schedule it's pins to be connected later
    UK2Node_Copy* CopyNode = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_Copy>(EditorGraph, NodePosition, EK2NewNodeFlags::None);

    UEdGraphPin* CopyNodeOutputPin = CopyNode->GetCopyResultPin();
    this->IntermediateVariableHandles.Add(LeftHandOperand, CopyNodeOutputPin);

    UEdGraphPin* InputValuePin = CopyNode->GetInputReferencePin();
    this->TerminalPatchUpMap.Add(InputValuePin, InputValue);

    return CopyNode;
}

UEdGraphNode* FKismetGraphDecompiler::CreateDelegateSetNode() {
    // TODO honestly? I have never seen this node and was unable to create it in editor
    // TODO additionally, I haven't found any references to it in Kismet except in the node itself
    // TODO overall, it seems to be a very complex node that basically allows creation of anonymous Events
    // TODO and binding them to multicast delegates
    // TODO anonymous events apparently would function as normal events/functions bound to multicast delegates
    // TODO through traditional MakeDelegate/AddDelegate node chain.
    // TODO we need to keep an eye on it, but for now this node seems to be unfinished/inaccessible.

    checkf(0, TEXT("Unsupported K2 Node encountered: UK2Node_DelegateSet"));
    return NULL;
}

// TODO K2Node_VariableSet can be prefixed with FlushNetDormancy() function call on the same context object
// TODO if property set is the replicated network property.
// TODO unnecessary FlushNetDormancy() call should be removed during post-processing and sanitization
// TODO because during standard code generation we cannot really know what statement we had coming before us
// TODO we could also try to detect it in CreateFunctionCallNode() and just discard such nodes at the start

// TODO x2 Variable set nodes using BlueprintSetter properties will be compiled to function calls
// TODO x2 These need to be handled in CreateFunctionCallNode() and reflected back into VariableSet nodes

UEdGraphNode* FKismetGraphDecompiler::CreateVariableSetNode() {

    const FVector2D NodePosition = EditorGraph->GetGoodPlaceForNewNode();
    const TSharedPtr<FKismetCompiledStatement> AssignmentStatement = PopStatement();
    const TSharedPtr<FKismetTerminal> AssignedTerminal = AssignmentStatement->RHS[0];
    const TSharedPtr<FKismetTerminal> LeftHandOperand = AssignmentStatement->LHS;
    check(LeftHandOperand.IsValid());

    // Kismet graph schema knows how to configure variable references automatically
    const UEdGraphSchema_K2* GraphSchema_K2 = CastChecked<UEdGraphSchema_K2>(EditorGraph->GetSchema());
    const FString& AccessedPropertyName = LeftHandOperand->AssociatedVarProperty;
    UStruct* ContextTypeStruct = ResolveContextTerminalType(LeftHandOperand->Context);

    // Context struct should always contain property in question
    // Only possible edge case is when variable referenced is local variable, but UFunction we are working with
    // should have user-defined local variables already generated prior to code generation, so they can be referenced
    // as normal properties without any special handling
    check(ContextTypeStruct->FindPropertyByName(*AccessedPropertyName));

    // Allocate node and try to resolve referenced member at the start
    UK2Node_VariableSet* MemberSetK2Node = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_VariableSet>(
        EditorGraph,
        NodePosition,
        EK2NewNodeFlags::None,
        [&](UK2Node_VariableSet* VariableSet) {
            GraphSchema_K2->ConfigureVarNode(VariableSet, *AccessedPropertyName, ContextTypeStruct, OwnerBlueprint);
        }
    );

    // Connect object on which variable is set pin, but only do it if we actually have a context and it is not a local variable
    UEdGraphPin* PossiblySelfPin = MemberSetK2Node->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
    if (PossiblySelfPin) {
        const TSharedPtr<FKismetTerminal> ContextTerminal = LeftHandOperand->Context;
        check(ContextTerminal.IsValid());
        this->TerminalPatchUpMap.Add(PossiblySelfPin, ContextTerminal);
    }

    // Connect input data type pin. Pass through pin will never be connected since after compilation it will be transformed into separate VariableGet node
    UEdGraphPin* InputValuePin = MemberSetK2Node->GetValuePin();
    this->TerminalPatchUpMap.Add(InputValuePin, AssignedTerminal);

    // Since this node is not pure, we need to connect Exec pins now
    UEdGraphPin* InputExecPin = MemberSetK2Node->FindPinChecked(UEdGraphSchema_K2::PN_Execute, EEdGraphPinDirection::EGPD_Input);
    this->NodeExecPinInputMap.Add(AssignmentStatement, InputExecPin);

    // Connect next node to the execution then pin
    UEdGraphPin* OutputExecPin = MemberSetK2Node->FindPinChecked(UEdGraphSchema_K2::PN_Then, EEdGraphPinDirection::EGPD_Output);

    // Here comes the special thing: next statement MIGHT be a CallFunction calling local RepNotify
    // Conditions for generation of that statement should match between bytecode and newly allocated node,
    // so if newly created node should call RepNotify, next statement can be stripped (provided that it is expected function call)
    // Note that this call will be issued only for properties using blueprint-defined RepNotifies

    if (MemberSetK2Node->HasLocalRepNotify()) {
        const TSharedPtr<FKismetCompiledStatement> RepNotifyStatement = PopStatement();
        const TSharedPtr<FKismetTerminal> CallContext = RepNotifyStatement->FunctionContext;

        check(RepNotifyStatement->Type == ECompiledStatementType::KCST_CallFunction);
        check(RepNotifyStatement->FunctionToCall);
        check(RepNotifyStatement->FunctionToCall->GetFName() == MemberSetK2Node->GetRepNotifyName());

        // Make sure functions have matching context if we are using context at all
        check(CallContext.IsValid() == AssignedTerminal->Context.IsValid());
        if (AssignedTerminal->Context.IsValid()) {
            check(CallContext->operator==(*AssignedTerminal->Context));
        }
    }

    // Connect next statement
    const TSharedPtr<FKismetCompiledStatement> NextStatement = PeekStatement();
    this->ExecPinPatchUpMap.Add(OutputExecPin, NextStatement);

    return MemberSetK2Node;
}

UEdGraphNode* FKismetGraphDecompiler::CreateVariableSetByRefNode() {
    const FVector2D NodePosition = EditorGraph->GetGoodPlaceForNewNode();
    const TSharedPtr<FKismetCompiledStatement> AssignmentStatement = PopStatement();
    const TSharedPtr<FKismetTerminal> AssignedTerminal = AssignmentStatement->RHS[0];
    const TSharedPtr<FKismetTerminal> LeftHandOperand = AssignmentStatement->LHS;
    check(LeftHandOperand.IsValid());

    // Allocate node without any predefined settings since it doesn't need any
    UK2Node_VariableSetRef* SetVariableByRef = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_VariableSetRef>(EditorGraph, NodePosition, EK2NewNodeFlags::None);

    // Connect input value pins first
    UEdGraphPin* TargetPin = SetVariableByRef->GetTargetPin();
    UEdGraphPin* AssignedValuePin = SetVariableByRef->GetValuePin();

    this->TerminalPatchUpMap.Add(TargetPin, LeftHandOperand);
    this->TerminalPatchUpMap.Add(AssignedValuePin, AssignedTerminal);

    // Since this node is not pure, we need to connect Exec pins now
    UEdGraphPin* InputExecPin = SetVariableByRef->FindPinChecked(UEdGraphSchema_K2::PN_Execute, EEdGraphPinDirection::EGPD_Input);
    this->NodeExecPinInputMap.Add(AssignmentStatement, InputExecPin);

    // This node does not perform any jumps on it's own, so it will just execute next statement in the instruction list
    UEdGraphPin* OutputExecPin = SetVariableByRef->FindPinChecked(UEdGraphSchema_K2::PN_Then, EEdGraphPinDirection::EGPD_Output);
    const TSharedPtr<FKismetCompiledStatement> NextStatement = PeekStatement();
    this->ExecPinPatchUpMap.Add(OutputExecPin, NextStatement);

    return SetVariableByRef;
}

UEdGraphNode* FKismetGraphDecompiler::CreateMakeDelegateNode() {
    const FVector2D NodePosition = EditorGraph->GetGoodPlaceForNewNode();
    const TSharedPtr<FKismetCompiledStatement> AssignmentStatement = PopStatement();
    const TSharedPtr<FKismetTerminal> AssignedDelegate = AssignmentStatement->LHS;

    check(AssignedDelegate.IsValid());
    check(IsLocalVariable(AssignedDelegate));
    check(AssignedDelegate->AssociatedVarProperty.StartsWith(TEXT("K2Node_CreateDelegate_OutputDelegate")));

    UK2Node_CreateDelegate* CreateDelegateNode = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_CreateDelegate>(EditorGraph, NodePosition, EK2NewNodeFlags::None);

    // We cannot set delegate function now because scope of the delegate is unknown since
    // connections from terminals are not handled yet
    // To work around it, we record node and function name we need to set on it in patch up map
    const TSharedPtr<FKismetTerminal> FunctionNameTerminal = AssignmentStatement->RHS[0];
    check(FunctionNameTerminal->bIsLiteral);

    const FString FunctionName = FunctionNameTerminal->StringLiteral;
    this->CreateDelegatePatchUpMap.Add(CreateDelegateNode, *FunctionName);

    // Connect self pin
    const TSharedPtr<FKismetTerminal> ObjectInputTerminal = AssignmentStatement->RHS[1];
    UEdGraphPin* SelfContextPin = CreateDelegateNode->GetObjectInPin();
    this->TerminalPatchUpMap.Add(SelfContextPin, ObjectInputTerminal);

    // Record delegate output pin as associated to this node
    UEdGraphPin* DelegateOutputPin = CreateDelegateNode->GetDelegateOutPin();
    this->IntermediateVariableHandles.Add(AssignedDelegate, DelegateOutputPin);

    return CreateDelegateNode;
}

UEdGraphNode* FKismetGraphDecompiler::CreateMulticastDelegateNode(TSubclassOf<UK2Node_BaseMCDelegate> NodeClass) {
    const FVector2D NodePosition = EditorGraph->GetGoodPlaceForNewNode();
    const UEdGraphSchema_K2* GraphSchema_K2 = CastChecked<UEdGraphSchema_K2>(EditorGraph->GetSchema());
    const TSharedPtr<FKismetCompiledStatement> DelegateStatement = PopStatement();

    // Resolve context class type. Blueprints cannot have event dispatchers as local variables, so we should be alright
    const TSharedPtr<FKismetTerminal> LeftSideTerminal = DelegateStatement->LHS;
    const TSharedPtr<FKismetTerminal> SelfContextTerminal = LeftSideTerminal->Context;
    UClass* ContextTerminalClassType = CastChecked<UClass>(ResolveContextTerminalType(SelfContextTerminal));

    // ResolveContextTerminalType will always return SkeletonGeneratedClass for both EX_Self and
    // variables with the type of this blueprint. It is ensured through KismetBytecodeTransformer routines handling Self specially
    const bool bIsSelfContext = OwnerBlueprint->SkeletonGeneratedClass->IsChildOf(ContextTerminalClassType);

    // Resolve delegate property. Multicast delegate related nodes will only work with UMulticastDelegateProperties
    const FString PropertyName = LeftSideTerminal->AssociatedVarProperty;
    UMulticastDelegateProperty* DelegateProperty = CastChecked<UMulticastDelegateProperty>(ContextTerminalClassType->FindPropertyByName(*PropertyName));

    // Allocate node of the class passed as the argument
    UK2Node_BaseMCDelegate* DelegateNode = Cast<UK2Node_BaseMCDelegate>(
        FEdGraphSchemaAction_K2NewNode::CreateNode(
            EditorGraph,
            TArrayView<UEdGraphPin*>(),
            NodePosition,
            [NodeClass](UEdGraph* InParentGraph) {
                return NewObject<UK2Node>(InParentGraph, NodeClass);
            },
            [DelegateProperty, bIsSelfContext, ContextTerminalClassType](UK2Node* NewNode) {
                UK2Node_BaseMCDelegate* DelegateNode = CastChecked<UK2Node_BaseMCDelegate>(NewNode);
                DelegateNode->SetFromProperty(DelegateProperty, bIsSelfContext, ContextTerminalClassType);
            },
            EK2NewNodeFlags::None
        )
    );

    // Interesting thing: K2Node_BaseMCDelegate can accept MULTIPLE Self inputs and perform operation on each of them
    // We don't really need to have special handling, because what it does is basically generating
    // a statement for each input, or prefixing it with array iteration if input is an array.
    // Either way, functionally it's completely the same thing as having multiple delegate nodes, which we are doing.

    // Connect Self pin first
    UEdGraphPin* SelfPin = GraphSchema_K2->FindSelfPin(*DelegateNode, EGPD_Input);
    check(SelfPin);
    this->TerminalPatchUpMap.Add(SelfPin, SelfContextTerminal);

    // Connect delegate input pin if the target node has one (ClearDelegate does not, for example)
    // If we don't have the delegate pin, RHS operand list will be empty too
    UEdGraphPin* DelegatePin = DelegateNode->GetDelegatePin();

    if (DelegatePin != NULL) {
        check(DelegateStatement->RHS.Num());
        const TSharedPtr<FKismetTerminal> RightHandOperand = DelegateStatement->RHS[0];
        this->TerminalPatchUpMap.Add(DelegatePin, RightHandOperand);
    }

    // Connect Exec input and output pins, since neither of these nodes are pure
    UEdGraphPin* InputExecPin = DelegateNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute, EEdGraphPinDirection::EGPD_Input);
    this->NodeExecPinInputMap.Add(DelegateStatement, InputExecPin);

    // This node does not perform any jumps on it's own, so it will just execute next statement in the instruction list
    UEdGraphPin* OutputExecPin = DelegateNode->FindPinChecked(UEdGraphSchema_K2::PN_Then, EEdGraphPinDirection::EGPD_Output);
    const TSharedPtr<FKismetCompiledStatement> NextStatement = PeekStatement();
    this->ExecPinPatchUpMap.Add(OutputExecPin, NextStatement);

    return DelegateNode;
}

UEdGraphNode* FKismetGraphDecompiler::CreateFunctionCallNode() {
    const UEdGraphSchema_K2* GraphSchema_K2 = CastChecked<UEdGraphSchema_K2>(EditorGraph->GetSchema());
    const FVector2D NodePosition = EditorGraph->GetGoodPlaceForNewNode();
    const TSharedPtr<FKismetCompiledStatement> CallFunctionStatement = PopStatement();
    check(CallFunctionStatement->Type == ECompiledStatementType::KCST_CallFunction);

    return NULL;
}

UEdGraphNode* FKismetGraphDecompiler::GenerateNodeForStatement() {
    const TSharedPtr<FKismetCompiledStatement> Statement = PeekStatement();

    // Just skip Nop instructions altogether and skip to processing next one
    if (Statement->Type == ECompiledStatementType::KCST_Nop) {
        PopStatement();
        return NULL;
    }

    // KCST_CreateMap statement can be generated only in two cases: either by user-created K2Node_MakeMap,
    // or by compiler generated K2Node_MakeVariable, generated to create a map value to be then used by K2Node_VariableSet
    // apparently both nodes have roughly identical functionality, but K2Node_VariableSet is capable of
    // "automatically initializing" itself from property types, but it's not really important during decompilation,
    // so we basically just treat every KCST_CreateMap opcode as K2Node_MakeMap
    // Later, if it can be reduced into default local variable value assignment, it will be removed and set as default value,
    // which will cause the generation of K2Node_MakeVariable by function entry again, resulting in the equal graph
    if (Statement->Type == ECompiledStatementType::KCST_CreateMap) {
        return CreateMakeMapNode();
    }

    // Exactly the same thing for KCST_CreateSet instruction - we treat all of them as K2Node_MakeSet
    if (Statement->Type == ECompiledStatementType::KCST_CreateSet) {
        return CreateMakeSetNode();
    }

    // Exactly the same thing with KCST_CreateArray instruction - we treat all of them as K2Node_MakeArray
    if (Statement->Type == ECompiledStatementType::KCST_CreateArray) {
        return CreateMakeArrayNode();
    }

    // These two instructions are only ever emitted to K2Node_DynamicCast, so it is completely safe to convert them directly
    if (Statement->Type == ECompiledStatementType::KCST_DynamicCast ||
        Statement->Type == ECompiledStatementType::KCST_CrossInterfaceCast) {
        return CreateCastNode(false);
    }

    // KCST_MetaCast will be emitted only by cast nodes casting from one class type to another
    if (Statement->Type == ECompiledStatementType::KCST_MetaCast) {
        return CreateCastNode(true);
    }

    // These statements can appear in multiple cases:
    // 1) As the part of K2Node_DynamicCast
    // 2) Instead of KCST_Assignment when trying to assign interface value to object property or vice versa
    // 3) When interface is passed to a function call node where object argument is expected, or vise versa
    if (Statement->Type == ECompiledStatementType::KCST_CastInterfaceToObj ||
        Statement->Type == ECompiledStatementType::KCST_CastObjToInterface) {

        // User-defined variables can only happen as the part of KCST_Assignment instructions
        if (IsUserCreatedLocalVariable(Statement->LHS)) {
            return CreateAssignmentNode();
        }

        // Otherwise it is a generated variable, and we can use a name to differentiate between 3 cases
        const FString VariableName = Statement->LHS->AssociatedVarProperty;

        // Dynamic cast node, handle the same way as KCST_DynamicCast
        if (VariableName.StartsWith(TEXT("K2Node_DynamicCast"))) {
            return CreateCastNode(false);
        }

        // Function call node with parameter converter in-place from interface to object or vice versa
        // Skipping the cast in that case is fine, Kismet Compiler will regenerate it
        // TODO it is really curious case, because usually Editor suggests to generate explicit cast itself
        // TODO but FactoryGame blueprints actually have one such case, so we will support it for now
        if (VariableName.StartsWith(TEXT("CallFunc_")) && VariableName.EndsWith(TEXT("_CastInput"))) {
            // Pop statement of the generated cast
            PopStatement();

            // Mark the output terminal as the pass-through of the input terminal
            const TSharedPtr<FKismetTerminal> InputObjectTerminal = Statement->RHS[1];
            const TSharedPtr<FKismetTerminal> OutputObjectTerminal = Statement->LHS;
            this->PassThroughTerminals.Add(OutputObjectTerminal, InputObjectTerminal);
            return NULL;
        }

        // Otherwise it should be assignment node with generated variable name, which can happen for various node types
        return CreateAssignmentNode();
    }

    // Assignment statements can be a part of multiple nodes, exact handling will be decided in CreateAssignmentNode
    if (Statement->Type == ECompiledStatementType::KCST_Assignment) {
        return CreateAssignmentNode();
    }

    // These statements are only generated by delegate handling nodes
    if (Statement->Type == ECompiledStatementType::KCST_BindDelegate) {
        return CreateMakeDelegateNode();
    }

    // Next 3 statements are basically handled in the same way, only thing different is the class of node generated
    if (Statement->Type == ECompiledStatementType::KCST_AddMulticastDelegate) {
        return CreateMulticastDelegateNode(UK2Node_AddDelegate::StaticClass());
    }
    if (Statement->Type == ECompiledStatementType::KCST_RemoveMulticastDelegate) {
        return CreateMulticastDelegateNode(UK2Node_RemoveDelegate::StaticClass());
    }
    if (Statement->Type == ECompiledStatementType::KCST_ClearMulticastDelegate) {
        return CreateMulticastDelegateNode(UK2Node_ClearDelegate::StaticClass());
    }

    if (Statement->Type == ECompiledStatementType::KCST_CallFunction) {

    }

    return NULL;
}

UStruct* FKismetGraphDecompiler::ResolveContextTerminalType(TSharedPtr<FKismetTerminal> Terminal) {
    // If context terminal is not valid, it is actually a local variable, and context type is... a function itself
    if (!Terminal.IsValid()) {
        return Function;
    }

    // Struct is the type of the terminal, UScriptStruct instance should be stored as sub category object
    if (Terminal->Type.PinCategory == UEdGraphSchema_K2::PC_Struct) {
        check(Terminal->ContextType == FKismetTerminal::EContextType_Struct);
        UScriptStruct* ScriptStruct = CastChecked<UScriptStruct>(Terminal->Type.PinSubCategoryObject);

        return ScriptStruct;
    }

    // Type of the terminal is interface, pin sub category object would be object class
    if (Terminal->Type.PinCategory == UEdGraphSchema_K2::PC_Interface) {
        check(Terminal->ContextType == FKismetTerminal::EContextType_Object);
        UClass* InterfaceClass = CastChecked<UClass>(Terminal->Type.PinSubCategoryObject);

        check(InterfaceClass->IsChildOf<UInterface>());
        return InterfaceClass;
    }

    // Type of the terminal is Class, pin sub category is the meta class type
    // Even though type of the context pin is Class, as context Class CDO object will be used
    // so correct context terminal type is the value of meta class as specified in sub object
    if (Terminal->Type.PinCategory == UEdGraphSchema_K2::PC_Class) {
        check(Terminal->ContextType == FKismetTerminal::EContextType_Class);

        // Sometimes SubCategory can be Self, then PinSubCategoryObject is ignored
        UClass* MetaClassType;
        if (Terminal->Type.PinSubCategory == UEdGraphSchema_K2::PSC_Self) {
            MetaClassType = OwnerBlueprint->SkeletonGeneratedClass;
        } else {
            MetaClassType = CastChecked<UClass>(Terminal->Type.PinSubCategoryObject);
        }

        return MetaClassType;
    }

    // Objects are handled exactly in the same way as PC_Class
    if (Terminal->Type.PinCategory == UEdGraphSchema_K2::PC_Object) {
        check(Terminal->ContextType == FKismetTerminal::EContextType_Object);

        // Sometimes SubCategory can be Self, then PinSubCategoryObject is ignored
        UClass* ObjectClassType;
        if (Terminal->Type.PinSubCategory == UEdGraphSchema_K2::PSC_Self) {
            ObjectClassType = OwnerBlueprint->SkeletonGeneratedClass;
        } else {
            ObjectClassType = CastChecked<UClass>(Terminal->Type.PinSubCategoryObject);
        }

        return ObjectClassType;
    }

    checkf(0, TEXT("Unknown context terminal type: Category = %s, Sub Category = %s"), *Terminal->Type.PinCategory.ToString(), *Terminal->Type.PinSubCategory.ToString());
    return NULL;
}
