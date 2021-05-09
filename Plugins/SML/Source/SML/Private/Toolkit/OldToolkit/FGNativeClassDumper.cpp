#include "Toolkit/OldToolkit/FGNativeClassDumper.h"
#include "HAL/ConsoleManager.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"

DEFINE_LOG_CATEGORY(LogNativeClassDumper)

bool CheckValueEqualDefault(UProperty* Property, const void* PropertyValue) {
    const SIZE_T PropertyValueSize = Property->GetSize();
    void* DefaultValue = FMemory::Malloc(PropertyValueSize);
    Property->InitializeValue(DefaultValue);
    const bool bRes = Property->Identical(PropertyValue, DefaultValue);
    FMemory::Free(DefaultValue);
    return bRes;
}

UClass* FindFirstClassWithValueOverride(UObject* ClassObject, UProperty* Property) {
    UClass* EndClass = Cast<UClass>(Property->GetOwnerUObject());
    UClass* TargetClass = ClassObject->GetClass();
    void* CurrentValue = Property->ContainerPtrToValuePtr<void>(ClassObject);
    while (TargetClass != EndClass) {
        UClass* ParentClass = TargetClass->GetSuperClass();
        UObject* ParentClassObject = ParentClass->GetDefaultObject();
        void* ParentPropertyValue = Property->ContainerPtrToValuePtr<void>(ParentClassObject);
        if (!Property->Identical(CurrentValue, ParentPropertyValue)) {
            // Values are different! So TargetClass is the class where value is overriden
            return TargetClass;
        }
        // Values are the same, so parent class becomes next target class
        TargetClass = ParentClass;
        CurrentValue = ParentPropertyValue;
    }
    // value is the same as in the property initial class
    return EndClass;
}

bool CanSkipPropertyValue(UProperty* Property, UObject* ClassObject) {
    UClass* ClassWithValueOverride = FindFirstClassWithValueOverride(ClassObject, Property);
    // We can use parent value if it's not set in native class (we don't have .cpp for native classes in editor), and it's not us who set the value
    const bool bCanUseParentValue = ClassWithValueOverride != ClassObject->GetClass() && !ClassWithValueOverride->GetPathName().StartsWith(TEXT("/Script/FactoryGame."));
    const bool bIsValueEqualDefault = CheckValueEqualDefault(Property, Property->ContainerPtrToValuePtr<void>(ClassObject));
    // we can skip property value if we're able to use parent value or it is default value anyway
    return bCanUseParentValue || bIsValueEqualDefault;
}

TSharedRef<FJsonValue> CustomPropertySerialization(UProperty* Property, const void* Value, UPropertySerializer* PropertySerializer) {
    if (const UClassProperty* ClassProperty = Cast<const UClassProperty>(Property)) {
        UClass* ClassObject = Cast<UClass>(ClassProperty->GetObjectPropertyValue(Value));
        // For class it's enough just to have it's path name for deserialization
        return MakeShareable(new FJsonValueString(ClassObject->GetPathName()));
    }

    if (Property->IsA<USoftObjectProperty>()) {
        // For soft object reference, path is enough too for deserialization.
        const FSoftObjectPtr* ObjectPtr = reinterpret_cast<const FSoftObjectPtr*>(Value);
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("AssetPathName"), ObjectPtr->ToSoftObjectPath().ToString());
        return MakeShareable(new FJsonValueObject(Result));
    }

    if (const UObjectPropertyBase* ObjectProperty = Cast<const UObjectPropertyBase>(Property)) {
        // Only need class and name for components
        UObject* Object = ObjectProperty->GetObjectPropertyValue(Value);
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        if (Object == nullptr) {
            Result->SetBoolField(TEXT("$Empty"), true);
        } else {
            Result->SetField(TEXT("$ObjectName"), MakeShareable(new FJsonValueString(Object->GetName())));
            Result->SetField(TEXT("$ObjectClass"), MakeShareable(new FJsonValueString(Object->GetClass()->GetPathName())));
            if (const USceneComponent* Component = Cast<USceneComponent>(Object)) {
                if (Component->GetAttachParent() != nullptr) {
                    TSharedPtr<FJsonObject> AttachParent = MakeShareable(new FJsonObject());
                    AttachParent->SetField(TEXT("$ObjectName"), MakeShareable(new FJsonValueString(Component->GetAttachParent()->GetName())));
                    Result->SetObjectField(TEXT("AttachParent"), AttachParent);
                }
            }
        }
        return MakeShareable(new FJsonValueObject(Result));
    }

    return PropertySerializer->SerializePropertyValue(Property, Value);
}

