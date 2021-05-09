#include "Reflection/BlueprintReflectedObject.h"
#include "UObject/TextProperty.h"

EReflectedPropertyType DeterminePropertyType(FProperty* Property);

FReflectedObjectState_Array::FReflectedObjectState_Array(const TSharedPtr<FReflectedObjectState> OwnerObject, const FName ArrayPropertyName) {
    this->OwnerObjectState = OwnerObject;
    this->ArrayPropertyName = ArrayPropertyName;
}

int32 FReflectedObjectState_Array::GetArrayNum() const {
    FArrayProperty* ArrayProperty = Cast<FArrayProperty>(OwnerObjectState->FindPropertyByName(ArrayPropertyName));
    if (ArrayProperty && ArrayProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)) {
        void* PropertyValue = OwnerObjectState->GetPropertyValue(ArrayPropertyName);
        const FScriptArrayHelper ArrayHelper{ArrayProperty, PropertyValue};
        return ArrayHelper.Num();
    }
    return 0;
}

int32 FReflectedObjectState_Array::AddNewArrayElement() const {
    FArrayProperty* ArrayProperty = Cast<FArrayProperty>(OwnerObjectState->FindPropertyByName(ArrayPropertyName));
    if (ArrayProperty && ArrayProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)) {
        void* PropertyValue = OwnerObjectState->GetPropertyValue(ArrayPropertyName);
        FScriptArrayHelper ArrayHelper{ArrayProperty, PropertyValue};
        return ArrayHelper.AddValue();
    }
    return -1;
}

void FReflectedObjectState_Array::RemoveArrayElements(int32 Index, int32 Count) const {
    FArrayProperty* ArrayProperty = Cast<FArrayProperty>(OwnerObjectState->FindPropertyByName(ArrayPropertyName));
    if (ArrayProperty && ArrayProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)) {
        void* PropertyValue = OwnerObjectState->GetPropertyValue(ArrayPropertyName);
        FScriptArrayHelper ArrayHelper{ArrayProperty, PropertyValue};
        if (ArrayHelper.IsValidIndex(Index) && ArrayHelper.IsValidIndex(Index + Count - 1)) {
            ArrayHelper.RemoveValues(Index, Count);
        }
    }
}

TArray<FReflectedPropertyInfo> FReflectedObjectState_Array::GetAllProperties() const {
    FArrayProperty* ArrayProperty = Cast<FArrayProperty>(OwnerObjectState->FindPropertyByName(ArrayPropertyName));
    TArray<FReflectedPropertyInfo> ResultProperties;

    if (ArrayProperty && ArrayProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)) {
        const EReflectedPropertyType InnerPropertyType = DeterminePropertyType(ArrayProperty->Inner);
        if (InnerPropertyType != EReflectedPropertyType::ERPT_Invalid) {
            void* PropertyValue = OwnerObjectState->GetPropertyValue(ArrayPropertyName);
            const FScriptArrayHelper ArrayHelper{ArrayProperty, PropertyValue};

            for (int32 i = 0; i < ArrayHelper.Num(); i++) {
                const FName PropertyName = *FString::FromInt(i);
                ResultProperties.Add(FReflectedPropertyInfo{PropertyName, InnerPropertyType});
            }
        }
    }
    return ResultProperties;
}

FProperty* FReflectedObjectState_Array::FindPropertyByName(const FName PropertyName) const {
    FArrayProperty* ArrayProperty = Cast<FArrayProperty>(OwnerObjectState->FindPropertyByName(ArrayPropertyName));
    if (ArrayProperty && ArrayProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)) {
        void* PropertyValue = OwnerObjectState->GetPropertyValue(ArrayPropertyName);
        const FScriptArrayHelper ArrayHelper{ArrayProperty, PropertyValue};

        const int32 ArrayIndex = FCString::Atoi(*PropertyName.ToString());
        if (ArrayHelper.IsValidIndex(ArrayIndex)) {
            return ArrayProperty->Inner;
        }
    }
    return NULL;
}

