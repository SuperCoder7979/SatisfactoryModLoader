#include "Toolkit/PropertyTypeHandler.h"
#include "Engine/MemberReference.h"

const FName PC_Exec(TEXT("exec"));
const FName PC_Boolean(TEXT("bool"));
const FName PC_Byte(TEXT("byte"));
const FName PC_Class(TEXT("class"));
const FName PC_Int(TEXT("int"));
const FName PC_Int64(TEXT("int64"));
const FName PC_Float(TEXT("float"));
const FName PC_Name(TEXT("name"));
const FName PC_Delegate(TEXT("delegate"));
const FName PC_MCDelegate(TEXT("mcdelegate"));
const FName PC_Object(TEXT("object"));
const FName PC_Interface(TEXT("interface"));
const FName PC_String(TEXT("string"));
const FName PC_Text(TEXT("text"));
const FName PC_Struct(TEXT("struct"));
const FName PC_Wildcard(TEXT("wildcard"));
const FName PC_Enum(TEXT("enum"));
const FName PC_SoftObject(TEXT("softobject"));
const FName PC_SoftClass(TEXT("softclass"));
const FName PSC_Self(TEXT("self"));
const FName PSC_Index(TEXT("index"));
const FName PSC_Bitmask(TEXT("bitmask"));
const FName PN_Execute(TEXT("execute"));

UObject* DeserializeObjectRef(const FString& ObjectPath, UClass* SelfScope) {
    if (ObjectPath == TEXT("<SELF>")) {
        checkf(SelfScope, TEXT("Attempt to deserialize <SELF> object reference, but no scope was provided for self"));
        return SelfScope;

    }
    UObject* LoadedObject = LoadObject<UObject>(NULL, *ObjectPath);
    checkf(LoadedObject, TEXT("Failed to deserializeo object reference %s"), *ObjectPath);
    return LoadedObject;
}

FEdGraphPinType FPropertyTypeHelper::DeserializeGraphPinType(const TSharedRef<FJsonObject>& PinJson, UClass* SelfScope) {

    FEdGraphPinType GraphPinType;
    GraphPinType.PinCategory = *PinJson->GetStringField(TEXT("PinCategory"));
    GraphPinType.PinSubCategory = *PinJson->GetStringField(TEXT("PinSubCategory"));

    if (PinJson->HasField(TEXT("PinSubCategoryObject"))) {
        const FString& ObjectPath = PinJson->GetStringField(TEXT("PinSubCategoryObject"));
        GraphPinType.PinSubCategoryObject = DeserializeObjectRef(*ObjectPath, SelfScope);
    }

    if (PinJson->HasField(TEXT("PinSubCategoryMemberReference"))) {
        FSimpleMemberReference& MemberRef = GraphPinType.PinSubCategoryMemberReference;
        const TSharedPtr<FJsonObject>& MemberJson = PinJson->GetObjectField(TEXT("PinSubCategoryMemberReference"));
        if (MemberJson->HasField("MemberParent")) {
            const FString& ObjectPath = MemberJson->GetStringField(TEXT("MemberParent"));
            MemberRef.MemberParent = DeserializeObjectRef(*ObjectPath, SelfScope);
        }
        MemberRef.MemberName = *MemberJson->GetStringField(TEXT("MemberName"));
        FGuid::Parse(MemberJson->GetStringField(TEXT("MemberGuid")), MemberRef.MemberGuid);
    }

    if (PinJson->HasField(TEXT("PinValueType"))) {
        FEdGraphTerminalType& ValueType = GraphPinType.PinValueType;
        const TSharedPtr<FJsonObject> ValueJson = PinJson->GetObjectField(TEXT("PinValueType"));
        ValueType.TerminalCategory = *ValueJson->GetStringField(TEXT("TerminalCategory"));
        ValueType.TerminalSubCategory = *ValueJson->GetStringField(TEXT("TerminalSubCategory"));
        if (ValueJson->HasField(TEXT("TerminalSubCategoryObject"))) {
            const FString& ObjectPath = ValueJson->GetStringField(TEXT("TerminalSubCategoryObject"));
            ValueType.TerminalSubCategoryObject = DeserializeObjectRef(*ObjectPath, SelfScope);
        }
        ValueType.bTerminalIsConst = ValueJson->GetBoolField(TEXT("TerminalIsConst"));
        ValueType.bTerminalIsWeakPointer = ValueJson->GetBoolField(TEXT("TerminalIsWeakPointer"));
    }

    if (PinJson->HasField(TEXT("ContainerType"))) {
        GraphPinType.ContainerType = static_cast<EPinContainerType>(PinJson->GetIntegerField(TEXT("ContainerType")));
    }

    if (PinJson->HasField(TEXT("IsReference"))) {
        GraphPinType.bIsReference = PinJson->GetBoolField(TEXT("IsReference"));
    }

    if (PinJson->HasField(TEXT("IsConst"))) {
        GraphPinType.bIsConst = PinJson->GetBoolField(TEXT("IsConst"));
    }

    if (PinJson->HasField(TEXT("IsWeakPointer"))) {
        GraphPinType.bIsWeakPointer = PinJson->GetBoolField(TEXT("IsWeakPointer"));
    }

    return GraphPinType;
}

