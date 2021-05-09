#include "Toolkit/AssetTypes/AssetHelper.h"
#include "UObject/Class.h"
#include "FGCharacterPlayer.h"
#include "FGHealthComponent.h"
#include "FGRemoteCallObject.h"
#include "Toolkit/KismetBytecodeDisassemblerJson.h"
#include "Engine/Texture2D.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "AssemblyAnalyzer.h"

// Note: these offsets are for shipping windows x64 build, retrieved from IDA
// They are checked automatically to be correct when SML is launched,
// so if UObject layout changes it will crash at startup rather than crashing randomly in runtime
#define WINDOWS_SHIPPING_SERIALIZE_VTABLE_OFFSET 19
#define WINDOWS_SHIPPING_STRUCTURED_SERIALIZE_VTABLE_OFFSET 20

void FAssetHelper::RunStaticTests() {
    check(FAssetHelper::FindClassWithSerializeImplementation(UObject::StaticClass()) == UObject::StaticClass());
    check(FAssetHelper::FindClassWithSerializeImplementation(UTexture2D::StaticClass()) == UTexture2D::StaticClass());
    check(FAssetHelper::FindClassWithSerializeImplementation(UFGRemoteCallObject::StaticClass()) == UObject::StaticClass());
    check(FAssetHelper::FindClassWithSerializeImplementation(UFGHealthComponent::StaticClass()) == UActorComponent::StaticClass());
    check(FAssetHelper::FindClassWithSerializeImplementation(AFGCharacterPlayer::StaticClass()) == AActor::StaticClass());
}

UClass* FAssetHelper::FindClassWithSerializeImplementation(UClass* Class) {
    while (Class != UObject::StaticClass() && !IsSerializeImplementationDifferent(Class, Class->GetSuperClass())) {
        Class = Class->GetSuperClass();
    }
    return Class;
}

bool AreFunctionPointersEqual(uint64 FirstPointer, uint64 SecondPointer) {
    const FunctionInfo FunctionInfo1 = DiscoverFunction((uint8*)FirstPointer);
    const FunctionInfo FunctionInfo2 = DiscoverFunction((uint8*)SecondPointer);
    return FunctionInfo1.bIsValid && FunctionInfo2.bIsValid &&
        !FunctionInfo1.bIsVirtualFunction && !FunctionInfo2.bIsVirtualFunction &&
        FunctionInfo1.RealFunctionAddress == FunctionInfo2.RealFunctionAddress;
}

bool FAssetHelper::IsSerializeImplementationDifferent(UClass* Class1, UClass* Class2) {
#if PLATFORM_WINDOWS
#if !WITH_EDITOR
    uint64* UObjectVTablePtr = *((uint64**)Class1->GetDefaultObject());
    uint64* ClassObjectVTablePtr = *((uint64**)Class2->GetDefaultObject());

    const uint64 UObjectSerializeFunctionPtr = UObjectVTablePtr[WINDOWS_SHIPPING_SERIALIZE_VTABLE_OFFSET];
    const uint64 ClassSerializeFunctionPtr = ClassObjectVTablePtr[WINDOWS_SHIPPING_SERIALIZE_VTABLE_OFFSET];
    if (!AreFunctionPointersEqual(UObjectSerializeFunctionPtr, ClassSerializeFunctionPtr)) {
        return true;
    }

    const uint64 UObjectStructuredSerializeFunctionPtr = UObjectVTablePtr[WINDOWS_SHIPPING_STRUCTURED_SERIALIZE_VTABLE_OFFSET];
    const uint64 ClassStructuredSerializeFunctionPtr = ClassObjectVTablePtr[WINDOWS_SHIPPING_STRUCTURED_SERIALIZE_VTABLE_OFFSET];
    if (!AreFunctionPointersEqual(UObjectStructuredSerializeFunctionPtr, ClassStructuredSerializeFunctionPtr)) {
        return true;
    }

    return false;
#endif
    checkf(0, TEXT("IsSerializeImplementationDifferent not implemented for editor build"));
#endif
    checkf(0, TEXT("IsSerializeImplementationDifferent not implemented for non-windows platforms"));
    return false;
}

bool FAssetHelper::HasCustomSerializeOnStruct(UScriptStruct* Struct) {
    return (Struct->StructFlags & STRUCT_SerializeNative) != 0;
}