void* FReflectedObjectState_Array::GetPropertyValue(const FName PropertyName) {
    FArrayProperty* ArrayProperty = Cast<FArrayProperty>(OwnerObjectState->FindPropertyByName(ArrayPropertyName));
    if (ArrayProperty && ArrayProperty->HasAnyPropertyFlags(CPF_BlueprintVisible)) {
        void* PropertyValue = OwnerObjectState->GetPropertyValue(ArrayPropertyName);
        FScriptArrayHelper ArrayHelper{ArrayProperty, PropertyValue};

        const int32 ArrayIndex = FCString::Atoi(*PropertyName.ToString());
        if (ArrayHelper.IsValidIndex(ArrayIndex)) {
            return ArrayHelper.GetRawPtr(ArrayIndex);
        }
    }
    return NULL;
}

void FReflectedObjectState_Array::AddReferencedObjects(FReferenceCollector& ReferenceCollector) {
    if (OwnerObjectState.IsValid()) {
        this->OwnerObjectState->AddReferencedObjects(ReferenceCollector);
    }
}

FReflectedObjectState_UObject::FReflectedObjectState_UObject(UObject* Object) {
    checkf(Object, TEXT("Cannot construct state without valid UObject"));
    this->ReferencedObject = Object;
}

UObject* FReflectedObjectState_UObject::GetObjectPointer() const {
    return ReferencedObject;
}

UStruct* FReflectedObjectState_UObject::GetStructObject() const {
    return ReferencedObject ? ReferencedObject->GetClass() : NULL;
}

void* FReflectedObjectState_UObject::GetObjectData() {
    return ReferencedObject;
}

void FReflectedObjectState_UObject::AddReferencedObjects(FReferenceCollector& ReferenceCollector) {
    ReferenceCollector.AddReferencedObject(ReferencedObject);
}

FReflectedObjectState_ScriptStruct::FReflectedObjectState_ScriptStruct(UScriptStruct* Struct) {
    checkf(Struct, TEXT("Cannot construct state with NULL struct"));
    this->ScriptStruct = Struct;
    this->StructDataPointer = FMemory::Malloc(Struct->GetStructureSize());
    Struct->InitializeStruct(StructDataPointer);
}

FReflectedObjectState_ScriptStruct::~FReflectedObjectState_ScriptStruct() {
    if (StructDataPointer != NULL) {
        ScriptStruct->DestroyStruct(StructDataPointer);
        FMemory::Free(StructDataPointer);
        this->StructDataPointer = NULL;
    }
}

void FReflectedObjectState_ScriptStruct::CopyStructValue(UScriptStruct* StructType, void* StructValue) const {
    checkf(StructValue, TEXT("Attempt to Copy Struct Value into NULL struct"));
    checkf(ScriptStruct == StructType, TEXT("Attempt to Copy Struct Value into struct with different type"));
    if (ScriptStruct != NULL && ScriptStruct == StructType) {
        ScriptStruct->CopyScriptStruct(StructValue, StructDataPointer);
    }
}

bool FReflectedObjectState_ScriptStruct::ShouldStripPropertyNames() const {
    return true;
}

void FReflectedObjectState_ScriptStruct::CopyDataFromStruct(const void* SourceStruct) const {
    if (StructDataPointer != NULL) {
        ScriptStruct->CopyScriptStruct(StructDataPointer, SourceStruct);
    }
}

UStruct* FReflectedObjectState_ScriptStruct::GetStructObject() const {
    return ScriptStruct;
}

void* FReflectedObjectState_ScriptStruct::GetObjectData() {
    return StructDataPointer;
}

void FReflectedObjectState_ScriptStruct::AddReferencedObjects(FReferenceCollector& ReferenceCollector) {
    UScriptStruct* OldStruct = ScriptStruct;
    ReferenceCollector.AddReferencedObject(ScriptStruct);
    // If struct defined custom AddStructReferencedObjects, call it directly
    if (OldStruct->StructFlags & STRUCT_AddStructReferencedObjects) {
        OldStruct->GetCppStructOps()->AddStructReferencedObjects()(StructDataPointer, ReferenceCollector);
    } else {
        // Otherwise fallback to slow UProperty reference traversal via serialization
        OldStruct->SerializeItem(ReferenceCollector.GetVerySlowReferenceCollectorArchive(), StructDataPointer, nullptr);
    }
}

