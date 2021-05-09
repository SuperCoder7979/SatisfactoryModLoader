#include "Toolkit/PropertySerializer.h"

#include "Toolkit/ObjectHierarchySerializer.h"
#include "UObject/TextProperty.h"

DECLARE_LOG_CATEGORY_CLASS(LogPropertySerializer, Error, Log);

void UPropertySerializer::DeserializePropertyValue(FProperty* Property, const TSharedRef<FJsonValue>& JsonValue, void* Value) {
    // Use custom deserializer if it is available
    if (CustomPropertyDeserializers.Contains(Property)) {
        CustomPropertyDeserializers.FindChecked(Property)(Property, JsonValue, Value);
        return;
    }

    // Handle statically sized array properties
    if (Property->ArrayDim != 1) {
        const TArray<TSharedPtr<FJsonValue>>& ArrayElements = JsonValue->AsArray();
        for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ArrayIndex++) {
            uint8* ArrayPropertyValue = (uint8*)Value + Property->ElementSize * ArrayIndex;
            check(ArrayElements.IsValidIndex(ArrayIndex));
            const TSharedRef<FJsonValue> ArrayJsonValue = ArrayElements[ArrayIndex].ToSharedRef();
            DeserializePropertyValueInner(Property, ArrayJsonValue, ArrayPropertyValue);
        }
    } else {
        DeserializePropertyValueInner(Property, JsonValue, Value);
    }
}