void FAssetHelper::SerializeClass(TSharedPtr<FJsonObject> OutObject, UClass* Class, UObjectHierarchySerializer* ObjectHierarchySerializer) {

    // Serialize native Struct class data first
    SerializeStruct(OutObject, Class, ObjectHierarchySerializer);

    // Skip serializing FuncMap as there is no point really
    // It can be regenerated on deserialization quickly

    // Save class flags (excluding those which should never get loaded)
    EClassFlags SavedClassFlags = Class->ClassFlags;
    SavedClassFlags &= ~CLASS_ShouldNeverBeLoaded;
    OutObject->SetStringField(TEXT("ClassFlags"), FString::Printf(TEXT("%d"), SavedClassFlags));

    // Class inside of which this class should always exist
    const int32 ClassWithinObjectIndex = ObjectHierarchySerializer->SerializeObject(Class->ClassWithin);
    OutObject->SetNumberField(TEXT("ClassWithin"), ClassWithinObjectIndex);

    // Class configuration name (e.g Engine for Engine.ini)
    OutObject->SetStringField(TEXT("ClassConfigName"), Class->ClassConfigName.ToString());

    // Skip ClassGeneratedBy - always NULL in shipping anyways

    // Serialize implemented interfaces
    TArray<TSharedPtr<FJsonValue>> ImplementedInterfaces;

    for (const FImplementedInterface& ImplementedInterface : Class->Interfaces) {
        TSharedPtr<FJsonObject> Interface = MakeShareable(new FJsonObject());
        const int32 ClassObjectIndex = ObjectHierarchySerializer->SerializeObject(ImplementedInterface.Class);
        Interface->SetNumberField(TEXT("Class"), ClassObjectIndex);
        Interface->SetNumberField(TEXT("PointerOffset"), ImplementedInterface.PointerOffset);
        Interface->SetBoolField(TEXT("bImplementedByK2"), ImplementedInterface.bImplementedByK2);
        ImplementedInterfaces.Add(MakeShareable(new FJsonValueObject(Interface)));
    }
    OutObject->SetArrayField(TEXT("Interfaces"), ImplementedInterfaces);

    // Skip bIsCooked - not really relevant, always cooked in shipping anyways

    // Serialize ClassDefaultObject - might be a bad idea actually
    // But it hopefully shouldn't reference any non-serializable properties
    const int32 ClassDefaultObjectIndex = ObjectHierarchySerializer->SerializeObject(Class->ClassDefaultObject);
    OutObject->SetNumberField(TEXT("ClassDefaultObject"), ClassDefaultObjectIndex);
}

void FAssetHelper::SerializeStruct(TSharedPtr<FJsonObject> OutObject, UStruct* Struct, UObjectHierarchySerializer* ObjectHierarchySerializer) {
    // Do not serialize UField parent object
    // It doesn't have anything special in it's Serialize anyway,
    // just support for legacy field serialization, which we don't need

    // Serialize SuperStruct first
    const int32 SuperStructIndex = ObjectHierarchySerializer->SerializeObject(Struct->GetSuperStruct());
    OutObject->SetNumberField(TEXT("SuperStruct"), SuperStructIndex);

    // Serialize Children now (these can only be UFunctions, properties are no longer UObjects)
    TArray<TSharedPtr<FJsonValue>> Children;
    UField* Child = Struct->Children;
    while (Child) {
        const TSharedPtr<FJsonObject> FieldObject = MakeShareable(new FJsonObject());

        if (Child->IsA<UFunction>()) {
            SerializeFunction(FieldObject, Cast<UFunction>(Child), ObjectHierarchySerializer);
        } else {
            checkf(0, TEXT("Unsupported Children object type: %s"), *Child->GetClass()->GetPathName());
        }

        Children.Add(MakeShareable(new FJsonValueObject(FieldObject)));
        Child = Child->Next;
    }
    OutObject->SetArrayField(TEXT("Children"), Children);

    // Serialize ChildProperties (these can only be FProperties, representing what old UProperties used to)
    TArray<TSharedPtr<FJsonValue>> ChildProperties;
    FField* ChildField = Struct->ChildProperties;
    while (ChildField) {
        const TSharedPtr<FJsonObject> FieldObject = MakeShareable(new FJsonObject());

        if (ChildField->IsA<FProperty>()) {
            SerializeProperty(FieldObject, Cast<FProperty>(ChildField), ObjectHierarchySerializer);
        } else {
            checkf(0, TEXT("Unsupported ChildProperties object type: %s"), *ChildField->GetClass()->GetName());
        }

        ChildProperties.Add(MakeShareable(new FJsonValueObject(FieldObject)));
        ChildField = ChildField->Next;
    }
    OutObject->SetArrayField(TEXT("ChildProperties"), ChildProperties);

    // Serialize script bytecode now
    // but only if we actually have some, since normal classes and script structs never have any
    if (Struct->Script.Num()) {
        FKismetBytecodeDisassemblerJson BytecodeDisassembler;
        const TArray<TSharedPtr<FJsonValue>> Statements = BytecodeDisassembler.SerializeFunction(Struct);
        OutObject->SetArrayField(TEXT("Script"), Statements);
    }
}