FString SerializeObjectRef(UObject* Object, UClass* SelfScope) {
    return SelfScope && Object == SelfScope ? TEXT("<SELF>") : Object->GetPathName();
}

TSharedRef<FJsonObject> FPropertyTypeHelper::SerializeGraphPinType(const FEdGraphPinType& GraphPinType, UClass* SelfScope) {

    TSharedRef<FJsonObject> TypeEntry = MakeShareable(new FJsonObject());
    TypeEntry->SetStringField(TEXT("PinCategory"), GraphPinType.PinCategory.ToString());
    TypeEntry->SetStringField(TEXT("PinSubCategory"), GraphPinType.PinCategory.ToString());

    UObject* SubCategoryObject = GraphPinType.PinSubCategoryObject.Get();
    if (SubCategoryObject != nullptr) {
        TypeEntry->SetStringField(TEXT("PinSubCategoryObject"), SerializeObjectRef(SubCategoryObject, SelfScope));
    }

    const FSimpleMemberReference& memberRef = GraphPinType.PinSubCategoryMemberReference;

    if (memberRef.MemberGuid.IsValid()) {
        TSharedRef<FJsonObject> memberReference = MakeShareable(new FJsonObject());
        if (memberRef.MemberParent != nullptr) {
            memberReference->SetStringField(TEXT("MemberParent"), SerializeObjectRef(memberRef.MemberParent, SelfScope));
        }
        memberReference->SetStringField(TEXT("MemberName"), memberRef.MemberName.ToString());
        memberReference->SetStringField(TEXT("MemberGuid"), memberRef.MemberGuid.ToString());
        TypeEntry->SetObjectField(TEXT("PinSubCategoryMemberReference"), memberReference);
    }

    if (GraphPinType.ContainerType == EPinContainerType::Map) {
        TSharedRef<FJsonObject> pinValueType = MakeShareable(new FJsonObject());
        pinValueType->SetStringField(TEXT("TerminalCategory"), GraphPinType.PinValueType.TerminalCategory.ToString());
        pinValueType->SetStringField(TEXT("TerminalSubCategory"), GraphPinType.PinValueType.TerminalSubCategory.ToString());
        UObject* terminalSubCategoryObject = GraphPinType.PinValueType.TerminalSubCategoryObject.Get();
        if (terminalSubCategoryObject != nullptr) {
            pinValueType->SetStringField(TEXT("TerminalSubCategoryObject"), SerializeObjectRef(terminalSubCategoryObject, SelfScope));
        }
        pinValueType->SetBoolField(TEXT("TerminalIsConst"), GraphPinType.PinValueType.bTerminalIsConst);
        pinValueType->SetBoolField(TEXT("TerminalIsWeakPointer"), GraphPinType.PinValueType.bTerminalIsWeakPointer);
        TypeEntry->SetObjectField(TEXT("PinValueType"), pinValueType);
    }

    if (GraphPinType.ContainerType != EPinContainerType::None) {
        TypeEntry->SetNumberField(TEXT("ContainerType"), static_cast<uint8>(GraphPinType.ContainerType));
    }

    if (GraphPinType.bIsReference) {
        TypeEntry->SetBoolField(TEXT("IsReference"), GraphPinType.bIsReference);
    }

    if (GraphPinType.bIsConst) {
        TypeEntry->SetBoolField(TEXT("IsConst"), GraphPinType.bIsConst);
    }

    if (GraphPinType.bIsWeakPointer) {
        TypeEntry->SetBoolField(TEXT("IsWeakPointer"), GraphPinType.bIsWeakPointer);
    }

    return TypeEntry;
}

bool GetPropertyCategoryInfo(const FProperty* TestProperty, FName& OutCategory, FName& OutSubCategory, UObject*& OutSubCategoryObject, bool& bOutIsWeakPointer);