void UPropertySerializer::DeserializePropertyValueInner(FProperty* Property, const TSharedRef<FJsonValue>& JsonValue, void* Value) {
    const FMapProperty* MapProperty = CastField<const FMapProperty>(Property);
    const FSetProperty* SetProperty = CastField<const FSetProperty>(Property);
    const FArrayProperty* ArrayProperty = CastField<const FArrayProperty>(Property);

    if (MapProperty) {
        FProperty* KeyProperty = MapProperty->KeyProp;
        FProperty* ValueProperty = MapProperty->ValueProp;
        FScriptMapHelper MapHelper(MapProperty, Value);
        const TArray<TSharedPtr<FJsonValue>>& PairArray = JsonValue->AsArray();

        for (int32 i = 0; i < PairArray.Num(); i++) {
            const TSharedPtr<FJsonObject>& Pair = PairArray[i]->AsObject();
            const TSharedPtr<FJsonValue>& EntryKey = Pair->Values.FindChecked(TEXT("Key"));
            const TSharedPtr<FJsonValue>& EntryValue = Pair->Values.FindChecked(TEXT("Value"));
            const int32 Index = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
            uint8* PairPtr = MapHelper.GetPairPtr(Index);
            // Copy over imported key and value from temporary storage
            DeserializePropertyValue(KeyProperty, EntryKey.ToSharedRef(), PairPtr);
            DeserializePropertyValue(ValueProperty, EntryValue.ToSharedRef(), PairPtr + MapHelper.MapLayout.ValueOffset);
        }
        MapHelper.Rehash();

    } else if (SetProperty) {
        FProperty* ElementProperty = SetProperty->ElementProp;
        FScriptSetHelper SetHelper(SetProperty, Value);
        const TArray<TSharedPtr<FJsonValue>>& SetArray = JsonValue->AsArray();
        SetHelper.EmptyElements();
        uint8* TempElementStorage = static_cast<uint8*>(FMemory::Malloc(ElementProperty->ElementSize));
        ElementProperty->InitializeValue(TempElementStorage);

        for (int32 i = 0; i < SetArray.Num(); i++) {
            const TSharedPtr<FJsonValue>& Element = SetArray[i];
            DeserializePropertyValue(ElementProperty, Element.ToSharedRef(), TempElementStorage);

            const int32 NewElementIndex = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
            uint8* NewElementPtr = SetHelper.GetElementPtr(NewElementIndex);

            // Copy over imported key from temporary storage
            ElementProperty->CopyCompleteValue_InContainer(NewElementPtr, TempElementStorage);
        }
        SetHelper.Rehash();

        ElementProperty->DestroyValue(TempElementStorage);
        FMemory::Free(TempElementStorage);

    } else if (ArrayProperty) {
        FProperty* ElementProperty = ArrayProperty->Inner;
        FScriptArrayHelper ArrayHelper(ArrayProperty, Value);
        const TArray<TSharedPtr<FJsonValue>>& SetArray = JsonValue->AsArray();
        ArrayHelper.EmptyValues();

        for (int32 i = 0; i < SetArray.Num(); i++) {
            const TSharedPtr<FJsonValue>& Element = SetArray[i];
            const uint32 AddedIndex = ArrayHelper.AddValue();
            uint8* ValuePtr = ArrayHelper.GetRawPtr(AddedIndex);
            DeserializePropertyValue(ElementProperty, Element.ToSharedRef(), ValuePtr);
        }

    } else if (Property->IsA<FMulticastDelegateProperty>()) {
        FMulticastScriptDelegate* MulticastScriptDelegate = (FMulticastScriptDelegate*)Value;
        const TArray<TSharedPtr<FJsonValue>>& DelegatesArray = JsonValue->AsArray();

        if (ObjectHierarchySerializer != NULL) {
            for (int32 i = 0; i < DelegatesArray.Num(); i++) {
                const TSharedPtr<FJsonObject>& Element = DelegatesArray[i]->AsObject();
                FScriptDelegate ScriptDelegate;

                UObject* Object = ObjectHierarchySerializer->DeserializeObject(Element->GetIntegerField(TEXT("Object")));
                const FString FunctionName = Element->GetStringField(TEXT("FunctionName"));
                ScriptDelegate.BindUFunction(Object, *FunctionName);

                MulticastScriptDelegate->Add(ScriptDelegate);
            }
        }

    } else if (Property->IsA<FDelegateProperty>()) {
        FScriptDelegate* ScriptDelegate = (FScriptDelegate*)Value;
        TSharedPtr<FJsonObject> Element = JsonValue->AsObject();

        if (ObjectHierarchySerializer != NULL) {
            UObject* Object = ObjectHierarchySerializer->DeserializeObject(Element->GetIntegerField(TEXT("Object")));
            const FString FunctionName = Element->GetStringField(TEXT("FunctionName"));

            ScriptDelegate->BindUFunction(Object, *FunctionName);
        }

    } else if (const FInterfaceProperty* InterfaceProperty = CastField<const FInterfaceProperty>(Property)) {
        // UObject is enough to re-create value, since we known property on deserialization
        FScriptInterface* Interface = static_cast<FScriptInterface*>(Value);
        UObject* Object = ObjectHierarchySerializer ? ObjectHierarchySerializer->DeserializeObject((int32)JsonValue->AsNumber()) : NULL;
        if (Object != nullptr) {
            void* InterfacePtr = Object->GetInterfaceAddress(InterfaceProperty->InterfaceClass);
            check(InterfacePtr != nullptr);
            Interface->SetObject(Object);
            Interface->SetInterface(InterfacePtr);
        }

    } else if (const FClassProperty* ClassProperty = CastField<const FClassProperty>(Property)) {
        // For class it's enough just to have it's path name for deserialization
        const FString PathName = JsonValue->AsString();
        if (PathName != TEXT("None")) {
            UClass* ClassObject = LoadObject<UClass>(nullptr, *PathName);
            check(ClassObject != nullptr && ClassObject->IsChildOf(ClassProperty->MetaClass));
            ClassProperty->SetObjectPropertyValue(Value, ClassObject);
        }

    } else if (Property->IsA<FSoftObjectProperty>()) {
        // For soft object reference, path is enough too for deserialization.
        const FString PathString = JsonValue->AsString();
        FSoftObjectPtr* ObjectPtr = static_cast<FSoftObjectPtr*>(Value);
        *ObjectPtr = FSoftObjectPath(PathString);

    } else if (const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(Property)) {
        // Need to serialize full UObject for object property
        UObject* Object = ObjectHierarchySerializer ? ObjectHierarchySerializer->DeserializeObject((int32)JsonValue->AsNumber()) : NULL;
        ObjectProperty->SetObjectPropertyValue(Value, Object);

    } else if (const FStructProperty* StructProperty = CastField<const FStructProperty>(Property)) {
        // To serialize struct, we need it's type and value pointer, because struct value doesn't contain type information
        DeserializeStruct(StructProperty->Struct, JsonValue->AsObject().ToSharedRef(), Value);

    } else if (const FByteProperty* ByteProperty = CastField<const FByteProperty>(Property)) {
        // If we have a string provided, make sure Enum is not null
        if (JsonValue->Type == EJson::String) {
            check(ByteProperty->Enum);
            const int64 EnumerationValue = ByteProperty->Enum->GetValueByNameString(JsonValue->AsString());
            ByteProperty->SetIntPropertyValue(Value, EnumerationValue);
        } else {
            // Should be a number, set property value accordingly
            const int64 NumberValue = (int64)JsonValue->AsNumber();
            ByteProperty->SetIntPropertyValue(Value, NumberValue);
        }
        // Primitives below, they are serialized as plain json values
    } else if (const FNumericProperty* NumberProperty = CastField<const FNumericProperty>(Property)) {
        const double NumberValue = JsonValue->AsNumber();
        if (NumberProperty->IsFloatingPoint()) NumberProperty->SetFloatingPointPropertyValue(Value, NumberValue);
        else NumberProperty->SetIntPropertyValue(Value, static_cast<int64>(NumberValue));

    } else if (const FBoolProperty* BoolProperty = CastField<const FBoolProperty>(Property)) {
        const bool bBooleanValue = JsonValue->AsBool();
        BoolProperty->SetPropertyValue(Value, bBooleanValue);

    } else if (Property->IsA<FStrProperty>()) {
        const FString StringValue = JsonValue->AsString();
        *static_cast<FString*>(Value) = StringValue;

    } else if (const FEnumProperty* EnumProperty = CastField<const FEnumProperty>(Property)) {
        // K2 only supports byte enums right now - any violations should have been caught by UHT or the editor
        if (!EnumProperty->GetUnderlyingProperty()->IsA<FByteProperty>()) {
            UE_LOG(LogPropertySerializer, Fatal, TEXT("Unsupported Underlying Enum Property Found: %s"), *EnumProperty->GetUnderlyingProperty()->GetClass()->GetName());
        }
        // Prefer readable enum names in result json to raw numbers
        const FString EnumName = JsonValue->AsString();
        const int64 UnderlyingValue = EnumProperty->GetEnum()->GetValueByNameString(EnumName);
        check(UnderlyingValue != INDEX_NONE);
        *static_cast<uint8*>(Value) = static_cast<uint8>(UnderlyingValue);

    } else if (Property->IsA<FNameProperty>()) {
        // Name is perfectly representable as string
        const FString NameString = JsonValue->AsString();
        *static_cast<FName*>(Value) = *NameString;

    } else if (const FTextProperty* TextProperty = CastField<const FTextProperty>(Property)) {
        // For FText, standard ExportTextItem is okay to use, because it's serialization is quite complex
        const FString SerializedValue = JsonValue->AsString();
        if (!SerializedValue.IsEmpty()) {
            FTextStringHelper::ReadFromBuffer(*SerializedValue, *static_cast<FText*>(Value));
        }
    } else if (const FFieldPathProperty* FieldPathProperty = CastField<const FFieldPathProperty>(Property)) {
        FFieldPath FieldPath;
        FieldPath.Generate(*JsonValue->AsString());
        *static_cast<FFieldPath*>(Value) = FieldPath;
    } else {
        UE_LOG(LogPropertySerializer, Fatal, TEXT("Found unsupported property type when deserializing value: %s"), *Property->GetClass()->GetName());
    }
}

