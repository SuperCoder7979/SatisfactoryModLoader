#include "Toolkit/ObjectHierarchySerializer.h"
#include "Toolkit/PropertySerializer.h"
#include "Toolkit/AssetTypes/AssetHelper.h"
#include "UObject/Package.h"
#include "Toolkit/DefaultSerializableNativeClasses.h"

DECLARE_LOG_CATEGORY_CLASS(LogObjectHierarchySerializer, Warning, Log);

TSet<FName> UObjectHierarchySerializer::UnhandledNativeClasses;

UObjectHierarchySerializer::UObjectHierarchySerializer() {
    bAllowExportObjectSerialization = true;
    LastObjectIndex = 0;
    AllowedNativeSerializeClasses.Add(UObject::StaticClass());
    APPEND_DEFAULT_SERIALIZABLE_NATIVE_CLASSES(AllowedNativeSerializeClasses.Add);
}

void UObjectHierarchySerializer::SetPropertySerializer(UPropertySerializer* NewPropertySerializer) {
    check(NewPropertySerializer);
    this->PropertySerializer = NewPropertySerializer;
    NewPropertySerializer->ObjectHierarchySerializer = this;
}

void UObjectHierarchySerializer::InitializeForDeserialization(const TArray<TSharedPtr<FJsonValue>>& ObjectsArray) {
    this->LastObjectIndex = ObjectsArray.Num();
    for (int32 i = 0; i < LastObjectIndex; i++) {
        SerializedObjects.Add(i, ObjectsArray[i]->AsObject());
    }
}

void UObjectHierarchySerializer::InitializeForSerialization(UPackage* NewSourcePackage) {
    check(NewSourcePackage);
    this->SourcePackage = NewSourcePackage;
}

void UObjectHierarchySerializer::SetPackageForDeserialization(UPackage* SelfPackage) {
    check(SelfPackage);
    this->SourcePackage = SelfPackage;
}

void UObjectHierarchySerializer::AllowNativeClassSerialization(UClass* ClassToAllow) {
    this->AllowedNativeSerializeClasses.AddUnique(ClassToAllow);
}

void UObjectHierarchySerializer::SetObjectMark(UObject* Object, const FString& ObjectMark) {
    this->ObjectMarks.Add(Object, ObjectMark);
}

void UObjectHierarchySerializer::SetAllowExportedObjectSerialization(bool bAllowExportedObjectSerialization) {
    this->bAllowExportObjectSerialization = bAllowExportedObjectSerialization;
}

int32 UObjectHierarchySerializer::SerializeObject(UObject* Object) {
    if (Object == nullptr) {
        return INDEX_NONE;
    }

    const int32* ObjectIndex = ObjectIndices.Find(Object);
    if (ObjectIndex != nullptr) {
        return *ObjectIndex;
    }

    const int32 NewObjectIndex = LastObjectIndex++;
    ObjectIndices.Add(Object, NewObjectIndex);
    UPackage* ObjectPackage = Object->GetOutermost();

    TSharedRef<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetNumberField(TEXT("ObjectIndex"), NewObjectIndex);
    SerializedObjects.Add(NewObjectIndex, ResultJson);

    if (ObjectPackage != SourcePackage) {
        ResultJson->SetStringField(TEXT("Type"), TEXT("Import"));
        SerializeImportedObject(ResultJson, Object);

    } else {
        checkf(bAllowExportObjectSerialization, TEXT("Exported object serialization is not currently allowed"));
        ResultJson->SetStringField(TEXT("Type"), TEXT("Export"));

        if (ObjectMarks.Contains(Object)) {
            // This object is serialized using object mark string
            ResultJson->SetStringField(TEXT("ObjectMark"), ObjectMarks.FindChecked(Object));

        } else {
            // Serialize object normally
            SerializeExportedObject(ResultJson, Object);
        }
    }

    return NewObjectIndex;
}

