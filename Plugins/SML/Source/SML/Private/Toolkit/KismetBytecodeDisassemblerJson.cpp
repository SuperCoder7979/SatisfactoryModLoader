#include "Toolkit/KismetBytecodeDisassemblerJson.h"
#include "Serialization/JsonSerializer.h"
#include "Toolkit/PropertyTypeHandler.h"

TSharedPtr<FJsonObject> FKismetBytecodeDisassemblerJson::SerializeExpression(int32& ScriptIndex) {
    EExprToken Opcode = (EExprToken)ReadByte(ScriptIndex);
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());

    switch (Opcode) {
    case EX_PrimitiveCast: {
        Result->SetStringField(TEXT("Inst"), TEXT("PrimitiveCast"));
        // A type conversion.
        uint8 ConversionType = ReadByte(ScriptIndex);

        if (ConversionType == ECastToken::CST_InterfaceToBool) {
            Result->SetStringField(TEXT("CastType"), TEXT("InterfaceToBool"));
        } else if (ConversionType == ECastToken::CST_ObjectToBool) {
            Result->SetStringField(TEXT("CastType"), TEXT("ObjectToBool"));
        } else if (ConversionType == ECastToken::CST_ObjectToInterface) {
            // Only remaining conversion type is CST_ObjectToInterface, but it is never generated
            // by Kismet backend and is phased out in favor of separate instruction
            // We will support it regardless, because VM actually supports code with it (for now)

            Result->SetStringField(TEXT("CastType"), TEXT("ObjectToInterface"));
            UClass* InterfaceClass = ReadPointer<UClass>(ScriptIndex);
            Result->SetStringField(TEXT("InterfaceClass"), InterfaceClass->GetPathName());
        } else {
            checkf(0, TEXT("Unsupported primitive cast type %d"), ConversionType);
        }

        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_SetSet: {
        Result->SetStringField(TEXT("Inst"), TEXT("SetSet"));
        Result->SetObjectField(TEXT("LeftSideExpression"), SerializeExpression(ScriptIndex));

        TArray<TSharedPtr<FJsonValue>> Values;
        ReadInt(ScriptIndex); // Skip element amount

        while (Script[ScriptIndex] != EX_EndSet) {
            TSharedPtr<FJsonObject> Expression = SerializeExpression(ScriptIndex);
            Values.Add(MakeShareable(new FJsonValueObject(Expression)));
        }
        ScriptIndex++; // Skip EX_EndSet
        Result->SetArrayField(TEXT("Values"), Values);
        break;
    }
    case EX_SetConst: {
        UProperty* InnerProp = ReadPointer<UProperty>(ScriptIndex);
        FEdGraphPinType PropertyPinType;
        FPropertyTypeHelper::ConvertPropertyToPinType(InnerProp, PropertyPinType);

        Result->SetStringField(TEXT("Inst"), TEXT("SetConst"));
        Result->SetObjectField(TEXT("InnerProperty"), FPropertyTypeHelper::SerializeGraphPinType(PropertyPinType, SelfScope.Get()));

        TArray<TSharedPtr<FJsonValue>> Values;
        ReadInt(ScriptIndex); // Skip element amount

        while (Script[ScriptIndex] != EX_EndSetConst) {
            TSharedPtr<FJsonObject> Expression = SerializeExpression(ScriptIndex);
            Values.Add(MakeShareable(new FJsonValueObject(Expression)));
        }
        ScriptIndex++; // Skip EX_EndSetConst
        Result->SetArrayField(TEXT("Values"), Values);
        break;
    }
    case EX_SetMap: {
        Result->SetStringField(TEXT("Inst"), TEXT("SetMap"));
        Result->SetObjectField(TEXT("LeftSideExpression"), SerializeExpression(ScriptIndex));

        TArray<TSharedPtr<FJsonValue>> Values;
        ReadInt(ScriptIndex); // Skip element amount

        while (Script[ScriptIndex] != EX_EndMap) {
            TSharedPtr<FJsonObject> KeyExpression = SerializeExpression(ScriptIndex);
            TSharedPtr<FJsonObject> ValueExpression = SerializeExpression(ScriptIndex);

            TSharedRef<FJsonObject> Pair = MakeShareable(new FJsonObject());
            Pair->SetObjectField(TEXT("Key"), KeyExpression);
            Pair->SetObjectField(TEXT("Value"), ValueExpression);
            Values.Add(MakeShareable(new FJsonValueObject(Pair)));
        }
        ScriptIndex++; // Skip EX_EndMap
        Result->SetArrayField(TEXT("Values"), Values);
        break;
    }
    case EX_MapConst: {
        Result->SetStringField(TEXT("Inst"), TEXT("MapConst"));

        UProperty* KeyProp = ReadPointer<UProperty>(ScriptIndex);
        FEdGraphPinType KeyPropPinType;
        FPropertyTypeHelper::ConvertPropertyToPinType(KeyProp, KeyPropPinType);
        Result->SetObjectField(TEXT("KeyProperty"), FPropertyTypeHelper::SerializeGraphPinType(KeyPropPinType, SelfScope.Get()));

        UProperty* ValProp = ReadPointer<UProperty>(ScriptIndex);
        FEdGraphPinType ValuePropPinType;
        FPropertyTypeHelper::ConvertPropertyToPinType(ValProp, ValuePropPinType);
        Result->SetObjectField(TEXT("ValueProperty"), FPropertyTypeHelper::SerializeGraphPinType(ValuePropPinType, SelfScope.Get()));

        TArray<TSharedPtr<FJsonValue>> Values;
        ReadInt(ScriptIndex); // Skip element amount

        while (Script[ScriptIndex] != EX_EndMap) {
            TSharedPtr<FJsonObject> KeyExpression = SerializeExpression(ScriptIndex);
            TSharedPtr<FJsonObject> ValueExpression = SerializeExpression(ScriptIndex);

            TSharedRef<FJsonObject> Pair = MakeShareable(new FJsonObject());
            Pair->SetObjectField(TEXT("Key"), KeyExpression);
            Pair->SetObjectField(TEXT("Value"), ValueExpression);
            Values.Add(MakeShareable(new FJsonValueObject(Pair)));
        }
        ScriptIndex++; // Skip EX_EndMapConst
        Result->SetArrayField(TEXT("Values"), Values);
        break;
    }
    case EX_ObjToInterfaceCast: {
        Result->SetStringField(TEXT("Inst"), TEXT("ObjToInterfaceCast"));
        UClass* InterfaceClass = ReadPointer<UClass>(ScriptIndex);

        Result->SetStringField(TEXT("InterfaceClass"), InterfaceClass->GetPathName());
        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_CrossInterfaceCast: {
        Result->SetStringField(TEXT("Inst"), TEXT("CrossInterfaceCast"));
        UClass* InterfaceClass = ReadPointer<UClass>(ScriptIndex);

        Result->SetStringField(TEXT("InterfaceClass"), InterfaceClass->GetPathName());
        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_InterfaceToObjCast: {
        Result->SetStringField(TEXT("Inst"), TEXT("InterfaceToObjCast"));
        UClass* ObjectClass = ReadPointer<UClass>(ScriptIndex);

        Result->SetStringField(TEXT("ObjectClass"), ObjectClass->GetPathName());
        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_Let: {
        Result->SetStringField(TEXT("Inst"), TEXT("Let"));

        // Skip property pointer, type can be deduced from Variable expression
        ReadPointer<FProperty>(ScriptIndex);

        Result->SetObjectField(TEXT("Variable"), SerializeExpression(ScriptIndex));
        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_LetObj: {
        Result->SetStringField(TEXT("Inst"), TEXT("LetObj"));

        Result->SetObjectField(TEXT("Variable"), SerializeExpression(ScriptIndex));
        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_LetWeakObjPtr: {
        Result->SetStringField(TEXT("Inst"), TEXT("LetWeakObjPtr"));

        Result->SetObjectField(TEXT("Variable"), SerializeExpression(ScriptIndex));
        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_LetBool: {
        Result->SetStringField(TEXT("Inst"), TEXT("LetBool"));

        Result->SetObjectField(TEXT("Variable"), SerializeExpression(ScriptIndex));
        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_LetValueOnPersistentFrame: {
        Result->SetStringField(TEXT("Inst"), TEXT("LetValueOnPersistentFrame"));

        UProperty* Property = ReadPointer<UProperty>(ScriptIndex);
        Result->SetStringField(TEXT("PropertyName"), Property->GetName());

        FEdGraphPinType PropertyType;
        FPropertyTypeHelper::ConvertPropertyToPinType(Property, PropertyType);
        Result->SetObjectField(TEXT("PropertyType"), FPropertyTypeHelper::SerializeGraphPinType(PropertyType, SelfScope.Get()));

        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_StructMemberContext: {
        Result->SetStringField(TEXT("Inst"), TEXT("StructMemberContext"));

        UProperty* Property = ReadPointer<UProperty>(ScriptIndex);
        FEdGraphPinType PropertyPinType;
        FPropertyTypeHelper::ConvertPropertyToPinType(Property, PropertyPinType);
        Result->SetObjectField(TEXT("PropertyType"), FPropertyTypeHelper::SerializeGraphPinType(PropertyPinType, SelfScope.Get()));
        Result->SetStringField(TEXT("PropertyName"), Property->GetName());

        Result->SetObjectField(TEXT("StructExpression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_LetDelegate: {
        Result->SetStringField(TEXT("Inst"), TEXT("LetDelegate"));

        Result->SetObjectField(TEXT("Variable"), SerializeExpression(ScriptIndex));
        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_LocalVirtualFunction: {
        Result->SetStringField(TEXT("Inst"), TEXT("LocalVirtualFunction"));
        FString FunctionName = ReadName(ScriptIndex);
        Result->SetStringField(TEXT("FunctionName"), FunctionName);

        TArray<TSharedPtr<FJsonValue>> Parameters;
        while (Script[ScriptIndex] != EX_EndFunctionParms) {
            TSharedPtr<FJsonObject> Parameter = SerializeExpression(ScriptIndex);
            Parameters.Add(MakeShareable(new FJsonValueObject(Parameter)));
        }
        ScriptIndex++; // Skip EX_EndFunctionParms
        Result->SetArrayField(TEXT("Parameters"), Parameters);
        break;
    }
    case EX_LocalFinalFunction: {
        Result->SetStringField(TEXT("Inst"), TEXT("LocalFinalFunction"));
        UFunction* StackNode = ReadPointer<UFunction>(ScriptIndex);
        Result->SetStringField(TEXT("Function"), StackNode->GetName());

        TArray<TSharedPtr<FJsonValue>> Parameters;
        while (Script[ScriptIndex] != EX_EndFunctionParms) {
            TSharedPtr<FJsonObject> Parameter = SerializeExpression(ScriptIndex);
            Parameters.Add(MakeShareable(new FJsonValueObject(Parameter)));
        }
        ScriptIndex++; // Skip EX_EndFunctionParms
        Result->SetArrayField(TEXT("Parameters"), Parameters);
        break;
    }
    case EX_LetMulticastDelegate: {
        Result->SetStringField(TEXT("Inst"), TEXT("LetMulticastDelegate"));

        Result->SetObjectField(TEXT("Variable"), SerializeExpression(ScriptIndex));
        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }

    case EX_ComputedJump: {
        Result->SetStringField(TEXT("Inst"), TEXT("ComputedJump"));
        Result->SetObjectField(TEXT("OffsetExpression"), SerializeExpression(ScriptIndex));
        break;
    }

    case EX_Jump: {
        Result->SetStringField(TEXT("Inst"), TEXT("Jump"));
        CodeSkipSizeType SkipCount = ReadSkipCount(ScriptIndex);
        Result->SetNumberField(TEXT("Offset"), SkipCount);
        break;
    }
    case EX_LocalVariable: {
        Result->SetStringField(TEXT("Inst"), TEXT("LocalVariable"));

        UProperty* Property = ReadPointer<UProperty>(ScriptIndex);
        FEdGraphPinType PropertyPinType;
        FPropertyTypeHelper::ConvertPropertyToPinType(Property, PropertyPinType);

        Result->SetObjectField(TEXT("VariableType"), FPropertyTypeHelper::SerializeGraphPinType(PropertyPinType, SelfScope.Get()));
        Result->SetStringField(TEXT("VariableName"), Property->GetName());
        break;
    }
    case EX_DefaultVariable: {
        Result->SetStringField(TEXT("Inst"), TEXT("DefaultVariable"));

        UProperty* Property = ReadPointer<UProperty>(ScriptIndex);
        FEdGraphPinType PropertyPinType;
        FPropertyTypeHelper::ConvertPropertyToPinType(Property, PropertyPinType);

        Result->SetObjectField(TEXT("VariableType"), FPropertyTypeHelper::SerializeGraphPinType(PropertyPinType, SelfScope.Get()));
        Result->SetStringField(TEXT("VariableName"), Property->GetName());
        break;
    }
    case EX_InstanceVariable: {
        Result->SetStringField(TEXT("Inst"), TEXT("InstanceVariable"));

        UProperty* Property = ReadPointer<UProperty>(ScriptIndex);
        FEdGraphPinType PropertyPinType;
        FPropertyTypeHelper::ConvertPropertyToPinType(Property, PropertyPinType);

        Result->SetObjectField(TEXT("VariableType"), FPropertyTypeHelper::SerializeGraphPinType(PropertyPinType, SelfScope.Get()));
        Result->SetStringField(TEXT("VariableName"), Property->GetName());
        break;
    }
    case EX_LocalOutVariable: {
        Result->SetStringField(TEXT("Inst"), TEXT("LocalOutVariable"));

        UProperty* Property = ReadPointer<UProperty>(ScriptIndex);
        FEdGraphPinType PropertyPinType;
        FPropertyTypeHelper::ConvertPropertyToPinType(Property, PropertyPinType);

        Result->SetObjectField(TEXT("VariableType"), FPropertyTypeHelper::SerializeGraphPinType(PropertyPinType, SelfScope.Get()));
        Result->SetStringField(TEXT("VariableName"), Property->GetName());
        break;
    }
    case EX_InterfaceContext: {
        Result->SetStringField(TEXT("Inst"), TEXT("InterfaceContext"));
        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_DeprecatedOp4A: {
        Result->SetStringField(TEXT("Inst"), TEXT("DeprecatedOp4A"));
        break;
    }
    case EX_Nothing: {
        Result->SetStringField(TEXT("Inst"), TEXT("Nothing"));
        break;
    }
    case EX_EndOfScript: {
        Result->SetStringField(TEXT("Inst"), TEXT("EndOfScript"));
        break;
    }
    case EX_IntZero: {
        Result->SetStringField(TEXT("Inst"), TEXT("IntZero"));
        break;
    }
    case EX_IntOne: {
        Result->SetStringField(TEXT("Inst"), TEXT("IntOne"));
        break;
    }
    case EX_True: {
        Result->SetStringField(TEXT("Inst"), TEXT("True"));
        break;
    }
    case EX_False: {
        Result->SetStringField(TEXT("Inst"), TEXT("False"));
        break;
    }
    case EX_NoObject: {
        Result->SetStringField(TEXT("Inst"), TEXT("NoObject"));
        break;
    }
    case EX_NoInterface: {
        Result->SetStringField(TEXT("Inst"), TEXT("NoInterface"));
        break;
    }
    case EX_Self: {
        Result->SetStringField(TEXT("Inst"), TEXT("Self"));
        break;
    }
    case EX_Return: {
        Result->SetStringField(TEXT("Inst"), TEXT("Return"));
        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_CallMath: {
        Result->SetStringField(TEXT("Inst"), TEXT("CallMath"));

        UFunction* StackNode = ReadPointer<UFunction>(ScriptIndex);
        Result->SetStringField(TEXT("Function"), StackNode->GetName());

        // EX_CallMath will never have EX_Context instructions because they don't need any context,
        // And because of that we need to record context type manually. It should always be a native class though.
        UClass* MemberParentClass = StackNode->GetOuterUClass();
        check(MemberParentClass->HasAllClassFlags(CLASS_Native));

        Result->SetStringField(TEXT("ContextClass"), MemberParentClass->GetPathName());

        TArray<TSharedPtr<FJsonValue>> Parameters;
        while (Script[ScriptIndex] != EX_EndFunctionParms) {
            TSharedPtr<FJsonObject> Parameter = SerializeExpression(ScriptIndex);
            Parameters.Add(MakeShareable(new FJsonValueObject(Parameter)));
        }
        ScriptIndex++; // Skip EX_EndFunctionParms
        Result->SetArrayField(TEXT("Parameters"), Parameters);
        break;
    }
    case EX_FinalFunction: {
        Result->SetStringField(TEXT("Inst"), TEXT("FinalFunction"));
        UFunction* StackNode = ReadPointer<UFunction>(ScriptIndex);
        Result->SetStringField(TEXT("Function"), StackNode->GetName());

        TArray<TSharedPtr<FJsonValue>> Parameters;
        while (Script[ScriptIndex] != EX_EndFunctionParms) {
            TSharedPtr<FJsonObject> Parameter = SerializeExpression(ScriptIndex);
            Parameters.Add(MakeShareable(new FJsonValueObject(Parameter)));
        }
        ScriptIndex++; // Skip EX_EndFunctionParms
        Result->SetArrayField(TEXT("Parameters"), Parameters);
        break;
    }
    case EX_CallMulticastDelegate: {
        Result->SetStringField(TEXT("Inst"), TEXT("CallMulticastDelegate"));
        UFunction* StackNode = ReadPointer<UFunction>(ScriptIndex);
        UClass* DelegateSignatureParent = StackNode->GetOuterUClass();
        const bool bIsSelfContext = DelegateSignatureParent == SelfScope;

        TSharedPtr<FJsonObject> DelegateSignatureFunction = MakeShareable(new FJsonObject());

        DelegateSignatureFunction->SetBoolField(TEXT("IsSelfContext"), bIsSelfContext);
        DelegateSignatureFunction->SetStringField("MemberParent", DelegateSignatureParent->GetPathName());
        DelegateSignatureFunction->SetStringField(TEXT("MemberName"), StackNode->GetName());

        Result->SetObjectField(TEXT("DelegateSignatureFunction"), DelegateSignatureFunction);

        Result->SetObjectField(TEXT("Delegate"), SerializeExpression(ScriptIndex));

        TArray<TSharedPtr<FJsonValue>> Parameters;
        while (Script[ScriptIndex] != EX_EndFunctionParms) {
            TSharedPtr<FJsonObject> Parameter = SerializeExpression(ScriptIndex);
            Parameters.Add(MakeShareable(new FJsonValueObject(Parameter)));
        }
        ScriptIndex++; // Skip EX_EndFunctionParms
        Result->SetArrayField(TEXT("Parameters"), Parameters);
        break;
    }
    case EX_VirtualFunction: {
        Result->SetStringField(TEXT("Inst"), TEXT("VirtualFunction"));
        FString FunctionName = ReadName(ScriptIndex);
        Result->SetStringField(TEXT("Function"), FunctionName);

        TArray<TSharedPtr<FJsonValue>> Parameters;
        while (Script[ScriptIndex] != EX_EndFunctionParms) {
            TSharedPtr<FJsonObject> Parameter = SerializeExpression(ScriptIndex);
            Parameters.Add(MakeShareable(new FJsonValueObject(Parameter)));
        }
        ScriptIndex++; // Skip EX_EndFunctionParms
        Result->SetArrayField(TEXT("Parameters"), Parameters);
        break;
    }
    case EX_ClassContext:
    case EX_Context:
    case EX_Context_FailSilent: {
        if (Opcode == EX_ClassContext) {
            Result->SetStringField(TEXT("Inst"), TEXT("ClassContext"));
        } else if (Opcode == EX_Context) {
            Result->SetStringField(TEXT("Inst"), TEXT("Context"));
        } else {
            Result->SetStringField(TEXT("Inst"), TEXT("Context_FailSilent"));
        }

        Result->SetObjectField(TEXT("Context"), SerializeExpression(ScriptIndex));

        // Code offset for NULL expressions
        CodeSkipSizeType SkipCount = ReadSkipCount(ScriptIndex);
        Result->SetNumberField(TEXT("SkipOffsetForNull"), SkipCount);

        // Property corresponding to the r-value data, in case the l-value needs to be mem-zero'd
        UProperty* Field = ReadPointer<UProperty>(ScriptIndex);
        if (Field) {
            FEdGraphPinType FieldPinType;
            FPropertyTypeHelper::ConvertPropertyToPinType(Field, FieldPinType);
            Result->SetObjectField(TEXT("RValuePropertyType"), FPropertyTypeHelper::SerializeGraphPinType(FieldPinType, SelfScope.Get()));
            Result->SetStringField(TEXT("RValuePropertyName"), Field->GetName());
        }

        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_IntConst: {
        int32 ConstValue = ReadInt(ScriptIndex);
        Result->SetStringField(TEXT("Inst"), TEXT("IntConst"));
        Result->SetNumberField(TEXT("Value"), ConstValue);
        break;
    }
    case EX_SkipOffsetConst: {
        CodeSkipSizeType ConstValue = ReadSkipCount(ScriptIndex);
        Result->SetStringField(TEXT("Inst"), TEXT("SkipOffsetConst"));
        Result->SetNumberField(TEXT("Value"), ConstValue);
        break;
    }
    case EX_FloatConst: {
        float ConstValue = ReadFloat(ScriptIndex);
        Result->SetStringField(TEXT("Inst"), TEXT("FloatConst"));
        Result->SetNumberField(TEXT("Value"), ConstValue);
        break;
    }
    case EX_StringConst: {
        FString ConstValue = ReadString8(ScriptIndex);
        Result->SetStringField(TEXT("Inst"), TEXT("StringConst"));
        Result->SetStringField(TEXT("Value"), ConstValue);
        break;
    }
    case EX_UnicodeStringConst: {
        FString ConstValue = ReadString16(ScriptIndex);
        Result->SetStringField(TEXT("Inst"), TEXT("UnicodeStringConst"));
        Result->SetStringField(TEXT("Value"), ConstValue);
        break;
    }
    case EX_TextConst: {
        Result->SetStringField(TEXT("Inst"), TEXT("TextConst"));
        // What kind of text are we dealing with?
        const EBlueprintTextLiteralType TextLiteralType = (EBlueprintTextLiteralType)Script[ScriptIndex++];

        switch (TextLiteralType) {
        case EBlueprintTextLiteralType::Empty: {
            Result->SetStringField(TEXT("TextLiteralType"), TEXT("Empty"));
            break;
        }
        case EBlueprintTextLiteralType::LocalizedText: {
            Result->SetStringField(TEXT("TextLiteralType"), TEXT("LocalizedText"));
            const FString SourceString = ReadString(ScriptIndex);
            const FString KeyString = ReadString(ScriptIndex);
            const FString Namespace = ReadString(ScriptIndex);
            Result->SetStringField(TEXT("SourceString"), SourceString);
            Result->SetStringField(TEXT("LocalizationKey"), KeyString);
            Result->SetStringField(TEXT("LocalizationNamespace"), Namespace);
            break;
        }
        case EBlueprintTextLiteralType::InvariantText: {
            Result->SetStringField(TEXT("TextLiteralType"), TEXT("InvariantText"));
            const FString SourceString = ReadString(ScriptIndex);
            Result->SetStringField(TEXT("SourceString"), SourceString);
            break;
        }

        case EBlueprintTextLiteralType::LiteralString: {
            Result->SetStringField(TEXT("TextLiteralType"), TEXT("LiteralString"));
            const FString SourceString = ReadString(ScriptIndex);
            Result->SetStringField(TEXT("SourceString"), SourceString);
            break;
        }
        case EBlueprintTextLiteralType::StringTableEntry: {
            Result->SetStringField(TEXT("TextLiteralType"), TEXT("StringTableEntry"));

            ReadPointer<UObject>(ScriptIndex); // String Table asset (if any)
            const FString TableIdString = ReadString(ScriptIndex);
            const FString KeyString = ReadString(ScriptIndex);

            Result->SetStringField(TEXT("TableId"), TableIdString);
            Result->SetStringField(TEXT("TableKey"), KeyString);
            break;
        }
        default:
        checkf(false, TEXT("Unknown EBlueprintTextLiteralType! Please update FKismetBytecodeDisassembler::ProcessCommon to handle this type of text."));
            break;
        }
        break;
    }
    case EX_ObjectConst: {
        Result->SetStringField(TEXT("Inst"), TEXT("ObjectConst"));
        UObject* Pointer = ReadPointer<UObject>(ScriptIndex);
        Result->SetStringField(TEXT("Object"), Pointer->GetPathName());
        break;
    }
    case EX_SoftObjectConst: {
        Result->SetStringField(TEXT("Inst"), TEXT("SoftObjectConst"));
        Result->SetObjectField(TEXT("Value"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_NameConst: {
        Result->SetStringField(TEXT("Inst"), TEXT("NameConst"));
        FString ConstValue = ReadName(ScriptIndex);
        Result->SetStringField(TEXT("Value"), ConstValue);
        break;
    }
    case EX_RotationConst: {
        Result->SetStringField(TEXT("Inst"), TEXT("RotationConst"));
        float Pitch = ReadFloat(ScriptIndex);
        float Yaw = ReadFloat(ScriptIndex);
        float Roll = ReadFloat(ScriptIndex);

        Result->SetNumberField(TEXT("Pitch"), Pitch);
        Result->SetNumberField(TEXT("Yaw"), Yaw);
        Result->SetNumberField(TEXT("Roll"), Roll);
        break;
    }
    case EX_VectorConst: {
        Result->SetStringField(TEXT("Inst"), TEXT("VectorConst"));
        float X = ReadFloat(ScriptIndex);
        float Y = ReadFloat(ScriptIndex);
        float Z = ReadFloat(ScriptIndex);

        Result->SetNumberField(TEXT("X"), X);
        Result->SetNumberField(TEXT("Y"), Y);
        Result->SetNumberField(TEXT("Z"), Z);
        break;
    }
    case EX_TransformConst: {
        Result->SetStringField(TEXT("Inst"), TEXT("TransformConst"));

        float RotX = ReadFloat(ScriptIndex);
        float RotY = ReadFloat(ScriptIndex);
        float RotZ = ReadFloat(ScriptIndex);
        float RotW = ReadFloat(ScriptIndex);

        float TransX = ReadFloat(ScriptIndex);
        float TransY = ReadFloat(ScriptIndex);
        float TransZ = ReadFloat(ScriptIndex);

        float ScaleX = ReadFloat(ScriptIndex);
        float ScaleY = ReadFloat(ScriptIndex);
        float ScaleZ = ReadFloat(ScriptIndex);

        TSharedPtr<FJsonObject> Rotation = MakeShareable(new FJsonObject());
        Rotation->SetNumberField(TEXT("X"), RotX);
        Rotation->SetNumberField(TEXT("Y"), RotY);
        Rotation->SetNumberField(TEXT("Z"), RotZ);
        Rotation->SetNumberField(TEXT("W"), RotW);
        Result->SetObjectField(TEXT("Rotation"), Rotation);

        TSharedPtr<FJsonObject> Translation = MakeShareable(new FJsonObject());
        Translation->SetNumberField(TEXT("X"), TransX);
        Translation->SetNumberField(TEXT("Y"), TransY);
        Translation->SetNumberField(TEXT("Z"), TransZ);
        Result->SetObjectField(TEXT("Translation"), Translation);

        TSharedPtr<FJsonObject> Scale = MakeShareable(new FJsonObject());
        Scale->SetNumberField(TEXT("X"), ScaleX);
        Scale->SetNumberField(TEXT("Y"), ScaleY);
        Scale->SetNumberField(TEXT("Z"), ScaleZ);
        Result->SetObjectField(TEXT("Scale"), Scale);

        break;
    }
    case EX_StructConst: {
        Result->SetStringField(TEXT("Inst"), TEXT("StructConst"));
        UScriptStruct* Struct = ReadPointer<UScriptStruct>(ScriptIndex);
        Result->SetStringField(TEXT("Struct"), Struct->GetPathName());

        ReadInt(ScriptIndex); // Skip serialized structure size (not particularly useful really)

        // TODO: Change this once structs/classes can be declared as explicitly editor only
        bool bIsEditorOnlyStruct = false;

        // Enumerate over structure properties and sort them by property names
        TSharedPtr<FJsonObject> Properties = MakeShareable(new FJsonObject());

        for (UProperty* StructProp = Struct->PropertyLink; StructProp; StructProp = StructProp->PropertyLinkNext) {
            // Skip transient and editor only properties, this needs to be synched with KismetCompilerVMBackend and ScriptCore
            if (StructProp->PropertyFlags & CPF_Transient || (!bIsEditorOnlyStruct && StructProp->PropertyFlags & CPF_EditorOnly)) {
                continue;
            }

            TArray<TSharedPtr<FJsonValue>> PropertyValue;
            for (int32 ArrayIter = 0; ArrayIter < StructProp->ArrayDim; ++ArrayIter) {
                TSharedPtr<FJsonObject> Value = SerializeExpression(ScriptIndex);
                PropertyValue.Add(MakeShareable(new FJsonValueObject(Value)));
            }
            Properties->SetArrayField(StructProp->GetName(), PropertyValue);
        }

        Result->SetObjectField(TEXT("Properties"), Properties);
        ScriptIndex++; // Skip over EX_EndStructConst
        break;
    }
    case EX_SetArray: {
        Result->SetStringField(TEXT("Inst"), TEXT("SetArray"));
        Result->SetObjectField(TEXT("LeftSideExpression"), SerializeExpression(ScriptIndex));

        TArray<TSharedPtr<FJsonValue>> Values;
        while (Script[ScriptIndex] != EX_EndArray) {
            TSharedPtr<FJsonObject> Value = SerializeExpression(ScriptIndex);
            Values.Add(MakeShareable(new FJsonValueObject(Value)));
        }
        ScriptIndex++; // Skip over EX_EndArray
        Result->SetArrayField(TEXT("Values"), Values);
        break;
    }
    case EX_ArrayConst: {
        UProperty* InnerProp = ReadPointer<UProperty>(ScriptIndex);
        FEdGraphPinType PropertyPinType;
        FPropertyTypeHelper::ConvertPropertyToPinType(InnerProp, PropertyPinType);

        Result->SetStringField(TEXT("Inst"), TEXT("ArrayConst"));
        Result->SetObjectField(TEXT("InnerProperty"), FPropertyTypeHelper::SerializeGraphPinType(PropertyPinType, SelfScope.Get()));

        TArray<TSharedPtr<FJsonValue>> Values;
        ReadInt(ScriptIndex); // Skip element amount

        while (Script[ScriptIndex] != EX_EndArrayConst) {
            TSharedPtr<FJsonObject> Expression = SerializeExpression(ScriptIndex);
            Values.Add(MakeShareable(new FJsonValueObject(Expression)));
        }
        ScriptIndex++; // Skip EX_EndArrayConst
        Result->SetArrayField(TEXT("Values"), Values);
        break;
    }
    case EX_ByteConst: {
        Result->SetStringField(TEXT("Inst"), TEXT("ByteConst"));
        uint8 ConstValue = ReadByte(ScriptIndex);
        Result->SetNumberField(TEXT("Value"), ConstValue);
        break;
    }
    case EX_IntConstByte: {
        Result->SetStringField(TEXT("Inst"), TEXT("IntConstByte"));
        int32 ConstValue = ReadByte(ScriptIndex);
        Result->SetNumberField(TEXT("Value"), ConstValue);
        break;
    }
    case EX_Int64Const: {
        Result->SetStringField(TEXT("Inst"), TEXT("Int64Const"));
        int64 ConstValue = ReadQword(ScriptIndex);
        Result->SetNumberField(TEXT("Value"), ConstValue);
        break;
    }
    case EX_UInt64Const: {
        Result->SetStringField(TEXT("Inst"), TEXT("UInt64Const"));
        uint64 ConstValue = ReadQword(ScriptIndex);
        Result->SetNumberField(TEXT("Value"), ConstValue);
        break;
    }
    case EX_FieldPathConst: {
        Result->SetStringField(TEXT("Inst"), TEXT("FieldPathConst"));
        const TSharedPtr<FJsonObject> InnerExpression = SerializeExpression(ScriptIndex);
        Result->SetObjectField(TEXT("Expression"), InnerExpression);
        break;
    }
    case EX_MetaCast: {
        // Cast of class object to another class object
        Result->SetStringField(TEXT("Inst"), TEXT("MetaCast"));
        UClass* Class = ReadPointer<UClass>(ScriptIndex);

        Result->SetStringField(TEXT("Class"), Class->GetPathName());
        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_DynamicCast: {
        // Cast of external object to provided class
        Result->SetStringField(TEXT("Inst"), TEXT("DynamicCast"));
        UClass* Class = ReadPointer<UClass>(ScriptIndex);

        Result->SetStringField(TEXT("Class"), Class->GetPathName());
        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_JumpIfNot: {
        Result->SetStringField(TEXT("Inst"), TEXT("JumpIfNot"));
        CodeSkipSizeType SkipCount = ReadSkipCount(ScriptIndex);

        Result->SetNumberField(TEXT("Offset"), SkipCount);
        Result->SetObjectField(TEXT("Condition"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_Assert: {
        Result->SetStringField(TEXT("Inst"), TEXT("Assert"));
        uint16 LineNumber = ReadWord(ScriptIndex);
        uint8 InDebugMode = ReadByte(ScriptIndex);

        Result->SetNumberField(TEXT("LineNumber"), LineNumber);
        Result->SetBoolField(TEXT("Debug"), bool(InDebugMode));
        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_InstanceDelegate: {
        Result->SetStringField(TEXT("Inst"), TEXT("InstanceDelegate"));
        // the name of the function assigned to the delegate.
        FString FuncName = ReadName(ScriptIndex);
        Result->SetStringField(TEXT("FunctionName"), FuncName);
        break;
    }
    case EX_AddMulticastDelegate: {
        Result->SetStringField(TEXT("Inst"), TEXT("AddMulticastDelegate"));
        Result->SetObjectField(TEXT("MulticastDelegate"), SerializeExpression(ScriptIndex));
        Result->SetObjectField(TEXT("Delegate"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_RemoveMulticastDelegate: {
        Result->SetStringField(TEXT("Inst"), TEXT("RemoveMulticastDelegate"));
        Result->SetObjectField(TEXT("MulticastDelegate"), SerializeExpression(ScriptIndex));
        Result->SetObjectField(TEXT("Delegate"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_ClearMulticastDelegate: {
        Result->SetStringField(TEXT("Inst"), TEXT("ClearMulticastDelegate"));
        Result->SetObjectField(TEXT("MulticastDelegate"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_BindDelegate: {
        Result->SetStringField(TEXT("Inst"), TEXT("BindDelegate"));
        // the name of the function assigned to the delegate.
        FString FuncName = ReadName(ScriptIndex);

        Result->SetStringField(TEXT("FunctionName"), FuncName);
        Result->SetObjectField(TEXT("Delegate"), SerializeExpression(ScriptIndex));
        Result->SetObjectField(TEXT("Object"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_PushExecutionFlow: {
        Result->SetStringField(TEXT("Inst"), TEXT("PushExecutionFlow"));
        CodeSkipSizeType SkipCount = ReadSkipCount(ScriptIndex);
        Result->SetNumberField(TEXT("Offset"), SkipCount);
        break;
    }
    case EX_PopExecutionFlow: {
        Result->SetStringField(TEXT("Inst"), TEXT("PopExecutionFlow"));
        break;
    }
    case EX_PopExecutionFlowIfNot: {
        Result->SetStringField(TEXT("Inst"), TEXT("PopExecutionFlowIfNot"));
        Result->SetObjectField(TEXT("Condition"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_Breakpoint: {
        Result->SetStringField(TEXT("Inst"), TEXT("Breakpoint"));
        break;
    }
    case EX_WireTracepoint: {
        Result->SetStringField(TEXT("Inst"), TEXT("WireTracepoint"));
        break;
    }
    case EX_InstrumentationEvent: {
        Result->SetStringField(TEXT("Inst"), TEXT("InstrumentationEvent"));
        const uint8 EventType = ReadByte(ScriptIndex);
        switch (EventType) {
        case EScriptInstrumentation::InlineEvent: {
            const FString EventName = ReadName(ScriptIndex);
            Result->SetStringField(TEXT("EventType"), TEXT("InlineEvent"));
            Result->SetStringField(TEXT("EventName"), EventName);
            break;
        }
        case EScriptInstrumentation::Stop: Result->SetStringField(TEXT("EventType"), TEXT("Stop"));
            break;
        case EScriptInstrumentation::PureNodeEntry: Result->SetStringField(TEXT("EventType"), TEXT("PureNodeEntry"));
            break;
        case EScriptInstrumentation::NodeDebugSite: Result->SetStringField(TEXT("EventType"), TEXT("NodeDebugSite"));
            break;
        case EScriptInstrumentation::NodeEntry: Result->SetStringField(TEXT("EventType"), TEXT("NodeEntry"));
            break;
        case EScriptInstrumentation::NodeExit: Result->SetStringField(TEXT("EventType"), TEXT("NodeExit"));
            break;
        case EScriptInstrumentation::PushState: Result->SetStringField(TEXT("EventType"), TEXT("PushState"));
            break;
        case EScriptInstrumentation::RestoreState: Result->SetStringField(TEXT("EventType"), TEXT("RestoreState"));
            break;
        case EScriptInstrumentation::ResetState: Result->SetStringField(TEXT("EventType"), TEXT("ResetState"));
            break;
        case EScriptInstrumentation::SuspendState: Result->SetStringField(TEXT("EventType"), TEXT("SuspendState"));
            break;
        case EScriptInstrumentation::PopState: Result->SetStringField(TEXT("EventType"), TEXT("PopState"));
            break;
        case EScriptInstrumentation::TunnelEndOfThread: Result->SetStringField(TEXT("EventType"), TEXT("TunnelEndOfThread"));
            break;
        default:
        checkf(0, TEXT("Unhandled instrumentation event type: %d"), EventType);
            break;
        }
        break;
    }
    case EX_Tracepoint: {
        Result->SetStringField(TEXT("Inst"), TEXT("Tracepoint"));
        break;
    }
    case EX_SwitchValue: {
        Result->SetStringField(TEXT("Inst"), TEXT("SwitchValue"));

        const uint16 NumCases = ReadWord(ScriptIndex);
        const CodeSkipSizeType AfterSkip = ReadSkipCount(ScriptIndex);

        Result->SetObjectField(TEXT("Expression"), SerializeExpression(ScriptIndex));
        Result->SetNumberField(TEXT("OffsetToSwitchEnd"), AfterSkip);

        TArray<TSharedPtr<FJsonValue>> Cases;
        for (uint16 CaseIndex = 0; CaseIndex < NumCases; ++CaseIndex) {
            TSharedPtr<FJsonObject> CaseObject = MakeShareable(new FJsonObject());
            CaseObject->SetObjectField(TEXT("CaseValue"), SerializeExpression(ScriptIndex));
            const CodeSkipSizeType OffsetToNextCase = ReadSkipCount(ScriptIndex);

            CaseObject->SetNumberField(TEXT("OffsetToNextCase"), OffsetToNextCase);
            CaseObject->SetObjectField(TEXT("CaseResult"), SerializeExpression(ScriptIndex));
            Cases.Add(MakeShareable(new FJsonValueObject(CaseObject)));
        }
        Result->SetArrayField(TEXT("Cases"), Cases);
        Result->SetObjectField(TEXT("DefaultResult"), SerializeExpression(ScriptIndex));
        break;
    }
    case EX_ArrayGetByRef: {
        Result->SetStringField(TEXT("Inst"), TEXT("ArrayGetByRef"));
        Result->SetObjectField(TEXT("ArrayExpression"), SerializeExpression(ScriptIndex));
        Result->SetObjectField(TEXT("IndexExpression"), SerializeExpression(ScriptIndex));
        break;
    }
    default: {
        // This should never occur.
        checkf(0, TEXT("Unknown bytecode 0x%02X"), (uint8) Opcode);
        break;
    }
    }
    // Make sure no instruction identifier is ever missing from returned json object
    check(Result->HasField(TEXT("Inst")));
    return Result;
}

TArray<TSharedPtr<FJsonValue>> FKismetBytecodeDisassemblerJson::SerializeFunction(UStruct* Function) {
    this->Script = Function->Script;
    this->SelfScope = Function->GetTypedOuter<UClass>();

    TArray<TSharedPtr<FJsonValue>> Statements;
    int32 ScriptIndex = 0;

    while (ScriptIndex < Script.Num()) {
        const int32 StatementIndex = ScriptIndex;
        TSharedPtr<FJsonObject> StatementObject = SerializeExpression(ScriptIndex);

        // Append statement index because several instructions can jump to statements (but not to separate expressions inside of statements!)
        StatementObject->SetNumberField(TEXT("StatementIndex"), StatementIndex);
        Statements.Add(MakeShareable(new FJsonValueObject(StatementObject)));
    }

    return Statements;
}

bool FKismetBytecodeDisassemblerJson::FindFirstStatementOfType(UStruct* Function, int32 StartScriptIndex, uint8 ExpectedStatementOpcode, int32& OutStatementIndex) {
    this->Script = Function->Script;
    this->SelfScope = Function->GetTypedOuter<UClass>();

    int32 ScriptIndex = StartScriptIndex;
    while (ScriptIndex < Script.Num()) {
        const int32 StatementIndex = ScriptIndex;
        const uint8 StatementOpcode = Script[ScriptIndex];
        FString ResultString;
        SerializeExpression(ScriptIndex);
        if (StatementOpcode == ExpectedStatementOpcode) {
            OutStatementIndex = StatementIndex;
            return true;
        }
    }
    // We haven't found any statement with matching opcode
    OutStatementIndex = -1;
    return false;
}

bool FKismetBytecodeDisassemblerJson::GetStatementLength(UStruct* Function, int32 ExpectedStatementIndex, int32& OutStatementLength) {
    this->Script = Function->Script;
    this->SelfScope = Function->GetTypedOuter<UClass>();

    int32 ScriptIndex = 0;
    while (ScriptIndex < Script.Num()) {
        const int32 StatementIndex = ScriptIndex;
        SerializeExpression(ScriptIndex);
        if (StatementIndex == ExpectedStatementIndex) {
            // This is the statement we are looking for, compute difference and return it as length
            OutStatementLength = ScriptIndex - StatementIndex;
            return true;
        }
    }
    // We haven't found a statement with expected index, so input is probably invalid
    OutStatementLength = -1;
    return false;
}

int32 FKismetBytecodeDisassemblerJson::ReadInt(int32& ScriptIndex) {
    int32 Value = Script[ScriptIndex];
    ++ScriptIndex;
    Value = Value | ((int32)Script[ScriptIndex] << 8);
    ++ScriptIndex;
    Value = Value | ((int32)Script[ScriptIndex] << 16);
    ++ScriptIndex;
    Value = Value | ((int32)Script[ScriptIndex] << 24);
    ++ScriptIndex;

    return Value;
}

uint64 FKismetBytecodeDisassemblerJson::ReadQword(int32& ScriptIndex) {
    uint64 Value = Script[ScriptIndex];
    ++ScriptIndex;
    Value = Value | ((uint64)Script[ScriptIndex] << 8);
    ++ScriptIndex;
    Value = Value | ((uint64)Script[ScriptIndex] << 16);
    ++ScriptIndex;
    Value = Value | ((uint64)Script[ScriptIndex] << 24);
    ++ScriptIndex;
    Value = Value | ((uint64)Script[ScriptIndex] << 32);
    ++ScriptIndex;
    Value = Value | ((uint64)Script[ScriptIndex] << 40);
    ++ScriptIndex;
    Value = Value | ((uint64)Script[ScriptIndex] << 48);
    ++ScriptIndex;
    Value = Value | ((uint64)Script[ScriptIndex] << 56);
    ++ScriptIndex;

    return Value;
}

uint8 FKismetBytecodeDisassemblerJson::ReadByte(int32& ScriptIndex) {
    uint8 Value = Script[ScriptIndex];
    ++ScriptIndex;

    return Value;
}

FString FKismetBytecodeDisassemblerJson::ReadName(int32& ScriptIndex) {
    const FScriptName ConstValue = *(FScriptName*)(Script.GetData() + ScriptIndex);
    ScriptIndex += sizeof(FScriptName);

    return ScriptNameToName(ConstValue).ToString();
}

uint16 FKismetBytecodeDisassemblerJson::ReadWord(int32& ScriptIndex) {
    uint16 Value = Script[ScriptIndex];
    ++ScriptIndex;
    Value = Value | ((uint16)Script[ScriptIndex] << 8);
    ++ScriptIndex;
    return Value;
}

float FKismetBytecodeDisassemblerJson::ReadFloat(int32& ScriptIndex) {
    union {
        float f;
        int32 i;
    } Result;
    Result.i = ReadInt(ScriptIndex);
    return Result.f;
}

CodeSkipSizeType FKismetBytecodeDisassemblerJson::ReadSkipCount(int32& ScriptIndex) {
#if SCRIPT_LIMIT_BYTECODE_TO_64KB
    return ReadWord(ScriptIndex);
#else
    static_assert(sizeof(CodeSkipSizeType) == 4, "Update this code as size changed.");
    return ReadInt(ScriptIndex);
#endif
}

FString FKismetBytecodeDisassemblerJson::ReadString8(int32& ScriptIndex) {
    FString Result;

    do {
        Result += (ANSICHAR)ReadByte(ScriptIndex);
    } while (Script[ScriptIndex - 1] != 0);

    return Result;
}

FString FKismetBytecodeDisassemblerJson::ReadString16(int32& ScriptIndex) {
    FString Result;

    do {
        Result += (TCHAR)ReadWord(ScriptIndex);
    } while ((Script[ScriptIndex - 1] != 0) || (Script[ScriptIndex - 2] != 0));

    return Result;
}

FString FKismetBytecodeDisassemblerJson::ReadString(int32& ScriptIndex) {
    const EExprToken Opcode = (EExprToken)Script[ScriptIndex++];

    switch (Opcode) {
    case EX_StringConst: return ReadString8(ScriptIndex);
    case EX_UnicodeStringConst: return ReadString16(ScriptIndex);
    default:
    checkf(false, TEXT("FKismetBytecodeDisassembler::ReadString - Unexpected opcode. Expected %d or %d, got %d"), (int)EX_StringConst, (int)EX_UnicodeStringConst, (int)Opcode);
        break;
    }

    return FString();
}
