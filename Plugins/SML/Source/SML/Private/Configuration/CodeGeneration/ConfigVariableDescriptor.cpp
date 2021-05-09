#include "Configuration/CodeGeneration/ConfigVariableDescriptor.h"

FConfigVariableDescriptor::FConfigVariableDescriptor() {
    SetupAsPrimitive(EConfigVariableType::ECVT_String);
}

FConfigVariableDescriptor::FConfigVariableDescriptor(const FConfigVariableDescriptor& Descriptor) {
    switch (Descriptor.VariableType) {
    case EConfigVariableType::ECVT_Object: this->SetupAsObject(BaseObjectClass);
        break;
    case EConfigVariableType::ECVT_Class: this->SetupAsClass(BaseClassType);
        break;
    case EConfigVariableType::ECVT_CustomStruct: this->SetupAsCustomStruct(Descriptor.CustomStructType);
        break;
    case EConfigVariableType::ECVT_ConfigGeneratedStruct: this->SetupAsConfigGeneratedStruct(Descriptor.ConfigGeneratedStructType);
        break;
    case EConfigVariableType::ECVT_Array: this->SetupAsArray(Descriptor.GetArrayElementDescriptor());
        break;
    case EConfigVariableType::ECVT_Map: this->SetupAsMap(Descriptor.GetMapKeyDescriptor(), Descriptor.GetMapValueDescriptor());
        break;
    default: this->SetupAsPrimitive(Descriptor.VariableType);
    }
}

bool FConfigVariableDescriptor::operator==(const FConfigVariableDescriptor& Other) const {
    if (Other.VariableType != VariableType) {
        return false;
    }
    switch (VariableType) {
    case EConfigVariableType::ECVT_Class: return BaseClassType == Other.BaseClassType;
    case EConfigVariableType::ECVT_Object: return BaseObjectClass == Other.BaseObjectClass;
    case EConfigVariableType::ECVT_CustomStruct: return CustomStructType == Other.CustomStructType;
    case EConfigVariableType::ECVT_ConfigGeneratedStruct: return ConfigGeneratedStructType == Other.ConfigGeneratedStructType;
    case EConfigVariableType::ECVT_Array: return GetArrayElementDescriptor() == Other.GetArrayElementDescriptor();
    case EConfigVariableType::ECVT_Map: return GetMapKeyDescriptor() == Other.GetMapKeyDescriptor() && GetMapValueDescriptor() == Other.GetMapValueDescriptor();
    default: return true;
    }
}

bool FConfigVariableDescriptor::operator!=(const FConfigVariableDescriptor& Other) const {
    return !operator==(Other);
}

void FConfigVariableDescriptor::AddStructReferencedObjects(FReferenceCollector& Collector) const {
    FConfigVariableDescriptor* NonConstDescriptor = const_cast<FConfigVariableDescriptor*>(this);
    Collector.AddReferencedObject(NonConstDescriptor->BaseObjectClass);
    Collector.AddReferencedObject(NonConstDescriptor->BaseClassType);
    Collector.AddReferencedObject(NonConstDescriptor->CustomStructType);
    Collector.AddReferencedObject(NonConstDescriptor->ConfigGeneratedStructType);
    if (ArrayElementDescriptor.IsValid()) {
        ArrayElementDescriptor->AddStructReferencedObjects(Collector);
    }
    if (MapKeyDescriptor.IsValid()) {
        MapKeyDescriptor->AddStructReferencedObjects(Collector);
    }
    if (MapValueDescriptor.IsValid()) {
        MapValueDescriptor->AddStructReferencedObjects(Collector);
    }
}

void FConfigVariableDescriptor::SetupAsPrimitive(EConfigVariableType NewVariableType) {
    this->VariableType = NewVariableType;
}

void FConfigVariableDescriptor::SetupAsArray(const FConfigVariableDescriptor& ElementType) {
    this->VariableType = EConfigVariableType::ECVT_Array;
    this->ArrayElementDescriptor = MakeShareable(new FConfigVariableDescriptor(ElementType));
}

void FConfigVariableDescriptor::SetupAsMap(const FConfigVariableDescriptor& KeyType, const FConfigVariableDescriptor& ValueType) {
    this->VariableType = EConfigVariableType::ECVT_Map;
    this->MapKeyDescriptor = MakeShareable(new FConfigVariableDescriptor(KeyType));
    this->MapValueDescriptor = MakeShareable(new FConfigVariableDescriptor(ValueType));
}

void FConfigVariableDescriptor::SetupAsCustomStruct(UScriptStruct* CustomStruct) {
    this->VariableType = EConfigVariableType::ECVT_CustomStruct;
    this->CustomStructType = CustomStruct;
}

void FConfigVariableDescriptor::SetupAsConfigGeneratedStruct(UConfigGeneratedStruct* ConfigStruct) {
    this->VariableType = EConfigVariableType::ECVT_ConfigGeneratedStruct;
    this->ConfigGeneratedStructType = ConfigStruct;
}

void FConfigVariableDescriptor::SetupAsClass(UClass* NewBaseClassType) {
    this->VariableType = EConfigVariableType::ECVT_Class;
    this->BaseClassType = NewBaseClassType;
}

void FConfigVariableDescriptor::SetupAsObject(UClass* NewBaseObjectClass) {
    this->VariableType = EConfigVariableType::ECVT_Object;
    this->BaseObjectClass = NewBaseObjectClass;
}

UScriptStruct* FConfigVariableDescriptor::GetCustomStructType() const {
    if (VariableType == EConfigVariableType::ECVT_CustomStruct) return CustomStructType;
    return NULL;
}

UClass* FConfigVariableDescriptor::GetBaseObjectClass() const {
    if (VariableType == EConfigVariableType::ECVT_Object) return BaseObjectClass;
    return NULL;
}

UClass* FConfigVariableDescriptor::GetBaseClassType() const {
    if (VariableType == EConfigVariableType::ECVT_Class) return BaseClassType;
    return NULL;
}

UConfigGeneratedStruct* FConfigVariableDescriptor::GetConfigGeneratedStruct() const {
    if (VariableType == EConfigVariableType::ECVT_ConfigGeneratedStruct) return ConfigGeneratedStructType;
    return NULL;
}

FConfigVariableDescriptor FConfigVariableDescriptor::GetArrayElementDescriptor() const {
    if (VariableType == EConfigVariableType::ECVT_Array) return ArrayElementDescriptor.IsValid() ? *ArrayElementDescriptor : FConfigVariableDescriptor{};
    return FConfigVariableDescriptor{};
}

FConfigVariableDescriptor FConfigVariableDescriptor::GetMapKeyDescriptor() const {
    if (VariableType == EConfigVariableType::ECVT_Map) return MapKeyDescriptor.IsValid() ? *MapKeyDescriptor : FConfigVariableDescriptor{};
    return FConfigVariableDescriptor{};
}

FConfigVariableDescriptor FConfigVariableDescriptor::GetMapValueDescriptor() const {
    if (VariableType == EConfigVariableType::ECVT_Map) return MapValueDescriptor.IsValid() ? *MapValueDescriptor : FConfigVariableDescriptor{};
    return FConfigVariableDescriptor{};
}