UObject* UObjectHierarchySerializer::DeserializeObject(int32 Index) {
    if (Index == INDEX_NONE) {
        return nullptr;
    }
    UObject** LoadedObject = LoadedObjects.Find(Index);
    if (LoadedObject != nullptr) {
        return *LoadedObject;
    }
    if (!SerializedObjects.Contains(Index)) {
        UE_LOG(LogObjectHierarchySerializer, Error, TEXT("DeserializeObject for package %s called with invalid Index: %d"), *SourcePackage->GetName(), Index);
        return nullptr;
    }
    const TSharedPtr<FJsonObject>& ObjectJson = SerializedObjects.FindChecked(Index);
    const FString ObjectType = ObjectJson->GetStringField(TEXT("Type"));

    if (ObjectType == TEXT("Import")) {
        // Object is imported from another package, and not located in our own
        UObject* NewLoadedObject = DeserializeImportedObject(ObjectJson);
        LoadedObjects.Add(Index, NewLoadedObject);
        return NewLoadedObject;
    }

    if (ObjectType == TEXT("Export")) {
        // Object is defined inside our own package
        UObject* ConstructedObject;

        if (ObjectJson->HasField(TEXT("ObjectMark"))) {

            // Object is serialized through object mark
            const FString ObjectMark = ObjectJson->GetStringField(TEXT("ObjectMark"));
            UObject* const* FoundObject = ObjectMarks.FindKey(ObjectMark);
            checkf(FoundObject, TEXT("Cannot resolve object serialized using mark: %s"), *ObjectMark);
            ConstructedObject = *FoundObject;

        } else {
            // Perform normal deserialization
            ConstructedObject = DeserializeExportedObject(ObjectJson);
        }

        LoadedObjects.Add(Index, ConstructedObject);
        return ConstructedObject;
    }

    UE_LOG(LogObjectHierarchySerializer, Fatal, TEXT("Unhandled object type: %s for package %s"), *ObjectType, *SourcePackage->GetPathName());
    return nullptr;
}

TSharedRef<FJsonObject> UObjectHierarchySerializer::SerializeObjectProperties(UObject* Object) {
    TSharedRef<FJsonObject> Properties = MakeShareable(new FJsonObject());
    SerializeObjectPropertiesIntoObject(Object, Properties);
    return Properties;
}

void UObjectHierarchySerializer::SerializeObjectPropertiesIntoObject(UObject* Object, TSharedPtr<FJsonObject> Properties) {
    UClass* ObjectClass = Object->GetClass();
    for (UProperty* Property = ObjectClass->PropertyLink; Property; Property = Property->PropertyLinkNext) {
        if (PropertySerializer->ShouldSerializeProperty(Property)) {
            const void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Object);
            TSharedRef<FJsonValue> PropertyValueJson = PropertySerializer->SerializePropertyValue(Property, PropertyValue);
            Properties->SetField(Property->GetName(), PropertyValueJson);
        }
    }
}

void UObjectHierarchySerializer::DeserializeObjectProperties(const TSharedRef<FJsonObject>& Properties, UObject* Object) {
    UClass* ObjectClass = Object->GetClass();
    for (UProperty* Property = ObjectClass->PropertyLink; Property; Property = Property->PropertyLinkNext) {
        const FString PropertyName = Property->GetName();
        if (PropertySerializer->ShouldSerializeProperty(Property) && Properties->HasField(PropertyName)) {
            void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Object);
            const TSharedPtr<FJsonValue>& ValueObject = Properties->Values.FindChecked(PropertyName);
            if (ValueObject.IsValid()) {
                PropertySerializer->DeserializePropertyValue(Property, ValueObject.ToSharedRef(), PropertyValue);
            }
        }
    }
}

TArray<TSharedPtr<FJsonValue>> UObjectHierarchySerializer::FinalizeSerialization() {
    TArray<TSharedPtr<FJsonValue>> ObjectsArray;
    for (int32 i = 0; i < LastObjectIndex; i++) {
        if (!SerializedObjects.Contains(i)) {
            checkf(false, TEXT("Object not in serialized objects: %s"), *(*ObjectIndices.FindKey(i))->GetPathName());
        }
        ObjectsArray.Add(MakeShareable(new FJsonValueObject(SerializedObjects.FindChecked(i))));
    }
    return ObjectsArray;
}