void UPropertySerializer::DisablePropertySerialization(UStruct* Struct, FName PropertyName) {
    FProperty* Property = Struct->FindPropertyByName(PropertyName);
    check(Property);
    this->BlacklistedProperties.Add(Property);
}

void UPropertySerializer::SetCustomSerializer(UStruct* Struct, FName PropertyName, FPropertySerializer Serializer) {
    FProperty* Property = Struct->FindPropertyByName(PropertyName);
    check(Property);
    this->PinnedStructs.Add(Struct);
    this->CustomPropertySerializers.Add(Property, Serializer);
}

void UPropertySerializer::SetCustomDeserializer(UStruct* Struct, FName PropertyName, FPropertyDeserializer Deserializer) {
    FProperty* Property = Struct->FindPropertyByName(PropertyName);
    check(Property);
    this->PinnedStructs.Add(Struct);
    this->CustomPropertyDeserializers.Add(Property, Deserializer);
}

bool UPropertySerializer::ShouldSerializeProperty(FProperty* Property) const {
    // skip transient properties
    if (Property->HasAnyPropertyFlags(CPF_Transient)) {
        return false;
    }
    // Skip editor only properties altogether
    if (Property->HasAnyPropertyFlags(CPF_EditorOnly)) {
        return false;
    }
    // Skip deprecated properties
    if (Property->HasAnyPropertyFlags(CPF_Deprecated)) {
        return false;
    }
    if (BlacklistedProperties.Contains(Property)) {
        return false;
    }
    return true;
}