bool FPropertyTypeHelper::ConvertPropertyToPinType(const FProperty* Property, /*out*/ FEdGraphPinType& TypeOut) {
    if (Property == nullptr) {
        TypeOut.PinCategory = TEXT("bad_type");
        return false;
    }

    TypeOut.PinSubCategory = NAME_None;

    // Handle whether or not this is an array property
    const FMapProperty* MapProperty = CastField<const FMapProperty>(Property);
    const FSetProperty* SetProperty = CastField<const FSetProperty>(Property);
    const FArrayProperty* ArrayProperty = CastField<const FArrayProperty>(Property);
    const FProperty* TestProperty = Property;
    if (MapProperty) {
        TestProperty = MapProperty->KeyProp;

        // set up value property:
        UObject* SubCategoryObject = nullptr;
        bool bIsWeakPtr = false;
        bool bResult = GetPropertyCategoryInfo(MapProperty->ValueProp, TypeOut.PinValueType.TerminalCategory, TypeOut.PinValueType.TerminalSubCategory, SubCategoryObject, bIsWeakPtr);
        TypeOut.PinValueType.TerminalSubCategoryObject = SubCategoryObject;

        if (bIsWeakPtr) {
            return false;
        }

        if (!bResult) {
            return false;
        }
    } else if (SetProperty) {
        TestProperty = SetProperty->ElementProp;
    } else if (ArrayProperty) {
        TestProperty = ArrayProperty->Inner;
    }
    TypeOut.ContainerType = FEdGraphPinType::ToPinContainerType(ArrayProperty != nullptr, SetProperty != nullptr, MapProperty != nullptr);
    TypeOut.bIsReference = Property->HasAllPropertyFlags(CPF_OutParm | CPF_ReferenceParm);
    TypeOut.bIsConst = Property->HasAllPropertyFlags(CPF_ConstParm);

    // Check to see if this is the wildcard property for the target container type
    if (const FMulticastDelegateProperty* MulticastDelegateProperty = CastField<const FMulticastDelegateProperty>(TestProperty)) {
        TypeOut.PinCategory = PC_MCDelegate;
        FMemberReference::FillSimpleMemberReference<UFunction>(MulticastDelegateProperty->SignatureFunction, TypeOut.PinSubCategoryMemberReference);
    } else if (const FDelegateProperty* DelegateProperty = CastField<const FDelegateProperty>(TestProperty)) {
        TypeOut.PinCategory = PC_Delegate;
        FMemberReference::FillSimpleMemberReference<UFunction>(DelegateProperty->SignatureFunction, TypeOut.PinSubCategoryMemberReference);
    } else {
        UObject* SubCategoryObject = nullptr;
        bool bIsWeakPointer = false;
        bool bResult = GetPropertyCategoryInfo(TestProperty, TypeOut.PinCategory, TypeOut.PinSubCategory, SubCategoryObject, bIsWeakPointer);
        TypeOut.bIsWeakPointer = bIsWeakPointer;
        TypeOut.PinSubCategoryObject = SubCategoryObject;
        if (!bResult) {
            return false;
        }
    }
    return true;
}

bool GetPropertyCategoryInfo(const FProperty* TestProperty, FName& OutCategory, FName& OutSubCategory, UObject*& OutSubCategoryObject, bool& bOutIsWeakPointer) {
    if (const FInterfaceProperty* InterfaceProperty = CastField<const FInterfaceProperty>(TestProperty)) {
        OutCategory = PC_Interface;
        OutSubCategoryObject = InterfaceProperty->InterfaceClass;
    } else if (const FClassProperty* ClassProperty = CastField<const FClassProperty>(TestProperty)) {
        OutCategory = PC_Class;
        OutSubCategoryObject = ClassProperty->MetaClass;
    } else if (const FSoftClassProperty* SoftClassProperty = CastField<const FSoftClassProperty>(TestProperty)) {
        OutCategory = PC_SoftClass;
        OutSubCategoryObject = SoftClassProperty->MetaClass;
    } else if (const FSoftObjectProperty* SoftObjectProperty = CastField<const FSoftObjectProperty>(TestProperty)) {
        OutCategory = PC_SoftObject;
        OutSubCategoryObject = SoftObjectProperty->PropertyClass;
    } else if (const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(TestProperty)) {
        OutCategory = PC_Object;
        OutSubCategoryObject = ObjectProperty->PropertyClass;
        bOutIsWeakPointer = TestProperty->IsA(FWeakObjectProperty::StaticClass());
    } else if (const FStructProperty* StructProperty = CastField<const FStructProperty>(TestProperty)) {
        OutCategory = PC_Struct;
        OutSubCategoryObject = StructProperty->Struct;
    } else if (TestProperty->IsA<FFloatProperty>()) {
        OutCategory = PC_Float;
    } else if (TestProperty->IsA<FInt64Property>()) {
        OutCategory = PC_Int64;
    } else if (TestProperty->IsA<FIntProperty>()) {
        OutCategory = PC_Int;
    } else if (const FByteProperty* ByteProperty = CastField<const FByteProperty>(TestProperty)) {
        OutCategory = PC_Byte;
        OutSubCategoryObject = ByteProperty->Enum;
    } else if (const FEnumProperty* EnumProperty = CastField<const FEnumProperty>(TestProperty)) {
        // K2 only supports byte enums right now - any violations should have been caught by UHT or the editor
        if (!EnumProperty->GetUnderlyingProperty()->IsA<FByteProperty>()) {
            OutCategory = TEXT("unsupported_enum_type");
            return false;
        }

        OutCategory = PC_Byte;
        OutSubCategoryObject = EnumProperty->GetEnum();

    } else if (TestProperty->IsA<FNameProperty>()) {
        OutCategory = PC_Name;
    } else if (TestProperty->IsA<FBoolProperty>()) {
        OutCategory = PC_Boolean;
    } else if (TestProperty->IsA<FStrProperty>()) {
        OutCategory = PC_String;
    } else if (TestProperty->IsA<FTextProperty>()) {
        OutCategory = PC_Text;
    } else {
        OutCategory = TEXT("bad_type");
        return false;
    }

    return true;
}