TArray<FReflectedPropertyInfo> FReflectedObjectState::GetAllProperties() const {
    UStruct* ObjectStruct = GetStructObject();
    TArray<FReflectedPropertyInfo> OutPropertyInfo;
    if (ObjectStruct != NULL) {
        for (FProperty* Property = ObjectStruct->PropertyLink; Property; Property = Property->PropertyLinkNext) {
            if (Property->HasAnyPropertyFlags(CPF_BlueprintVisible)) {
                const FName Name = Property->GetFName();
                const EReflectedPropertyType PropertyType = DeterminePropertyType(Property);
                if (PropertyType != EReflectedPropertyType::ERPT_Invalid) {
                    OutPropertyInfo.Add(FReflectedPropertyInfo{Name, PropertyType});
                }
            }
        }
    }
    return OutPropertyInfo;
}

FProperty* FReflectedObjectState::FindPropertyByName(FName PropertyName) const {
    UStruct* AssociatedStruct = GetStructObject();
    if (AssociatedStruct != NULL) {
        return AssociatedStruct->FindPropertyByName(PropertyName);
    }
    return NULL;
}

void* FReflectedObjectState::GetPropertyValue(FName PropertyName) {
    UStruct* AssociatedStruct = GetStructObject();
    void* ObjectData = GetObjectData();
    if (AssociatedStruct != NULL && ObjectData != NULL) {
        FProperty* Property = AssociatedStruct->FindPropertyByName(PropertyName);
        if (Property != NULL) {
            return Property->ContainerPtrToValuePtr<void>(ObjectData);
        }
    }
    return NULL;
}

int32 FReflectedObjectState::GetArrayNum() const {
    return 0;
}

int32 FReflectedObjectState::AddNewArrayElement() const {
    return -1;
}

void FReflectedObjectState::RemoveArrayElements(int32 Index, int32 Count) const {
}

void FReflectedObjectState::CopyStructValue(UScriptStruct* StructType, void* StructValue) const {
}

UObject* FReflectedObjectState::GetObjectPointer() const {
    return NULL;
}

bool FReflectedObjectState::ShouldStripPropertyNames() const {
    return false;
}

UStruct* FReflectedObjectState::GetStructObject() const {
    return NULL;
}

void* FReflectedObjectState::GetObjectData() {
    return NULL;
}

FReflectedEnumValue::FReflectedEnumValue() : EnumerationType(NULL), RawEnumValue(0) {
}

FReflectedEnumValue::FReflectedEnumValue(UEnum* EnumType, int64 EnumValue) : EnumerationType(EnumType), RawEnumValue(EnumValue) {
}

UEnum* FReflectedEnumValue::GetEnumerationType() const {
    return EnumerationType;
}

int64 FReflectedEnumValue::GetCurrentValue() const {
    return RawEnumValue;
}

void FReflectedEnumValue::SetCurrentValue(int64 NewValue) {
    if (EnumerationType != NULL) {
        checkf(EnumerationType->IsValidEnumValue(NewValue), TEXT("Invalid Enum value: %lld"), NewValue);
    }
    this->RawEnumValue = NewValue;
}

void FReflectedObject::SetupFromUObject(UObject* Object) {
    checkf(Object, TEXT("Cannot setup from NULL UObject"));
    this->State = MakeShareable(new FReflectedObjectState_UObject(Object));
}

void FReflectedObject::SetupFromStruct(UScriptStruct* StructType, const void* StructData) {
    checkf(StructType, TEXT("Cannot setup from NULL ScriptStruct"));
    const TSharedPtr<FReflectedObjectState_ScriptStruct> StructState = MakeShareable(new FReflectedObjectState_ScriptStruct(StructType));
    this->State = StructState;
    if (StructData != NULL) {
        StructState->CopyDataFromStruct(StructData);
    }
}

void FReflectedObject::SetupFromArray(const FReflectedObject& Object, const FName PropertyName) {
    if (Object.State.IsValid()) {
        this->State = MakeShareable(new FReflectedObjectState_Array(Object.State, PropertyName));
    }
}

UObject* FReflectedObject::GetWrappedObject() const {
    return State.IsValid() ? State->GetObjectPointer() : NULL;
}

void FReflectedObject::CopyWrappedStruct(UScriptStruct* StructType, void* StructValue) const {
    if (State.IsValid()) {
        State->CopyStructValue(StructType, StructValue);
    }
}