UObject* UObjectHierarchySerializer::DeserializeExportedObject(TSharedPtr<FJsonObject> ObjectJson) {
    // Object is defined inside our own package, so we should have
    const int32 ObjectClassIndex = ObjectJson->GetIntegerField(TEXT("ObjectClass"));
    UClass* ObjectClass = Cast<UClass>(DeserializeObject(ObjectClassIndex));
    if (ObjectClass == nullptr) {
        UE_LOG(LogObjectHierarchySerializer, Error, TEXT("DeserializeObject for package %s failed: Cannot resolve object class %d"), *SourcePackage->GetName(), ObjectClassIndex);
        return nullptr;
    }

    // Outer will be missing for root UPackage export, e.g SourcePackage
    if (!ObjectJson->HasField(TEXT("Outer"))) {
        check(ObjectClass == UPackage::StaticClass());
        return SourcePackage;
    }

    const int32 OuterObjectIndex = ObjectJson->GetIntegerField(TEXT("Outer"));
    UObject* OuterObject = DeserializeObject(OuterObjectIndex);
    if (OuterObject == nullptr) {
        UE_LOG(LogObjectHierarchySerializer, Error, TEXT("DeserializeObject for package %s failed: Cannot resolve outer object %d"), *SourcePackage->GetName(), OuterObjectIndex);
        return nullptr;
    }

    const EObjectFlags ObjectLoadFlags = (EObjectFlags)ObjectJson->GetIntegerField(TEXT("ObjectFlags"));
    const FString ObjectName = ObjectJson->GetStringField(TEXT("ObjectName"));
    UObject* Template = GetArchetypeFromRequiredInfo(ObjectClass, OuterObject, *ObjectName, ObjectLoadFlags);

    // Give native deserializer a chance to handle object allocation
    UObject* ConstructedObject = StaticConstructObject_Internal(ObjectClass, OuterObject, *ObjectName, ObjectLoadFlags, EInternalObjectFlags::None, Template);

    // Deserialize object properties now
    if (ObjectJson->HasField(TEXT("Properties"))) {
        const TSharedPtr<FJsonObject>& Properties = ObjectJson->GetObjectField(TEXT("Properties"));
        if (Properties.IsValid()) {
            DeserializeObjectProperties(Properties.ToSharedRef(), ConstructedObject);
        }
    }
    return ConstructedObject;
}

UObject* UObjectHierarchySerializer::DeserializeImportedObject(TSharedPtr<FJsonObject> ObjectJson) {
    const FString PackageName = ObjectJson->GetStringField(TEXT("ClassPackage"));
    const FString ClassName = ObjectJson->GetStringField(TEXT("ClassName"));

    if (UPackage* Package = LoadPackage(NULL, *PackageName, LOAD_None)) {
        if (UClass* ObjectClass = FindObjectFast<UClass>(Package, *ClassName)) {
            const FString ObjectName = ObjectJson->GetStringField(TEXT("ObjectName"));
            // Outer is absent for root UPackage imports - Use ObjectName with LoadPackage directly
            if (!ObjectJson->HasField(TEXT("Outer"))) {
                check(ObjectClass == UPackage::StaticClass());
                UPackage* ResultPackage = LoadPackage(NULL, *ObjectName, LOAD_None);
                if (ResultPackage == nullptr) {
                    UE_LOG(LogObjectHierarchySerializer, Error, TEXT("DeserializeObject for package %s failed: Cannot resolve package %s"), *SourcePackage->GetName(), *ObjectName);
                }
                return ResultPackage;
            }
            // Otherwise, it is a normal object inside some outer
            const int32 OuterObjectIndex = ObjectJson->GetIntegerField(TEXT("Outer"));
            UObject* OuterObject = DeserializeObject(OuterObjectIndex);
            if (OuterObject == nullptr) {
                UE_LOG(LogObjectHierarchySerializer, Error, TEXT("DeserializeObject for package %s failed: Cannot resolve outer object %d"), *SourcePackage->GetName(), OuterObjectIndex);
                return nullptr;
            }
            // Use FindObjectFast now to resolve our object inside Outer
            UObject* ResultObject = StaticFindObjectFast(ObjectClass, OuterObject, *ObjectName);
            if (ResultObject == nullptr) {
                UE_LOG(LogObjectHierarchySerializer, Error, TEXT("DeserializeObject for package %s failed: Cannot find object %s inside outer %s"), *SourcePackage->GetName(), *OuterObject->GetPathName(), *ObjectName);
                return nullptr;
            }
            return ResultObject;
        }
    }

    // We are here if LoadPackage/FindObjectFast for class are failed
    UE_LOG(LogObjectHierarchySerializer, Error, TEXT("DeserializeObject for package %s failed: Cannot resolve object class %s.%s"), *SourcePackage->GetName(), *PackageName, *ClassName);
    return NULL;
}

