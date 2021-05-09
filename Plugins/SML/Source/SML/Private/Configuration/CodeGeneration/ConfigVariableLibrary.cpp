#include "Configuration/CodeGeneration/ConfigVariableLibrary.h"

FConfigVariableDescriptor UConfigVariableLibrary::MakeConfigVariablePrimitive(EConfigVariableType VariableType) {
    checkf(
        VariableType == EConfigVariableType::ECVT_Int32 ||
        VariableType == EConfigVariableType::ECVT_Float ||
        VariableType == EConfigVariableType::ECVT_String ||
        VariableType == EConfigVariableType::ECVT_Bool,
        TEXT("MakeConfigVariablePrimitive: non-primitive (string, int32, float) variable type passed: %d"),
        VariableType
    );
    FConfigVariableDescriptor VariableDescriptor{};
    VariableDescriptor.SetupAsPrimitive(VariableType);
    return VariableDescriptor;
}

FConfigVariableDescriptor UConfigVariableLibrary::MakeConfigVariableStruct(const FDynamicStructInfo& Struct) {
    checkf(Struct.Struct, TEXT("MakeConfigVariableStruct: NULL struct passed"));
    FConfigVariableDescriptor VariableDescriptor{};
    VariableDescriptor.SetupAsCustomStruct(Struct.Struct);
    return VariableDescriptor;
}

FConfigVariableDescriptor UConfigVariableLibrary::MakeConfigVariableObject(UClass* BaseObjectClass) {
    checkf(BaseObjectClass, TEXT("MakeConfigVariableObject: NULL base class passed"));
    FConfigVariableDescriptor VariableDescriptor{};
    VariableDescriptor.SetupAsObject(BaseObjectClass);
    return VariableDescriptor;
}

FConfigVariableDescriptor UConfigVariableLibrary::MakeConfigVariableClass(UClass* BaseClassType) {
    checkf(BaseClassType, TEXT("MakeConfigVariableClass: NULL base class type passed"));
    FConfigVariableDescriptor VariableDescriptor{};
    VariableDescriptor.SetupAsClass(BaseClassType);
    return VariableDescriptor;
}

FConfigVariableDescriptor UConfigVariableLibrary::MakeConfigVariableGeneratedStruct(UConfigGeneratedStruct* Struct) {
    checkf(Struct, TEXT("MakeConfigVariableGeneratedStruct: NULL config struct passed"));
    FConfigVariableDescriptor VariableDescriptor{};
    VariableDescriptor.SetupAsConfigGeneratedStruct(Struct);
    return VariableDescriptor;
}

FConfigVariableDescriptor UConfigVariableLibrary::MakeConfigVariableArray(const FConfigVariableDescriptor& ElementType) {
    FConfigVariableDescriptor VariableDescriptor{};
    VariableDescriptor.SetupAsArray(ElementType);
    return VariableDescriptor;
}

FConfigVariableDescriptor UConfigVariableLibrary::MakeConfigVariableMap(const FConfigVariableDescriptor& KeyType, const FConfigVariableDescriptor& ValueType) {
    FConfigVariableDescriptor VariableDescriptor{};
    VariableDescriptor.SetupAsMap(KeyType, ValueType);
    return VariableDescriptor;
}