EReflectedPropertyType DeterminePropertyType(FProperty* Property) {
    if (Property->ArrayDim != 1) {
        return EReflectedPropertyType::ERPT_Array;
    }
    if (Property->IsA<FArrayProperty>()) {
        return EReflectedPropertyType::ERPT_Array;
    }
    if (FByteProperty* ByteProperty = Cast<FByteProperty>(Property)) {
        if (ByteProperty->IsEnum()) return EReflectedPropertyType::ERPT_Enum;
        return EReflectedPropertyType::ERPT_Byte;
    }
    if (Property->IsA<FIntProperty>()) {
        return EReflectedPropertyType::ERPT_Int32;
    }
    if (Property->IsA<FInt64Property>()) {
        return EReflectedPropertyType::ERPT_Int64;
    }
    if (Property->IsA<FFloatProperty>()) {
        return EReflectedPropertyType::ERPT_Float;
    }
    if (Property->IsA<FStrProperty>()) {
        return EReflectedPropertyType::ERPT_String;
    }
    if (Property->IsA<FNameProperty>()) {
        return EReflectedPropertyType::ERPT_Name;
    }
    if (Property->IsA<FTextProperty>()) {
        return EReflectedPropertyType::ERPT_Text;
    }
    if (Property->IsA<FBoolProperty>()) {
        return EReflectedPropertyType::ERPT_Bool;
    }
    if (Property->IsA<FEnumProperty>()) {
        return EReflectedPropertyType::ERPT_Enum;
    }
    if (Property->IsA<FObjectPropertyBase>()) {
        return EReflectedPropertyType::ERPT_Object;
    }
    if (Property->IsA<FStructProperty>()) {
        return EReflectedPropertyType::ERPT_Struct;
    }
    return EReflectedPropertyType::ERPT_Invalid;
}

TArray<FReflectedPropertyInfo> FReflectedObject::GetReflectedProperties() const {
    return State.IsValid() ? State->GetAllProperties() : TArray<FReflectedPropertyInfo>();
}

#define IMPLEMENT_GET_PROPERTY_VALUE(PropertyType, DefaultValue) \
    F##PropertyType::TCppType FReflectedObject::Get##PropertyType(FName PropertyName) const { \
        if (F##PropertyType* CastedProperty = FindPropertyByName<F##PropertyType>(PropertyName, false)) { \
            void* PropertyValuePtr = State->GetPropertyValue(PropertyName); \
            return CastedProperty->GetPropertyValue(PropertyValuePtr); \
        } \
        return DefaultValue; \
    }

#define IMPLEMENT_SET_PROPERTY_VALUE(PropertyType) \
    void FReflectedObject::Set##PropertyType(FName PropertyName, const F##PropertyType::TCppType Value) const { \
        if (F##PropertyType* CastedProperty = FindPropertyByName<F##PropertyType>(PropertyName, true)) { \
            void* PropertyValuePtr = State->GetPropertyValue(PropertyName); \
            CastedProperty->SetPropertyValue(PropertyValuePtr, Value); \
        } \
    }

#define IMPLEMENT_PROPERTY_TYPE(PropertyType, DefaultValue) \
    IMPLEMENT_GET_PROPERTY_VALUE(PropertyType, DefaultValue); \
    IMPLEMENT_SET_PROPERTY_VALUE(PropertyType);

IMPLEMENT_PROPERTY_TYPE(ByteProperty, 0);
IMPLEMENT_PROPERTY_TYPE(IntProperty, 0);
IMPLEMENT_PROPERTY_TYPE(Int64Property, 0);
IMPLEMENT_PROPERTY_TYPE(FloatProperty, 0.0f);
IMPLEMENT_PROPERTY_TYPE(BoolProperty, false);
IMPLEMENT_PROPERTY_TYPE(NameProperty, NAME_None);
IMPLEMENT_PROPERTY_TYPE(TextProperty, FText::GetEmpty());
IMPLEMENT_PROPERTY_TYPE(StrProperty, TEXT(""));
IMPLEMENT_PROPERTY_TYPE(ObjectProperty, NULL);
IMPLEMENT_PROPERTY_TYPE(WeakObjectProperty, NULL);
IMPLEMENT_PROPERTY_TYPE(LazyObjectProperty, FLazyObjectPtr(NULL));
IMPLEMENT_PROPERTY_TYPE(SoftObjectProperty, FSoftObjectPtr(NULL));