TSharedRef<FJsonValue> UPropertySerializer::SerializePropertyValue(FProperty* Property, const void* Value) {
    if (CustomPropertySerializers.Contains(Property)) {
        // Use custom property serializer when it is available
        return CustomPropertySerializers.FindChecked(Property)(Property, Value);
    }

    // Serialize statically sized array properties
    if (Property->ArrayDim != 1) {
        TArray<TSharedPtr<FJsonValue>> OutJsonValueArray;
        for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ArrayIndex++) {
            const uint8* ArrayPropertyValue = (const uint8*)Value + Property->ElementSize * ArrayIndex;
            const TSharedRef<FJsonValue> ElementValue = SerializePropertyValueInner(Property, ArrayPropertyValue);
            OutJsonValueArray.Add(ElementValue);
        }
        return MakeShareable(new FJsonValueArray(OutJsonValueArray));
    } else {
        return SerializePropertyValueInner(Property, Value);
    }
}

TSharedRef<FJsonValue> UPropertySerializer::SerializePropertyValueInner(FProperty* Property, const void* Value) {
    const FMapProperty* MapProperty = CastField<const FMapProperty>(Property);
    const FSetProperty* SetProperty = CastField<const FSetProperty>(Property);
    const FArrayProperty* ArrayProperty = CastField<const FArrayProperty>(Property);

    if (MapProperty) {
        FProperty* KeyProperty = MapProperty->KeyProp;
        FProperty* ValueProperty = MapProperty->ValueProp;
        FScriptMapHelper MapHelper(MapProperty, Value);
        TArray<TSharedPtr<FJsonValue>> ResultArray;
        for (int32 i = 0; i < MapHelper.Num(); i++) {
            TSharedPtr<FJsonValue> EntryKey = SerializePropertyValue(KeyProperty, MapHelper.GetKeyPtr(i));
            TSharedPtr<FJsonValue> EntryValue = SerializePropertyValue(ValueProperty, MapHelper.GetValuePtr(i));
            TSharedRef<FJsonObject> Pair = MakeShareable(new FJsonObject());
            Pair->SetField(TEXT("Key"), EntryKey);
            Pair->SetField(TEXT("Value"), EntryValue);
            ResultArray.Add(MakeShareable(new FJsonValueObject(Pair)));
        }
        return MakeShareable(new FJsonValueArray(ResultArray));
    }

    if (SetProperty) {
        FProperty* ElementProperty = SetProperty->ElementProp;
        FScriptSetHelper SetHelper(SetProperty, Value);
        TArray<TSharedPtr<FJsonValue>> ResultArray;
        for (int32 i = 0; i < SetHelper.Num(); i++) {
            TSharedPtr<FJsonValue> Element = SerializePropertyValue(ElementProperty, SetHelper.GetElementPtr(i));
            ResultArray.Add(Element);
        }
        return MakeShareable(new FJsonValueArray(ResultArray));
    }

    if (ArrayProperty) {
        FProperty* ElementProperty = ArrayProperty->Inner;
        FScriptArrayHelper ArrayHelper(ArrayProperty, Value);
        TArray<TSharedPtr<FJsonValue>> ResultArray;
        for (int32 i = 0; i < ArrayHelper.Num(); i++) {
            TSharedPtr<FJsonValue> Element = SerializePropertyValue(ElementProperty, ArrayHelper.GetRawPtr(i));
            ResultArray.Add(Element);
        }
        return MakeShareable(new FJsonValueArray(ResultArray));
    }

    if (Property->IsA<FMulticastDelegateProperty>()) {
        FMulticastScriptDelegate* MulticastScriptDelegate = (FMulticastScriptDelegate*)Value;
        TArray<TSharedPtr<FJsonValue>> DelegatesArray;

        if (ObjectHierarchySerializer != NULL) {
            for (FScriptDelegate& ScriptDelegate : MulticastScriptDelegate->InvocationList) {
                TSharedPtr<FJsonObject> DelegateObject = MakeShareable(new FJsonObject());

                UObject* Object = ScriptDelegate.GetUObject();
                DelegateObject->SetNumberField(TEXT("Object"), ObjectHierarchySerializer->SerializeObject(Object));
                DelegateObject->SetStringField(TEXT("FunctionName"), ScriptDelegate.GetFunctionName().ToString());

                DelegatesArray.Add(MakeShareable(new FJsonValueObject(DelegateObject)));
            }
        }

        return MakeShareable(new FJsonValueArray(DelegatesArray));
    }

    if (Property->IsA<FDelegateProperty>()) {
        FScriptDelegate* ScriptDelegate = (FScriptDelegate*)Value;
        TSharedPtr<FJsonObject> DelegateObject = MakeShareable(new FJsonObject());

        if (ObjectHierarchySerializer != NULL) {
            if (ScriptDelegate->IsBound()) {
                UObject* Object = ScriptDelegate->GetUObject();
                DelegateObject->SetNumberField(TEXT("Object"), ObjectHierarchySerializer->SerializeObject(Object));
                DelegateObject->SetStringField(TEXT("FunctionName"), ScriptDelegate->GetFunctionName().ToString());
            }
        }

        return MakeShareable(new FJsonValueObject(DelegateObject));
    }

    if (Property->IsA<FInterfaceProperty>()) {
        // UObject is enough to re-create value, since we known property on deserialization
        const FScriptInterface* Interface = reinterpret_cast<const FScriptInterface*>(Value);
        int32 ObjectIndex = ObjectHierarchySerializer ? ObjectHierarchySerializer->SerializeObject(Interface->GetObject()) : 0;
        return MakeShareable(new FJsonValueNumber(ObjectIndex));
    }

    if (const FClassProperty* ClassProperty = CastField<const FClassProperty>(Property)) {
        UClass* ClassObject = Cast<UClass>(ClassProperty->GetObjectPropertyValue(Value));
        // For class it's enough just to have it's path name for deserialization
        return MakeShareable(new FJsonValueString(ClassObject->GetPathName()));
    }

    if (Property->IsA<FSoftObjectProperty>()) {
        // For soft object reference, path is enough too for deserialization.
        const FSoftObjectPtr* ObjectPtr = reinterpret_cast<const FSoftObjectPtr*>(Value);
        return MakeShareable(new FJsonValueString(ObjectPtr->ToSoftObjectPath().ToString()));
    }

    if (const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(Property)) {
        // Need to serialize full UObject for object property
        UObject* ObjectPointer = ObjectProperty->GetObjectPropertyValue(Value);
        int32 ObjectIndex = ObjectHierarchySerializer ? ObjectHierarchySerializer->SerializeObject(ObjectPointer) : 0;
        return MakeShareable(new FJsonValueNumber(ObjectIndex));
    }

    if (const FStructProperty* StructProperty = CastField<const FStructProperty>(Property)) {
        // To serialize struct, we need it's type and value pointer, because struct value doesn't contain type information
        return MakeShareable(new FJsonValueObject(SerializeStruct(StructProperty->Struct, Value)));
    }

    if (const FByteProperty* ByteProperty = CastField<const FByteProperty>(Property)) {
        // If Enum is NULL, property will be handled as standard UNumericProperty
        if (ByteProperty->Enum) {
            const int64 UnderlyingValue = ByteProperty->GetSignedIntPropertyValue(Value);
            const FString EnumName = ByteProperty->Enum->GetNameByValue(UnderlyingValue).ToString();
            return MakeShareable(new FJsonValueString(EnumName));
        }
    }

    if (const FNumericProperty* NumberProperty = CastField<const FNumericProperty>(Property)) {
        double ResultValue;
        if (NumberProperty->IsFloatingPoint()) ResultValue = NumberProperty->GetFloatingPointPropertyValue(Value);
        else ResultValue = NumberProperty->GetSignedIntPropertyValue(Value);
        return MakeShareable(new FJsonValueNumber(ResultValue));
    }

    if (const FBoolProperty* BoolProperty = CastField<const FBoolProperty>(Property)) {
        const bool bBooleanValue = BoolProperty->GetPropertyValue(Value);
        return MakeShareable(new FJsonValueBoolean(bBooleanValue));
    }

    if (Property->IsA<FStrProperty>()) {
        const FString& StringValue = *reinterpret_cast<const FString*>(Value);
        return MakeShareable(new FJsonValueString(StringValue));
    }

    if (const FEnumProperty* EnumProperty = CastField<const FEnumProperty>(Property)) {
        const int64 UnderlyingValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value);
        const FString EnumName = EnumProperty->GetEnum()->GetNameByValue(UnderlyingValue).ToString();
        return MakeShareable(new FJsonValueString(EnumName));
    }

    if (Property->IsA<FNameProperty>()) {
        // Name is perfectly representable as string
        FName* Temp = ((FName*)Value);
        return MakeShareable(new FJsonValueString(Temp->ToString()));
    }

    if (const FTextProperty* TextProperty = CastField<const FTextProperty>(Property)) {
        FString ResultValue;
        const FText& TextValue = TextProperty->GetPropertyValue(Value);
        FTextStringHelper::WriteToBuffer(ResultValue, TextValue);
        return MakeShareable(new FJsonValueString(ResultValue));
    }

    if (Property->IsA<FFieldPathProperty>()) {
        FFieldPath* Temp = ((FFieldPath*)Value);
        return MakeShareable(new FJsonValueString(Temp->ToString()));
    }

    UE_LOG(LogPropertySerializer, Fatal, TEXT("Found unsupported property type when serializing value: %s"), *Property->GetClass()->GetName());
    return MakeShareable(new FJsonValueString(TEXT("#ERROR#")));
}