void FAssetHelper::SerializeScriptStruct(TSharedPtr<FJsonObject> OutObject, UScriptStruct* Struct, UObjectHierarchySerializer* ObjectHierarchySerializer) {

    // Serialize normal Struct data first
    SerializeStruct(OutObject, Struct, ObjectHierarchySerializer);

    // Serialize struct flags
    OutObject->SetStringField(TEXT("StructFlags"), FString::Printf(TEXT("%d"), Struct->StructFlags));
}

void FAssetHelper::SerializeProperty(TSharedPtr<FJsonObject> OutObject, FProperty* Property, UObjectHierarchySerializer* ObjectHierarchySerializer) {
    OutObject->SetStringField(TEXT("ObjectClass"), Property->GetClass()->GetName());
    OutObject->SetStringField(TEXT("ObjectName"), Property->GetName());

    // Serialize array dimensions
    OutObject->SetNumberField(TEXT("ArrayDim"), Property->ArrayDim);

    // Serialize property flags
    const EPropertyFlags SaveFlags = Property->PropertyFlags & ~CPF_ComputedFlags;
    OutObject->SetStringField(TEXT("PropertyFlags"), FString::Printf(TEXT("%llu"), SaveFlags));

    // Serialize RepNotify function name
    OutObject->SetStringField(TEXT("RepNotifyFunc"), Property->RepNotifyFunc.ToString());

    // Serialize additional data depending on property type
    if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property)) {
        // For object properties, we serialize object type
        const int32 ObjectClassIndex = ObjectHierarchySerializer->SerializeObject(ObjectProperty->PropertyClass);
        OutObject->SetNumberField(TEXT("PropertyClass"), ObjectClassIndex);

    } else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property)) {
        // For enum properties, we serialize Enum and UnderlyingProperty
        const int32 EnumObjectIndex = ObjectHierarchySerializer->SerializeObject(EnumProperty->GetEnum());
        OutObject->SetNumberField(TEXT("Enum"), EnumObjectIndex);

        const TSharedPtr<FJsonObject> UnderlyingProp = MakeShareable(new FJsonObject());
        SerializeProperty(UnderlyingProp, EnumProperty->GetUnderlyingProperty(), ObjectHierarchySerializer);
        OutObject->SetObjectField(TEXT("UnderlyingProp"), UnderlyingProp);

    } else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property)) {
        // Serialize structure type of this property
        const int32 StructObjectIndex = ObjectHierarchySerializer->SerializeObject(StructProperty->Struct);
        OutObject->SetNumberField(TEXT("Struct"), StructObjectIndex);

    } else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property)) {
        // Serialize inner property for array
        const TSharedPtr<FJsonObject> InnerProperty = MakeShareable(new FJsonObject());
        SerializeProperty(InnerProperty, ArrayProperty->Inner, ObjectHierarchySerializer);
        OutObject->SetObjectField(TEXT("Inner"), InnerProperty);

    } else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property)) {
        // Serialize bool property native type and size
        OutObject->SetNumberField(TEXT("BoolSize"), BoolProperty->ElementSize);
        OutObject->SetBoolField(TEXT("NativeBool"), BoolProperty->IsNativeBool());

    } else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property)) {
        // Serialize Enum for legacy enum properties (TByteAsEnum)
        const int32 EnumObjectIndex = ObjectHierarchySerializer->SerializeObject(ByteProperty->Enum);
        OutObject->SetNumberField(TEXT("Enum"), EnumObjectIndex);

    } else if (FClassProperty* ClassProperty = CastField<FClassProperty>(Property)) {
        // For class property, we need to serialize MetaClass
        const int32 MetaClassIndex = ObjectHierarchySerializer->SerializeObject(ClassProperty->MetaClass);
        OutObject->SetNumberField(TEXT("MetaClass"), MetaClassIndex);

    } else if (FDelegateProperty* DelegateProperty = CastField<FDelegateProperty>(Property)) {
        // For delegate properties, we need to serialize signature function
        // Since it will always be present in the Child array too, we serialize just it's name
        // and not actual full UFunction object
        OutObject->SetStringField(TEXT("SignatureFunction"), DelegateProperty->SignatureFunction->GetName());

    } else if (FInterfaceProperty* InterfaceProperty = CastField<FInterfaceProperty>(Property)) {
        // For interface properties, we serialize interface type class
        const int32 InterfaceClassIndex = ObjectHierarchySerializer->SerializeObject(InterfaceProperty->InterfaceClass);
        OutObject->SetNumberField(TEXT("InterfaceClass"), InterfaceClassIndex);

    } else if (FMapProperty* MapProperty = Cast<FMapProperty>(Property)) {
        // For map properties, we just serialize key property type and value property type
        const TSharedPtr<FJsonObject> KeyProperty = MakeShareable(new FJsonObject());
        SerializeProperty(KeyProperty, MapProperty->KeyProp, ObjectHierarchySerializer);
        OutObject->SetObjectField(TEXT("KeyProp"), KeyProperty);

        const TSharedPtr<FJsonObject> ValueProperty = MakeShareable(new FJsonObject());
        SerializeProperty(ValueProperty, MapProperty->ValueProp, ObjectHierarchySerializer);
        OutObject->SetObjectField(TEXT("ValueProp"), ValueProperty);

    } else if (FMulticastDelegateProperty* MulticastDelegateProperty = CastField<FMulticastDelegateProperty>(Property)) {
        // For multicast delegate properties, record signature function name
        OutObject->SetStringField(TEXT("SignatureFunction"), MulticastDelegateProperty->SignatureFunction->GetName());

    } else if (FSetProperty* SetProperty = CastField<FSetProperty>(Property)) {
        // For set properties, serialize element type
        const TSharedPtr<FJsonObject> ElementType = MakeShareable(new FJsonObject());
        SerializeProperty(ElementType, SetProperty->ElementProp, ObjectHierarchySerializer);
        OutObject->SetObjectField(TEXT("ElementType"), ElementType);

    } else if (FSoftClassProperty* SoftClassProperty = Cast<FSoftClassProperty>(Property)) {
        // For soft class property, we serialize MetaClass
        const int32 MetaClassIndex = ObjectHierarchySerializer->SerializeObject(SoftClassProperty->MetaClass);
        OutObject->SetNumberField(TEXT("MetaClass"), MetaClassIndex);

    } else if (FFieldPathProperty* FieldPathProperty = Cast<FFieldPathProperty>(Property)) {
        // Serialize field class
        OutObject->SetStringField(TEXT("PropertyClass"), FieldPathProperty->PropertyClass->GetName());

    } else {
        // Other property classes don't override Serialize, so do nothing
    }
}