FReflectedObject FReflectedObject::GetStructProperty(FName PropertyName) const {
    FReflectedObject ReflectedObject{};
    if (FStructProperty* StructProperty = FindPropertyByName<FStructProperty>(PropertyName, false)) {
        UScriptStruct* ScriptStruct = StructProperty->Struct;
        void* StructValue = State->GetPropertyValue(PropertyName);
        ReflectedObject.SetupFromStruct(ScriptStruct, StructValue);
    }
    return ReflectedObject;
}

void FReflectedObject::SetStructProperty(FName PropertyName, const FReflectedObject& Struct) const {
    if (FStructProperty* StructProperty = FindPropertyByName<FStructProperty>(PropertyName, true)) {
        UScriptStruct* ScriptStruct = StructProperty->Struct;
        void* StructValue = State->GetPropertyValue(PropertyName);
        if (ScriptStruct == Struct.GetStruct()) {
            ScriptStruct->CopyScriptStruct(StructValue, Struct.State->GetObjectData());
        }
    }
}

FReflectedObject FReflectedObject::GetArrayProperty(FName PropertyName) const {
    FReflectedObject ReflectedObject{};
    if (FindPropertyByName<FArrayProperty>(PropertyName, true)) {
        ReflectedObject.SetupFromArray(*this, PropertyName);
    }
    return ReflectedObject;
}

FReflectedEnumValue FReflectedObject::GetEnumProperty(FName PropertyName) const {
    FProperty* Property = FindPropertyByName<FProperty>(PropertyName, false);
    if (Property != NULL) {
        void* PropertyValue = State->GetPropertyValue(PropertyName);
        if (FEnumProperty* EnumProperty = Cast<FEnumProperty>(Property)) {
            const int64 RawPropertyValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(PropertyValue);
            return FReflectedEnumValue{EnumProperty->GetEnum(), RawPropertyValue};
        }

        if (FByteProperty* ByteProperty = Cast<FByteProperty>(Property)) {
            if (ByteProperty->IsEnum()) {
                const int64 RawPropertyValue = ByteProperty->GetPropertyValue(PropertyValue);
                return FReflectedEnumValue{ByteProperty->Enum, RawPropertyValue};
            }
        }
    }
    return FReflectedEnumValue{NULL, 0};
}

void FReflectedObject::SetEnumProperty(FName PropertyName, const FReflectedEnumValue& Enum) const {
    FProperty* Property = FindPropertyByName<FProperty>(PropertyName, true);
    if (Property != NULL) {
        void* PropertyValue = State->GetPropertyValue(PropertyName);
        if (FEnumProperty* EnumProperty = Cast<FEnumProperty>(Property)) {
            if (EnumProperty->GetEnum() == Enum.GetEnumerationType()) {
                EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(PropertyValue, Enum.GetCurrentValue());
            }
        }

        if (FByteProperty* ByteProperty = Cast<FByteProperty>(Property)) {
            if (ByteProperty->IsEnum() && ByteProperty->Enum == Enum.GetEnumerationType()) {
                ByteProperty->SetPropertyValue_InContainer(PropertyValue, (uint8)Enum.GetCurrentValue());
            }
        }
    }
}

void FReflectedObject::AddStructReferencedObjects(FReferenceCollector& Collector) const {
    if (State.IsValid()) {
        State->AddReferencedObjects(Collector);
    }
}

int32 FReflectedObject::GetArrayNum() const {
    return State.IsValid() ? State->GetArrayNum() : 0;
}

int32 FReflectedObject::AddNewArrayElement() const {
    return State.IsValid() ? State->AddNewArrayElement() : -1;
}

void FReflectedObject::ClearArray() const {
    RemoveArrayElements(0, GetArrayNum());
}

void FReflectedObject::RemoveArrayElements(int32 Index, int32 Count) const {
    if (State.IsValid()) {
        State->RemoveArrayElements(Index, Count);
    }
}

bool ComparePropertyNames(FName Name, FName Pattern, bool bStripName) {
    return bStripName ? (Name.ToString().StartsWith(Pattern.ToString())) : (Name == Pattern);
}

bool CheckPropertyFlags(FProperty* Property, bool bCheckForWriteable) {
    return Property->HasAnyPropertyFlags(CPF_BlueprintVisible) &&
        (!bCheckForWriteable || !Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
}