void UObjectHierarchySerializer::SerializeImportedObject(TSharedPtr<FJsonObject> ResultJson, UObject* Object) {
    // Object is imported from different package
    UClass* ObjectClass = Object->GetClass();
    ResultJson->SetStringField(TEXT("ClassPackage"), ObjectClass->GetOutermost()->GetName());
    ResultJson->SetStringField(TEXT("ClassName"), ObjectClass->GetName());
    UObject* OuterObject = Object->GetOuter();

    // Outer object can be null if import is the top UPackage object
    if (OuterObject != nullptr) {
        const int32 OuterObjectIndex = SerializeObject(OuterObject);
        ResultJson->SetNumberField(TEXT("Outer"), OuterObjectIndex);
    }

    ResultJson->SetStringField(TEXT("ObjectName"), Object->GetName());
}

void UObjectHierarchySerializer::SerializeExportedObject(TSharedPtr<FJsonObject> ResultJson, UObject* Object) {
    // Object is located inside our own package, so we need to serialize it properly
    UClass* ObjectClass = Object->GetClass();
    ResultJson->SetNumberField(TEXT("ObjectClass"), SerializeObject(ObjectClass));
    UObject* OuterObject = Object->GetOuter();

    // Object being serialized is this package itself
    // Make sure object is package and write only object class, that is enough
    if (OuterObject == NULL) {
        check(ObjectClass == UPackage::StaticClass());
        return;
    }

    const int32 OuterObjectIndex = SerializeObject(OuterObject);
    ResultJson->SetNumberField(TEXT("Outer"), OuterObjectIndex);
    ResultJson->SetStringField(TEXT("ObjectName"), Object->GetName());

    // Serialize object flags that match RF_Load specification
    ResultJson->SetNumberField(TEXT("ObjectFlags"), (int32)(Object->GetFlags() & RF_Load));

    // If we have native serializer set, let it decide whenever we want normal property serialization or not
    const bool bShouldSerializeProperties = true;

    UClass* ClassWithSerialize = FAssetHelper::FindClassWithSerializeImplementation(ObjectClass);
    // checkf(AllowedNativeSerializeClasses.Contains(ClassWithSerialize), TEXT("Attempt to serialize object of class %s (%s) which has custom Serialize"),
    // *ClassWithSerialize->GetPathName(), *Object->GetPathName());
    if (!AllowedNativeSerializeClasses.Contains(ClassWithSerialize)) {
        UnhandledNativeClasses.Add(ClassWithSerialize->GetFName());
    }

    // Serialize UProperties for this object if requested
    if (bShouldSerializeProperties) {
        const TSharedRef<FJsonObject> Properties = SerializeObjectProperties(Object);
        ResultJson->SetObjectField(TEXT("Properties"), Properties);
    }
}