void FAssetHelper::SerializeEnum(TSharedPtr<FJsonObject> OutObject, UEnum* Enum) {
    // Serialize Names map as objects
    TArray<TSharedPtr<FJsonValue>> Names;
    for (int32 i = 0; i < Enum->NumEnums(); i++) {
        TSharedPtr<FJsonObject> NameObject = MakeShareable(new FJsonObject());
        NameObject->SetNumberField(TEXT("Value"), Enum->GetValueByIndex(i));
        NameObject->SetStringField(TEXT("Name"), Enum->GetNameStringByIndex(i));
        Names.Add(MakeShareable(new FJsonValueObject(NameObject)));
    }
    OutObject->SetArrayField(TEXT("Names"), Names);

    // Serialize cppform as byte
    OutObject->SetNumberField(TEXT("CppForm"), (uint8)Enum->GetCppForm());
}

void FAssetHelper::SerializeFunction(TSharedPtr<FJsonObject> OutObject, UFunction* Function, UObjectHierarchySerializer* ObjectHierarchySerializer) {
    OutObject->SetStringField(TEXT("ObjectClass"), UFunction::StaticClass()->GetName());
    OutObject->SetStringField(TEXT("ObjectName"), Function->GetName());

    // Serialize super Struct data
    // It will also serialize script bytecode for function
    SerializeStruct(OutObject, Function, ObjectHierarchySerializer);

    // Save function flags
    const EFunctionFlags FunctionFlags = Function->FunctionFlags;
    OutObject->SetStringField(TEXT("FunctionFlags"), FString::Printf(TEXT("%d"), FunctionFlags));

    // Serialize event graph call information to aid with blueprint decompilation
    if (Function->EventGraphFunction) {
        OutObject->SetStringField(TEXT("EventGraphFunction"), Function->EventGraphFunction->GetName());
        OutObject->SetNumberField(TEXT("EventGraphCallOffset"), Function->EventGraphCallOffset);
    }
}

FString FAssetHelper::PropertyPathToString(const FCachedPropertyPath& Path) {
    FString OutString;
    UStruct* StructObject = FindObjectChecked<UStruct>(NULL, TEXT("/Script/PropertyPath.CachedPropertyPath"));
    FProperty* Property = StructObject->FindPropertyByName(TEXT("Segments"));
    check(Property != nullptr);
    const TArray<FPropertyPathSegment>* Segments = Property->ContainerPtrToValuePtr<const TArray<FPropertyPathSegment>>(&Path);

    for (int32 SegmentIndex = 0; SegmentIndex < Segments->Num(); ++SegmentIndex) {
        const FPropertyPathSegment& Segment = (*Segments)[SegmentIndex];

        // Add property name
        OutString += Segment.Name.ToString();

        // Add array index
        if (Segment.ArrayIndex != INDEX_NONE) {
            OutString += FString::Printf(TEXT("[%d]"), Segment.ArrayIndex);
        }

        // Add separator
        if (SegmentIndex < Segments->Num() - 1) {
            OutString += TEXT(".");
        }
    }

    return OutString;
}