TSharedPtr<FJsonObject> DumpNativeClass(UClass* NativeClass, UPropertySerializer* PropertySerializer) {
    const TSharedPtr<FJsonObject> ResultJsonObject = MakeShareable(new FJsonObject());
    UE_LOG(LogNativeClassDumper, Display, TEXT("Dumping native class %s"), *NativeClass->GetFullName());

    ResultJsonObject->SetStringField(TEXT("Class"), NativeClass->GetPathName());

    TArray<TSharedPtr<FJsonValue>> Fields;
    UObject* CDO = NativeClass->GetDefaultObject();
    for (TFieldIterator<UProperty> It(NativeClass); It; ++It) {
        UProperty* Property = *It;
        if (CanSkipPropertyValue(Property, CDO)) continue;
        const void* Value = Property->ContainerPtrToValuePtr<void>(CDO);
        TSharedPtr<FJsonObject> PropertyObject = MakeShareable(new FJsonObject());
        PropertyObject->SetStringField(TEXT("Name"), *Property->GetName());
        PropertyObject->SetField(TEXT("Value"), CustomPropertySerialization(Property, Value, PropertySerializer));
        PropertyObject->SetBoolField(TEXT("NativeArray"), Property->ArrayDim > 1);
        Fields.Add(MakeShareable(new FJsonValueObject(PropertyObject)));
    }
    if (Fields.Num() > 0) {
        ResultJsonObject->SetArrayField(TEXT("Fields"), Fields);
    }

    return ResultJsonObject;
}

void ExecuteDumpAllGameNativeClasses(const TArray<FString>& Arguments) {
    UE_LOG(LogNativeClassDumper, Display, TEXT("Dumping all native game classes..."));
    UE_LOG(LogNativeClassDumper, Display, TEXT("Warning! It may take noticable amount of time!"));

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    TArray<TSharedPtr<FJsonValue>> ClassesJsonObjects;

    UPackage* NativePackage = FindPackage(NULL, TEXT("/Script/FactoryGame"));
    UPropertySerializer* PropertySerializer = NewObject<UPropertySerializer>(GetTransientPackage());

    for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt) {
        UClass* Class = *ClassIt;

        // Only interested in native C++ classes
        if (!Class->IsNative()) {
            continue;
        }

        if (Class->GetOuterUPackage() == NativePackage) {
            ClassesJsonObjects.Add(MakeShareable(new FJsonValueObject(DumpNativeClass(Class, PropertySerializer))));
        }
    }

    FString ResultPath = FPaths::Combine(FPaths::RootDir(), TEXT("AssetDump"), TEXT("NativeClasses.json"));
    ResultPath = FPaths::ConvertRelativePathToFull(ResultPath);

    TSharedPtr<FJsonObject> ResultJsonObject = MakeShareable(new FJsonObject());
    ResultJsonObject->SetArrayField(TEXT("Classes"), ClassesJsonObjects);

    PlatformFile.CreateDirectoryTree(*FPaths::GetPath(ResultPath));
    UE_LOG(LogNativeClassDumper, Display, TEXT("Saving to %s"), *ResultPath);

    // Serialize contents to string
    FString OutFileContentsString;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutFileContentsString);
    FJsonSerializer::Serialize(ResultJsonObject.ToSharedRef(), Writer);

    // Save primary asset file text as json
    FFileHelper::SaveStringToFile(OutFileContentsString, *ResultPath);


    UE_LOG(LogNativeClassDumper, Display, TEXT("Dumping native game classes finished!"));
}

void FGameNativeClassDumper::RegisterConsoleCommands() {
    IConsoleManager& ConsoleManager = FConsoleManager::Get();

    FConsoleCommandWithArgsDelegate CommandDelegateDumpAllNative;
    CommandDelegateDumpAllNative.BindStatic(ExecuteDumpAllGameNativeClasses);
    ConsoleManager.RegisterConsoleCommand(TEXT("DumpAllGameNativeClasses"), TEXT("DumpAllGameNativeClasses"), CommandDelegateDumpAllNative);
}