TSharedRef<FJsonObject> UPropertySerializer::SerializeStruct(UScriptStruct* Struct, const void* Value) {
    // checkf((Struct->StructFlags & EStructFlags::STRUCT_SerializeNative) == 0, TEXT("Attempt to serialize struct with native Serialize: %s"), *Struct->GetPathName());
    TSharedRef<FJsonObject> Properties = MakeShareable(new FJsonObject());
    for (FProperty* Property = Struct->PropertyLink; Property; Property = Property->PropertyLinkNext) {
        if (ObjectHierarchySerializer == NULL || ShouldSerializeProperty(Property)) {
            const void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Value);
            TSharedRef<FJsonValue> PropertyValueJson = SerializePropertyValue(Property, PropertyValue);
            Properties->SetField(Property->GetName(), PropertyValueJson);
        }
    }
    return Properties;
}

void UPropertySerializer::DeserializeStruct(UScriptStruct* Struct, const TSharedRef<FJsonObject>& Properties, void* OutValue) {
    for (FProperty* Property = Struct->PropertyLink; Property; Property = Property->PropertyLinkNext) {
        const FString PropertyName = Property->GetName();
        if ((ObjectHierarchySerializer == NULL || ShouldSerializeProperty(Property)) && Properties->HasField(PropertyName)) {
            void* PropertyValue = Property->ContainerPtrToValuePtr<void>(OutValue);
            const TSharedPtr<FJsonValue>& ValueObject = Properties->Values.FindChecked(PropertyName);
            if (ValueObject.IsValid()) {
                DeserializePropertyValue(Property, ValueObject.ToSharedRef(), PropertyValue);
            }
        }
    }
}
