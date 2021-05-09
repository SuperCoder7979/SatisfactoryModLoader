#include "AssetGeneration/FGAssetGenerator.h"

void FGameAssetGenerator::GenerateSatisfactoryAssets(const FString& DataJsonFilePath) {
}

/*#include "K2Node_MakeArray.h"
#include "K2Node_MakeMap.h"
#include "K2Node_MakeSet.h"
#include "K2Node_Self.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_MakeStruct.h"
#include "Misc/FileHelper.h"
#include "AssetRegistryModule.h"
#include "Kismet2/EnumEditorUtils.h"
#include "Engine/UserDefinedEnum.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Engine/UserDefinedStruct.h"
#include "EdGraphSchema_K2.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Json.h"
#include "FileHelpers.h"
#include "BlueprintCompilationManager.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EdGraphSchema_K2_Actions.h"
#include "UMGEditor/Public/WidgetBlueprint.h"

struct FPackageObjectData {
    FString ObjectPath;
    TSharedPtr<FJsonObject> SourceObject;
    bool bIsUserStruct;
    bool bIsBlueprint;
};

FPackageObjectData OBJECT_DATA_Empty;

void ParseObjectPath(const FString& ObjectPath, FString& PackageName, FString& ObjectName);
FEdGraphPinType CreateGraphPinType(const TSharedRef<FJsonObject>& PinJson);
void GatherPinTypeDependencies(const TSharedRef<FJsonObject>& PinJson, TArray<FString>& Dependencies);
bool IsObjectAlreadyLoaded(const FString& ObjectPath);
void AddDependencyIfNeeded(const FString& ObjectPath, TArray<FString>& Dependencies);

void AddDependenciesForStruct(const FPackageObjectData& ObjectData, TArray<FString>& Dependencies);
void AddDependenciesForBlueprint(const FPackageObjectData& ObjectData, TArray<FString>& Dependencies);

UPackage* CreateEnumerationFromJson(const TSharedRef<FJsonObject>& EnumJson);
UPackage* CreateGenericObject(const FPackageObjectData& PackageObjectData, bool bHasDependents);
UPackage* CreateStructFromJson(const TSharedRef<FJsonObject>& StructJson);
UPackage* CreateBlueprintFromJson(const TSharedRef<FJsonObject>& BlueprintJson, bool bHasDependents);

void InitializeBlueprint(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& BlueprintJson, bool bHasDepdents);

void PostInitializeObject(UObject* Object, const TSharedPtr<FJsonObject>& JsonObject);

struct FDeserializationContext {
    TMap<uint32, UObject*> SerializationStack;
    UObject* Outer;
};

void DeserializePropertyValue(const UProperty* Property, void* Value, const TSharedPtr<FJsonValue>& JsonValue, FDeserializationContext& Context);
UObject* DeserializeUObject(const TSharedPtr<FJsonObject>& Value, FDeserializationContext& Context);
void DeserializeFields(UStruct* Struct, void* Object, const TSharedPtr<FJsonObject>& ObjectJson, FDeserializationContext& Context);

inline uint64 GetObjectIndex(const FString& Object, uint64& LastIndex, TMap<FString, uint64>& ObjectPathToIndex) {
    if (!ObjectPathToIndex.Contains(Object)) {
        const uint64 ObjectIndex = LastIndex++;
        ObjectPathToIndex.Add(Object, ObjectIndex);
        return ObjectIndex;
    }
    return ObjectPathToIndex[Object];
}

inline uint64 AddPackageDependencies(const FString& ObjectPath, const TArray<FString>& Dependencies, SML::TopologicalSort::DirectedGraph<uint64>& DependencyGraph, uint64& LastObjectIndex, TMap<FString, uint64>& ObjectPathToIndex, TMap<uint64, bool>& HasDependentsMap) {
    const uint64 ObjectIndex = GetObjectIndex(ObjectPath, LastObjectIndex, ObjectPathToIndex);
    DependencyGraph.addNode(ObjectIndex);
    for (const FString& DependencyObjectPath : Dependencies) {
        uint64 DependencyIndex = GetObjectIndex(DependencyObjectPath, LastObjectIndex, ObjectPathToIndex);
        DependencyGraph.addNode(DependencyIndex);
        DependencyGraph.addEdge(DependencyIndex, ObjectIndex);
        HasDependentsMap.Add(DependencyIndex, true);
    }
    return ObjectIndex;
}

TArray<TSharedPtr<FJsonValue>> FindBlueprintDumps(FString FolderPath) {
    FString LoadedJsonFileText;
    TArray<TSharedPtr<FJsonValue>> Blueprints;
    TArray<FString> Result;
    IFileManager& FileManager = IFileManager::Get();
    FString FinalPath = FolderPath;
    FileManager.FindFilesRecursive(Result, *FinalPath, TEXT("*.json"), true, false);

    for (int32 i = 0; i < Result.Num(); i++)
    {
        const bool result = FFileHelper::LoadFileToString(LoadedJsonFileText, *Result[i]);
        if (!result) {
            UE_LOG(LogTemp, Error, TEXT("Failed to load FG Blueprints definitions json file"));
            return TArray<TSharedPtr<FJsonValue>>();
        }
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(LoadedJsonFileText);
        FJsonSerializer Serializer;
        TSharedPtr<FJsonValue> ResultJsonObject;
        if (!Serializer.Deserialize(Reader, ResultJsonObject)) {
            UE_LOG(LogTemp, Error, TEXT("Failed to parse FG Blueprints definitions json file"));
            return TArray<TSharedPtr<FJsonValue>>();
        }
        TSharedPtr<FJsonObject> StructJson = ResultJsonObject->AsObject();
        FString ObjectPath = StructJson->GetStringField(TEXT("Blueprint"));
        if (ObjectPath == "")
            ObjectPath = StructJson->GetStringField(TEXT("StructName"));
        if(ObjectPath != "")
        {
            FString left;
            FString right;
            ObjectPath.Split("Game/", &left, &right);
            right.Split(".", &left, &right, ESearchCase::Type::CaseSensitive, ESearchDir::Type::FromEnd);
            FString finalpath = FPaths::ProjectContentDir();
            finalpath.Append(left);
            finalpath.Append(".uasset");
            if(!FileManager.FileExists(*finalpath))
                Blueprints.Add(ResultJsonObject);
        }
    }
    return Blueprints;
}

void generateSatisfactoryAssetsInternal(const FString& DataJsonFilePath) {
    FString LoadedJsonFileText;
    SML::Logging::info(TEXT("Generating assets from dump "), *DataJsonFilePath);

    // const bool result = FFileHelper::LoadFileToString(LoadedJsonFileText, *DataJsonFilePath);
    // if (!result) {
    //     UE_LOG(LogTemp, Error, TEXT("Failed to load FG Blueprints definitions json file"));
    //     return;
    // }
    // const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(LoadedJsonFileText);
    // FJsonSerializer Serializer;
    // TSharedPtr<FJsonObject> ResultJsonObject;
    // if (!Serializer.Deserialize(Reader, ResultJsonObject)) {
    //     UE_LOG(LogTemp, Error, TEXT("Failed to parse FG Blueprints definitions json file"));
    //     return;
    // }

    SML::Logging::info(TEXT("Generating user defined enumerations..."));
    TArray<UPackage*> DefinedPackages;
    // const TArray<TSharedPtr<FJsonValue>>& UserDefinedEnums = ResultJsonObject->GetArrayField(TEXT("UserDefinedEnums"));
    TArray<TSharedPtr<FJsonValue>> UserDefinedEnums = FindBlueprintDumps(DataJsonFilePath / "BPdump" / "Enums");
    for (const TSharedPtr<FJsonValue>& Value : UserDefinedEnums) {
        UPackage* Package = CreateEnumerationFromJson(Value->AsObject().ToSharedRef());
        if (Package != nullptr) {
            Package->MarkPackageDirty();
            DefinedPackages.Add(Package);
        }
    }

    SML::TopologicalSort::DirectedGraph<uint64> DependencyGraph;
    TMap<uint64, FPackageObjectData> ObjectHeaders;
    TMap<FString, uint64> ObjectPathToIndex;
    TMap<uint64, bool> HasDependentsMap;
    uint64 LastObjectIndex = 2;

    // UEditorLoadingAndSavingUtils::SavePackages(DefinedPackages, false);
    // const TArray<TSharedPtr<FJsonValue>>& UserDefinedStructs = ResultJsonObject->GetArrayField(TEXT("UserDefinedStructs"));
    TArray<TSharedPtr<FJsonValue>> UserDefinedStructs = FindBlueprintDumps(DataJsonFilePath / "BPdump" / "Structs");


    SML::Logging::info(TEXT("Building structure dependency graph..."));
    // Gather UserStruct and Blueprint dependencies now
    for (const TSharedPtr<FJsonValue>& Value : UserDefinedStructs) {
        TSharedPtr<FJsonObject> StructJson = Value->AsObject();
        const FString& ObjectPath = StructJson->GetStringField(TEXT("StructName"));
        const FPackageObjectData& ObjectData = FPackageObjectData{ ObjectPath, StructJson, true, false };
        if (!ObjectData.ObjectPath.IsEmpty()) {
            TArray<FString> Dependencies;
            AddDependenciesForStruct(ObjectData, Dependencies);
            uint64 ObjectIndex = AddPackageDependencies(ObjectData.ObjectPath, Dependencies, DependencyGraph, LastObjectIndex, ObjectPathToIndex, HasDependentsMap);
            ObjectHeaders.Add(ObjectIndex, ObjectData);
        }
    }

    // const TArray<TSharedPtr<FJsonValue>>& Blueprints = ResultJsonObject->GetArrayField(TEXT("Blueprints"));
    TArray<TSharedPtr<FJsonValue>> Blueprints = FindBlueprintDumps(DataJsonFilePath / "BPdump" / "Blueprints");

    SML::Logging::info(TEXT("Building blueprint dependency graph..."));
    for (const TSharedPtr<FJsonValue>& Value : Blueprints) {
        TSharedPtr<FJsonObject> StructJson = Value->AsObject();
        const FString& ObjectPath = StructJson->GetStringField(TEXT("Blueprint"));
        const FPackageObjectData& ObjectData = FPackageObjectData{ ObjectPath, StructJson, false, true };
        if (!ObjectData.ObjectPath.IsEmpty()) {
            TArray<FString> Dependencies;
            AddDependenciesForBlueprint(ObjectData, Dependencies);
            uint64 ObjectIndex = AddPackageDependencies(ObjectData.ObjectPath, Dependencies, DependencyGraph, LastObjectIndex, ObjectPathToIndex, HasDependentsMap);
            ObjectHeaders.Add(ObjectIndex, ObjectData);
        }
    }

    // Apply topological sort now
    const TArray<uint64> SortingResult = SML::TopologicalSort::topologicalSort(DependencyGraph);
    SML::Logging::info(TEXT("Loading assets..."));

    // Load assets in sorted order now
    for (int32 i = 0; i < SortingResult.Num(); i++) {
        uint64 ObjectIndex = SortingResult[i];
        const FPackageObjectData& ObjectData = ObjectHeaders.FindChecked(ObjectIndex);
        check(!ObjectData.ObjectPath.IsEmpty());
        SML::Logging::info(TEXT("Loading object "), *ObjectData.ObjectPath, TEXT(" ("), i, TEXT("/"), SortingResult.Num(), TEXT(")"));
        bool bHasDependents = HasDependentsMap.Contains(ObjectIndex);
        UPackage* Package = CreateGenericObject(ObjectData, bHasDependents);
        if (Package != nullptr) {
            DefinedPackages.Add(Package);
            if (bHasDependents) {
                // If somebody depends on us, we need to save compiled package right now
                TArray<UPackage*> SelfPackageArr;
                SelfPackageArr.Add(Package);
                UEditorLoadingAndSavingUtils::SavePackages(SelfPackageArr, false);
            }
        }
    }

    // Compile queued blueprints without dependents now
    SML::Logging::info(TEXT("Flushing compilation queue.."));
    FBlueprintCompilationManager::FlushCompilationQueueAndReinstance();

    // Save packages after all assets have been re-created (not just dirty, every package created)
    UEditorLoadingAndSavingUtils::SavePackages(DefinedPackages, false);

    SML::Logging::info(TEXT("Initializing objects.."));

    // Finish blueprint construction once all dependencies have been defined and saved
    for (int32 i = 0; i < SortingResult.Num(); i++) {
        uint64 ObjectIndex = SortingResult[i];
        const FPackageObjectData& ObjectData = ObjectHeaders[ObjectIndex];
        if (ObjectData.bIsBlueprint) {
            const FString& ObjectPath = ObjectData.ObjectPath.LeftChop(2);
            UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
            bool bHasDependents = HasDependentsMap.Contains(ObjectIndex);
            check(Blueprint != nullptr);
            InitializeBlueprint(Blueprint, ObjectData.SourceObject, bHasDependents);
        }
    }

    // Compile delay initialized blueprints now
    SML::Logging::info(TEXT("Flushing compilation queue.."));
    FBlueprintCompilationManager::FlushCompilationQueueAndReinstance();
    // UEditorLoadingAndSavingUtils::SavePackages(DefinedPackages, false);
    SML::Logging::info(TEXT("Post-initializing loaded assets.."));

    // Now, post initialize delayed default properties
    for (int32 i = 0; i < SortingResult.Num(); i++) {
        uint64 ObjectIndex = SortingResult[i];
        const FPackageObjectData& ObjectData = ObjectHeaders[ObjectIndex];
        UObject* Object = nullptr;
        if (ObjectData.bIsBlueprint) {
            const FString& ObjectPath = ObjectData.ObjectPath.LeftChop(2);
            Object = LoadObject<UBlueprint>(nullptr, *ObjectPath);
        }
        if (ObjectData.bIsUserStruct) {
            const FString& ObjectPath = ObjectData.ObjectPath;
            Object = LoadObject<UUserDefinedStruct>(nullptr, *ObjectPath);
        }
        if (Object != nullptr) {
            PostInitializeObject(Object, ObjectData.SourceObject);
        } else {
            SML::Logging::error(TEXT("Object not found for object path "), *ObjectData.ObjectPath);
        }
    }

    SML::Logging::info(TEXT("Saving Packages..."));
    // Now, save all packages we defined
    UEditorLoadingAndSavingUtils::SavePackages(DefinedPackages, false);
    SML::Logging::info(TEXT("Success!"));
}

void SML::generateSatisfactoryAssets(const FString& DataJsonFilePath) {
    generateSatisfactoryAssetsInternal(DataJsonFilePath);
}

void AddDependenciesForBlueprint(const FPackageObjectData& ObjectData, TArray<FString>& Dependencies) {
    const TSharedPtr<FJsonObject> StructJson = ObjectData.SourceObject;
    // add parent class as only dependency
    const FString& ParentClass = StructJson->GetStringField(TEXT("ParentClass"));
    AddDependencyIfNeeded(ParentClass, Dependencies);

    if (StructJson->HasField(TEXT("Functions"))) {
        const TArray<TSharedPtr<FJsonValue>> Functions = StructJson->GetArrayField(TEXT("Functions"));
        for (const TSharedPtr<FJsonValue>& Value : Functions) {
            const TSharedPtr<FJsonObject>& MethodObject = Value.Get()->AsObject();
            const FString& MethodName = MethodObject->GetStringField(TEXT("Name"));

            if (MethodName.StartsWith(TEXT("ExecuteUbergraph_"))) {
                continue; // Skip generated uber graph entry point function
            }
            if (MethodObject->HasField(TEXT("SuperClass"))) {
                const FString& SuperObjectPath = MethodObject->GetStringField(TEXT("SuperClass"));
                AddDependencyIfNeeded(SuperObjectPath, Dependencies);
            }
        }
    }
}


void PostInitializeObject(UObject* Object, const TSharedPtr<FJsonObject>& JsonObject) {
    UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(Object);

    if (UserDefinedStruct) {
        const TArray<TSharedPtr<FJsonValue>>& Fields = JsonObject->GetArrayField(TEXT("Fields"));
        // re-initialize all properties to their new defaults, then request recompile
        void* AllocatedStructInstance = FMemory::Malloc(UserDefinedStruct->GetStructureSize());
        UserDefinedStruct->InitializeStruct(AllocatedStructInstance);
        FDeserializationContext DeserializationContext;
        DeserializationContext.Outer = UserDefinedStruct;

        for (const TSharedPtr<FJsonValue>& Value : Fields) {
            const FJsonObject* ValueObject = Value.Get()->AsObject().Get();
            const FString& FieldName = ValueObject->GetStringField(TEXT("Name"));
            const FGuid& VariableGuid = FStructureEditorUtils::GetGuidFromPropertyName(*FieldName);
            if (ValueObject->HasField(TEXT("Value"))) {
                UProperty* VariableProperty = UserDefinedStruct->FindPropertyByName(*FieldName);
                FStructVariableDescription* VariableDesc = FStructureEditorUtils::GetVarDescByGuid(UserDefinedStruct, VariableGuid);
                check(VariableDesc != nullptr && VariableProperty != nullptr);
                void* PropertyValuePtr = VariableProperty->ContainerPtrToValuePtr<void>(AllocatedStructInstance);
                const TSharedPtr<FJsonValue>& FieldValue = ValueObject->Values.FindChecked(TEXT("Value"));
                DeserializePropertyValue(VariableProperty, PropertyValuePtr, FieldValue, DeserializationContext);
                // Now, export it's value as text
                FString ExportedTextValue;
                VariableProperty->ExportTextItem(ExportedTextValue, PropertyValuePtr, PropertyValuePtr, nullptr, 0);
                if (VariableDesc->DefaultValue != ExportedTextValue) {
                    VariableDesc->DefaultValue = ExportedTextValue;
                }
            }
        }
        FMemory::Free(AllocatedStructInstance);
        // request structure recompile when we actually set some default values
        FStructureEditorUtils::OnStructureChanged(UserDefinedStruct, FStructureEditorUtils::EStructureEditorChangeInfo::DefaultValueChanged);
    }

    UBlueprint* Blueprint = Cast<UBlueprint>(Object);
    if (Blueprint && JsonObject->HasField(TEXT("Fields")) && !Blueprint->IsA<UAnimBlueprint>()) {
        const TArray<TSharedPtr<FJsonValue>> Fields = JsonObject->GetArrayField(TEXT("Fields"));
        // Blueprint should have been already compiled at that moment
        check(Blueprint->GeneratedClass != nullptr);
        UClass* BlueprintGeneratedClass = Blueprint->GeneratedClass;
        UObject* ClassDefaultObject = BlueprintGeneratedClass->GetDefaultObject();
        FDeserializationContext DeserializationContext;
        DeserializationContext.Outer = ClassDefaultObject;
        DeserializationContext.SerializationStack.Add(0, ClassDefaultObject);

        bool bSetDefaultValues = false;
        for (const TSharedPtr<FJsonValue>& Value : Fields) {
            const TSharedPtr<FJsonObject>& FieldObject = Value.Get()->AsObject();
            const FString& FieldName = FieldObject->GetStringField(TEXT("Name"));
            if (FieldName == TEXT("UberGraphFrame"))
                continue; // Skip generated uber graph frame field
            if (FieldObject->HasField(TEXT("Value"))) {
                UProperty* ResultProperty = BlueprintGeneratedClass->FindPropertyByName(*FieldName);
                SML::Logging::info(TEXT("BP "), *BlueprintGeneratedClass->GetPathName(), TEXT(" "), *FieldName);
                check(ResultProperty != nullptr);
                const TSharedPtr<FJsonValue>& FieldValue = FieldObject->Values.FindChecked(TEXT("Value"));
                void* PropertyValuePtr = ResultProperty->ContainerPtrToValuePtr<void>(ClassDefaultObject);
                DeserializePropertyValue(ResultProperty, PropertyValuePtr, FieldValue, DeserializationContext);
                bSetDefaultValues = true;
            }
        }
        if (bSetDefaultValues) {
            // Mark default class object package as dirty now
            ClassDefaultObject->MarkPackageDirty();
        }
    }
    Object->MarkPackageDirty();
}

void AddDependenciesForStruct(const FPackageObjectData& ObjectData, TArray<FString>& Dependencies) {
    const TArray<TSharedPtr<FJsonValue>>& Fields = ObjectData.SourceObject->GetArrayField(TEXT("Fields"));
    for (const TSharedPtr<FJsonValue>& Value : Fields) {
        const FJsonObject* ValueObject = Value.Get()->AsObject().Get();
        TSharedPtr<FJsonObject> PinData = ValueObject->GetObjectField(TEXT("PinType"));
        GatherPinTypeDependencies(PinData.ToSharedRef(), Dependencies);
    }
}

UUserDefinedStruct* CreateUserDefinedStructWithGUID(UObject* InParent, const FName& Name, EObjectFlags Flags, const FGuid& PredefinedGuid) {
    UUserDefinedStruct* Struct = NewObject<UUserDefinedStruct>(InParent, Name, Flags);
    check(Struct);
    Struct->EditorData = NewObject<UUserDefinedStructEditorData>(Struct, NAME_None, RF_Transactional);
    check(Struct->EditorData);

    Struct->Guid = PredefinedGuid;
    Struct->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
    Struct->Bind();
    Struct->StaticLink(true);
    Struct->Status = UDSS_Error;
    return Struct;
}

FGuid AddStructVariableWithName(UUserDefinedStruct* Struct, const FEdGraphPinType& VarType, const FString& PredefinedUniqueName) {
    FString ErrorMessage;
    const bool Result = FStructureEditorUtils::CanHaveAMemberVariableOfType(Struct, VarType, &ErrorMessage);
    if (!Result) {
        SML::Logging::error(TEXT("Error Adding Field Because of: "), *ErrorMessage);
        check(false);
    }

    const FGuid& Guid = FStructureEditorUtils::GetGuidFromPropertyName(*PredefinedUniqueName);
    int32 FirstUnderscoreIndex;
    PredefinedUniqueName.FindChar('_', FirstUnderscoreIndex);
    check(FirstUnderscoreIndex != INDEX_NONE);
    const FString& DisplayName = PredefinedUniqueName.Mid(0, FirstUnderscoreIndex);
    const FName VarName = FName(*PredefinedUniqueName);

    // Only add variable if it doesn't exist already
    if (FStructureEditorUtils::GetVarDescByGuid(Struct, Guid) == nullptr && !FindObject<UObject>(Struct, *PredefinedUniqueName, false)) {
        FStructureEditorUtils::ModifyStructData(Struct);
        FStructVariableDescription NewVar;
        NewVar.VarName = VarName;
        NewVar.FriendlyName = DisplayName;
        NewVar.SetPinType(VarType);
        NewVar.VarGuid = Guid;
        NewVar.bDontEditoOnInstance = false;
        NewVar.bInvalidMember = false;
        FStructureEditorUtils::GetVarDesc(Struct).Add(NewVar);
    }

    // Do not fire struct change event now, fire it only after all fields have been added
    return Guid;
}

UPackage* CreateGenericObject(const FPackageObjectData& PackageObjectData, bool bHasDependents) {
    if (PackageObjectData.bIsUserStruct) {
        return CreateStructFromJson(PackageObjectData.SourceObject.ToSharedRef());
    }
    if (PackageObjectData.bIsBlueprint) {
        return CreateBlueprintFromJson(PackageObjectData.SourceObject.ToSharedRef(), bHasDependents);
    }
    return nullptr;
}

template <typename T>
T* CreateMakeContainerNode(UEdGraph* Graph, const UK2Node* OriginNode) {
    FGraphNodeCreator<T> ArrayNodeCreator(*Graph);
    T* NewArrayNode = ArrayNodeCreator.CreateNode();
    NewArrayNode->NodePosX = OriginNode->NodePosX - NewArrayNode->NodeWidth - 128;
    NewArrayNode->NodePosY = OriginNode->NodePosY - NewArrayNode->NodeHeight - 256;
    ArrayNodeCreator.Finalize();
    return NewArrayNode;
}

void SetPinDefaultGeneratedValue(UEdGraph* Graph, const UK2Node* OriginNode, const UEdGraphSchema_K2* Schema, UEdGraphPin* NewPin) {
    Schema->SetPinAutogeneratedDefaultValueBasedOnType(NewPin);
    if (!NewPin->PinType.IsContainer()) {
        const FName& PinCategory = NewPin->PinType.PinCategory;
        // For class pins, we set just that class as default value
        if (PinCategory == UEdGraphSchema_K2::PC_Class || PinCategory == UEdGraphSchema_K2::PC_SoftClass) {
            UClass* TargetClass = Cast<UClass>(NewPin->PinType.PinSubCategoryObject);
            check(TargetClass);
            Schema->TrySetDefaultObject(*NewPin, TargetClass);
        }

        // For objects and interfaces, we use pure cast from Self node
        if (PinCategory == UEdGraphSchema_K2::PC_Object || PinCategory == UEdGraphSchema_K2::PC_Interface) {
            UClass* TargetClass = Cast<UClass>(NewPin->PinType.PinSubCategoryObject);
            check(TargetClass);

            FGraphNodeCreator<UK2Node_DynamicCast> NodeCreator(*Graph);
            UK2Node_DynamicCast* CastNode = NodeCreator.CreateNode();
            CastNode->TargetType = TargetClass;
            CastNode->NodePosX = OriginNode->NodePosX - CastNode->NodeWidth - 128;
            CastNode->NodePosY = OriginNode->NodePosY - CastNode->NodeHeight - 256;
            NodeCreator.Finalize();
            CastNode->SetPurity(true);

            FGraphNodeCreator<UK2Node_Self> SelfNodeCreator(*Graph);
            UK2Node_Self* SelfNode = SelfNodeCreator.CreateNode();
            SelfNode->NodePosX = CastNode->NodePosX - SelfNode->NodeWidth - 64;
            SelfNode->NodePosY = CastNode->NodePosY;
            SelfNodeCreator.Finalize();

            SelfNode->GetPinAt(0)->MakeLinkTo(CastNode->GetCastSourcePin());
            CastNode->GetCastResultPin()->MakeLinkTo(NewPin);
            SelfNode->ReconstructNode();
            CastNode->ReconstructNode();

        }

        // For Structs, we create Default MakeNodes if possible
        if (PinCategory == UEdGraphSchema_K2::PC_Struct) {
            UScriptStruct* TargetStruct = Cast<UScriptStruct>(NewPin->PinType.PinSubCategoryObject);
            check(TargetStruct);
            if (!TargetStruct->HasMetaData(TEXT("HasNativeMake"))) {
                FGraphNodeCreator<UK2Node_MakeStruct> StructNodeCreator(*Graph);
                UK2Node_MakeStruct * MakeNode = StructNodeCreator.CreateNode();
                MakeNode->StructType = TargetStruct;
                MakeNode->NodePosX = OriginNode->NodePosX - MakeNode->NodeWidth - 64;
                MakeNode->NodePosY = OriginNode->NodePosY - MakeNode->NodeHeight + 64;
                StructNodeCreator.Finalize();

                // Ensure all input pins have correct values
                for (UEdGraphPin* GraphPin : MakeNode->GetAllPins()) {
                    if (GraphPin->Direction == EEdGraphPinDirection::EGPD_Input) {
                        SetPinDefaultGeneratedValue(Graph, MakeNode, Schema, GraphPin);
                    }
                }

                // Find output pin and connect it to our input pin
                for (UEdGraphPin* GraphPin : MakeNode->GetAllPins()) {
                    if (GraphPin->Direction == EEdGraphPinDirection::EGPD_Output) {
                        GraphPin->MakeLinkTo(NewPin);
                        break;
                    }
                }
            } else {
                // MetaData Attribute Value: Engine.KismetMathLibrary.MakeVector
                const FString NativeMakePath = TargetStruct->GetMetaData(TEXT("HasNativeMake"));
                int32 LastSeparatorIndex;
                check(NativeMakePath.FindLastChar('.', LastSeparatorIndex));

                // Class Path: /Script/Engine.KismetMathLibrary
                const FString ClassPath = FString(TEXT("/Script/")).Append(NativeMakePath.Mid(0, LastSeparatorIndex));
                UClass* FunctionClass = LoadObject<UClass>(nullptr, *ClassPath);
                checkf(FunctionClass, TEXT("Failed to load UClass from HasNativeMake. Class Path: %s"), *ClassPath)

                // Method Name: MakeVector
                const FString FunctionName = NativeMakePath.Mid(LastSeparatorIndex + 1);
                UFunction* TargetFunction = FunctionClass->FindFunctionByName(*FunctionName);
                checkf(TargetFunction, TEXT("Function specified in HasNativeMake not found: %s"), *FunctionName);

                FGraphNodeCreator<UK2Node_CallFunction> StructNodeCreator(*Graph);
                UK2Node_CallFunction* MakeNode = StructNodeCreator.CreateNode();
                MakeNode->SetFromFunction(TargetFunction);
                MakeNode->NodePosX = OriginNode->NodePosX - MakeNode->NodeWidth - 128;
                MakeNode->NodePosY = OriginNode->NodePosY - MakeNode->NodeHeight - 256;
                StructNodeCreator.Finalize();

                // Ensure all input pins have correct values
                for (UEdGraphPin* GraphPin : MakeNode->GetAllPins()) {
                    if (GraphPin->Direction == EEdGraphPinDirection::EGPD_Input) {
                        SetPinDefaultGeneratedValue(Graph, MakeNode, Schema, GraphPin);
                    }
                }
                UEdGraphPin* ReturnValuePin = MakeNode->GetReturnValuePin();
                check(ReturnValuePin);
                ReturnValuePin->MakeLinkTo(NewPin);
            }
        }
    } else if (OriginNode->IsA<UK2Node_MakeStruct>()) {
        // For some reason MakeStruct nodes don't support Map/Array literals directly, so we need Make Map/Array nodes for them
        UK2Node_MakeContainer* NewContainerNode = nullptr;
        if (NewPin->PinType.IsArray()) {
            NewContainerNode = CreateMakeContainerNode<UK2Node_MakeArray>(Graph, OriginNode);
        } else if (NewPin->PinType.IsMap()) {
            NewContainerNode = CreateMakeContainerNode<UK2Node_MakeMap>(Graph, OriginNode);
        } else if (NewPin->PinType.IsSet()) {
            NewContainerNode = CreateMakeContainerNode<UK2Node_MakeSet>(Graph, OriginNode);
        }
        if (NewContainerNode) {
            UEdGraphPin* ArrayOutPin = NewContainerNode->GetOutputPin();
            ArrayOutPin->MakeLinkTo(NewPin);
            NewContainerNode->ReconstructNode();
        }
    }
}

void DefineCustomFunctionFromJson(UEdGraph* Graph, UK2Node_EditablePinBase* FunctionNode, const TSharedPtr<FJsonObject> FunctionObject) {
    const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());

    // Add parameters specified in the json for the function entry
    if (FunctionObject->HasField(TEXT("Arguments"))) {
        const TArray<TSharedPtr<FJsonValue>>& Arguments = FunctionObject->GetArrayField(TEXT("Arguments"));
        for (const TSharedPtr<FJsonValue> Value : Arguments) {
            // initialize properties for argument
            const TSharedPtr<FJsonObject>& ArgumentObject = Value->AsObject();
            const FString& ParameterName = ArgumentObject->GetStringField(TEXT("Name"));
            if (ParameterName == TEXT("__WorldContext"))
                continue; // World context pin is created automatically in AllocateDefaultPins()
            const FEdGraphPinType& GraphPinType = CreateGraphPinType(ArgumentObject->GetObjectField(TEXT("PinType")).ToSharedRef());
            // Create argument pin from given information
            FunctionNode->CreateUserDefinedPin(*ParameterName, GraphPinType, EGPD_Output, false);
        }
    }

    // generate function result node if are generating function and have outputs
    if (FunctionNode->IsA<UK2Node_FunctionEntry>() && FunctionObject->HasField(TEXT("Outputs"))) {
        const TArray<TSharedPtr<FJsonValue>>& Outputs = FunctionObject->GetArrayField(TEXT("Outputs"));
        if (Outputs.Num() > 0) {
            // Initialize node with default pins and location
            FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(*Graph);
            UK2Node_FunctionResult* ReturnNode = NodeCreator.CreateNode();
            ReturnNode->NodePosX = FunctionNode->NodePosX + FunctionNode->NodeWidth + 256;
            ReturnNode->NodePosY = FunctionNode->NodePosY;
            NodeCreator.Finalize();

            for (const TSharedPtr<FJsonValue> Value : Outputs) {
                const TSharedPtr<FJsonObject>& ArgumentObject = Value->AsObject();
                const FString& ReturnPinName = ArgumentObject->GetStringField(TEXT("Name"));
                const FEdGraphPinType& GraphPinType = CreateGraphPinType(ArgumentObject->GetObjectField(TEXT("PinType")).ToSharedRef());
                UEdGraphPin* NewPin = ReturnNode->CreateUserDefinedPin(*ReturnPinName, GraphPinType, EGPD_Input, false);
                SetPinDefaultGeneratedValue(Graph, ReturnNode, Schema, NewPin);
            }

            Schema->SetNodeMetaData(ReturnNode, FNodeMetadata::DefaultGraphNode);
            // Auto-connect the pins for entry and exit, so that by default the signature is properly generated
            UEdGraphPin* ResultNodeExec = Schema->FindExecutionPin(*ReturnNode, EGPD_Input);
            UEdGraphPin* NextExec = Schema->FindExecutionPin(*FunctionNode, EGPD_Output);
            if (ResultNodeExec && NextExec) {
                NextExec->MakeLinkTo(ResultNodeExec);
            }
            ReturnNode->ReconstructNode();

        }
    }
}

bool CanPlaceFunctionAsEventJson(const TSharedPtr<FJsonObject>& FunctionObject) {
    const int32 FunctionFlags = FunctionObject->GetIntegerField(TEXT("FunctionFlags"));
    if ((FunctionFlags & FUNC_Const) > 0) {
        return false;
    }
    if (FunctionObject->HasField(TEXT("Outputs"))) {
        const TArray<TSharedPtr<FJsonValue>>& Outputs = FunctionObject->GetArrayField(TEXT("Outputs"));
        return Outputs.Num() == 0;
    }
    return true;
}

UK2Node* CreateCustomFunction(UBlueprint* Blueprint, const FString& FunctionName, const TSharedPtr<FJsonObject>& FunctionObject) {
    UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
    const uint32 FunctionFlags = FunctionObject->GetIntegerField(TEXT("FunctionFlags"));

    if (CanPlaceFunctionAsEventJson(FunctionObject) && EventGraph) {
        UK2Node_CustomEvent* NewNode = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_CustomEvent>(
            EventGraph,
            EventGraph->GetGoodPlaceForNewNode(),
            EK2NewNodeFlags::None,
            [FunctionName](UK2Node_CustomEvent* NewInstance) {
            NewInstance->bIsEditable = true;
            NewInstance->CustomFunctionName = *FunctionName;
        });
        NewNode->FunctionFlags |= FunctionFlags;
        DefineCustomFunctionFromJson(EventGraph, NewNode, FunctionObject);
        return NewNode;
    } else {
        UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, *FunctionName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddFunctionGraph(Blueprint, FunctionGraph, false, static_cast<UClass*>(nullptr));
        TArray<UK2Node_FunctionEntry*> EntryNodes;
        FunctionGraph->GetNodesOfClass(EntryNodes);
        UK2Node_FunctionEntry* EntryNode = EntryNodes[0];
        EntryNode->AddExtraFlags(FunctionFlags);
        DefineCustomFunctionFromJson(FunctionGraph, EntryNode, FunctionObject);
        return EntryNode;
    }
}

UK2Node* CreateFunctionOverride(UBlueprint* Blueprint, UFunction* OverrideFunc) {
    UClass* FunctionClass = Cast<UClass>(OverrideFunc->GetOuter());
    UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
    if (UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(OverrideFunc) && EventGraph) {
        // Add to event graph
        FName EventName = OverrideFunc->GetFName();
        UK2Node_Event* ExistingNode = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, FunctionClass, EventName);

        if (!ExistingNode) {
            return FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_Event>(
                EventGraph,
                EventGraph->GetGoodPlaceForNewNode(),
                EK2NewNodeFlags::SelectNewNode,
                [EventName, FunctionClass](UK2Node_Event* NewInstance) {
                NewInstance->EventReference.SetExternalMember(EventName, FunctionClass);
                NewInstance->bOverrideFunction = true;
            });
        }
    } else {
        // Implement the function graph
        UEdGraph* const NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, OverrideFunc->GetFName(), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddFunctionGraph(Blueprint, NewGraph, false, FunctionClass);
        TArray<UK2Node_FunctionEntry*> ResultArray;
        NewGraph->GetNodesOfClass<UK2Node_FunctionEntry>(ResultArray);
        return ResultArray.Num() > 0 ? ResultArray[0] : nullptr;
    }
    return nullptr;
}

bool IsInterfaceImplemented(UBlueprint* Blueprint, UClass* InterfaceClass) {
    for (int32 i = 0; i < Blueprint->ImplementedInterfaces.Num(); i++) {
        if (Blueprint->ImplementedInterfaces[i].Interface == InterfaceClass) {
            return true;
        }
    }
    return false;
}

void GenerateBlueprintSCS(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& SCSJson) {
    USimpleConstructionScript* SimpleConstructionScript = Blueprint->SimpleConstructionScript;
    check(SimpleConstructionScript != nullptr);
    const TArray<TSharedPtr<FJsonValue>>& AllNodes = SCSJson->GetArrayField(TEXT("AllNodes"));
    TMap<uint32, USCS_Node*> NodeToIndexMap;
    // First pass: construct all nodes
    for (const TSharedPtr<FJsonValue>& NodeJson : AllNodes) {
        // Read basic node information here
        const TSharedPtr<FJsonObject> NodeObject = NodeJson->AsObject();
        uint32 NodeIndex = NodeObject->GetIntegerField(TEXT("NodeIndex"));
        const FString ComponentClassString = NodeObject->GetStringField(TEXT("ComponentClass"));
        const FString VariableName = NodeObject->GetStringField(TEXT("VariableName"));
        UClass* ComponentClass = LoadObject<UClass>(nullptr, *ComponentClassString);
        check(ComponentClass != nullptr);
        // Initialize node with predefined name and component class
        // Copy over predefined variable GUID so each generation setup has these the same (since editor references variables by GUIDs)
        USCS_Node* NewlyAllocatedNode = SimpleConstructionScript->CreateNode(ComponentClass, *VariableName);
        FGuid::Parse(NodeObject->GetStringField(TEXT("VariableGuid")), NewlyAllocatedNode->VariableGuid);

        // Copy field values over from the serialized template to the newly created one
        // It is much safer than reassigning template object with custom name/flags or other unwanted stuff
        const TSharedPtr<FJsonObject> ComponentTemplateObj = NodeObject->GetObjectField(TEXT("ComponentTemplate"));
        UActorComponent* ComponentTemplate = NewlyAllocatedNode->ComponentTemplate;
        FDeserializationContext DeserializationContext;
        DeserializationContext.Outer = ComponentTemplate;
        DeserializeFields(ComponentClass, ComponentTemplate, ComponentTemplateObj, DeserializationContext);

        // Copy parenting information right from dump. Although it's not strictly required
        // And can be inferred from the parent node/name most of the time, that way we ensure it will always be consistent with the game's data
        NewlyAllocatedNode->ParentComponentOrVariableName = *NodeObject->GetStringField(TEXT("ParentComponentOrVariableName"));
        NewlyAllocatedNode->ParentComponentOwnerClassName = *NodeObject->GetStringField(TEXT("ParentComponentOwnerClassName"));
        NewlyAllocatedNode->bIsParentComponentNative = NodeObject->GetBoolField(TEXT("ParentComponentNative"));
        NewlyAllocatedNode->AttachToName = *NodeObject->GetStringField(TEXT("AttachToName"));
        // Write node to our list
        NodeToIndexMap.Add(NodeIndex, NewlyAllocatedNode);
    }

    // Second pass: Create child node hierarchy
    for (const TSharedPtr<FJsonValue>& NodeJson : AllNodes) {
        // Read basic node information here
        const TSharedPtr<FJsonObject> NodeObject = NodeJson->AsObject();
        const TArray<TSharedPtr<FJsonValue>> ChildNodes = NodeObject->GetArrayField(TEXT("ChildNodes"));
        const uint32 NodeIndex = NodeObject->GetIntegerField(TEXT("NodeIndex"));
        USCS_Node* ParentNode = NodeToIndexMap.FindChecked(NodeIndex);
        for (const TSharedPtr<FJsonValue>& ChildNodeJson : ChildNodes) {
            const uint32 ChildIndex = static_cast<uint32>(ChildNodeJson->AsNumber());
            USCS_Node* ChildNode = NodeToIndexMap.FindChecked(ChildIndex);
            ParentNode->AddChildNode(ChildNode);
        }
    }
    // Final pass: Initialize root nodes
    const TArray<TSharedPtr<FJsonValue>>& RootNodes = SCSJson->GetArrayField(TEXT("RootNodes"));
    for (const TSharedPtr<FJsonValue>& RootNodeJson : RootNodes) {
        const uint32 NodeIndex = static_cast<uint32>(RootNodeJson->AsNumber());
        USCS_Node* RootNode = NodeToIndexMap.FindChecked(NodeIndex);
        SimpleConstructionScript->AddNode(RootNode);
    }
}

UWidgetAnimation* DeserializeAnimation(UWidgetBlueprint* WidgetBlueprint, const FString& AnimationName, const TSharedPtr<FJsonObject>& MovieSceneObject) {
    UWidgetAnimation* NewAnimation = NewObject<UWidgetAnimation>(WidgetBlueprint, FName(), RF_Transactional);
    NewAnimation->Rename(*AnimationName);
    NewAnimation->SetDisplayLabel(AnimationName);
    NewAnimation->MovieScene = NewObject<UMovieScene>(NewAnimation, *AnimationName, RF_Transactional);
    // Apply default setting as from AnimationTabSummoner.cpp
    NewAnimation->MovieScene->SetDisplayRate(FFrameRate(20, 1));
    float InTime = 0.0f;
    float OutTime = 5.0f;
    const FFrameTime InFrame = InTime * NewAnimation->MovieScene->GetTickResolution();
    const FFrameTime OutFrame = OutTime * NewAnimation->MovieScene->GetTickResolution();
    NewAnimation->MovieScene->SetPlaybackRange(TRange<FFrameNumber>(InFrame.FrameNumber, OutFrame.FrameNumber + 1));
    NewAnimation->MovieScene->GetEditorData().WorkStart = InTime;
    NewAnimation->MovieScene->GetEditorData().WorkEnd = OutTime;
    // Deserialize animation object fields now
    FDeserializationContext AnimationContext;
    AnimationContext.Outer = NewAnimation->MovieScene;
    AnimationContext.SerializationStack.Add(0, NewAnimation->MovieScene);
    DeserializeFields(UMovieScene::StaticClass(), NewAnimation->MovieScene, MovieSceneObject, AnimationContext);
    return NewAnimation;
}

void InitializeBlueprint(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& BlueprintJson, bool bHasDependents) {
    SML::Logging::info(TEXT("Initializing blueprint "), *Blueprint->GetPathName());
    const FString& ParentClassPath = BlueprintJson->GetStringField(TEXT("ParentClass"));
    TArray<UClass*> OverrideFunctionSources;
    UClass* ParentClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ParentClassPath));
    check(ParentClass != nullptr);
    OverrideFunctionSources.Add(ParentClass);

    // Implement required interfaces for generated blueprint
    if (BlueprintJson->HasField(TEXT("ImplementedInterfaces"))) {
        const TArray<TSharedPtr<FJsonValue>> ImplementedInterfaces = BlueprintJson->GetArrayField(TEXT("ImplementedInterfaces"));
        for (const TSharedPtr<FJsonValue>& Value : ImplementedInterfaces) {
            const FString& InterfaceName = Value.Get()->AsString();
            // Ensure that class exists and is in fact interface
            UClass* InterfaceClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *InterfaceName));
            check(InterfaceClass != nullptr && InterfaceClass->HasAnyClassFlags(EClassFlags::CLASS_Interface));
            // Only implement interface if it isn't already implemented
            if (!IsInterfaceImplemented(Blueprint, InterfaceClass)) {
                FBlueprintEditorUtils::ImplementNewInterface(Blueprint, *InterfaceName);
            }
            OverrideFunctionSources.Add(InterfaceClass);
        }
    }

    // Generate construction script before fields
    if (BlueprintJson->HasField(TEXT("ConstructionScript"))) {
        const TSharedPtr<FJsonObject> ConstructionScript = BlueprintJson->GetObjectField(TEXT("ConstructionScript"));
        GenerateBlueprintSCS(Blueprint, ConstructionScript);
    }

    // Generate blueprint own properties (only properties with specified pin type)
    if (BlueprintJson->HasField(TEXT("Fields")) && !Blueprint->IsA<UAnimBlueprint>()) {
        const TArray<TSharedPtr<FJsonValue>> Fields = BlueprintJson->GetArrayField(TEXT("Fields"));
        for (const TSharedPtr<FJsonValue>& Value : Fields) {
            const TSharedPtr<FJsonObject>& FieldObject = Value.Get()->AsObject();
            const FString& FieldName = FieldObject->GetStringField(TEXT("Name"));
            if (FieldName == TEXT("UberGraphFrame"))
                continue; // Skip generated uber graph frame field
            if (FBlueprintEditorUtils::FindMemberVariableGuidByName(Blueprint, *FieldName).IsValid())
                continue; // Skip adding variable if it is already there
            if (FieldObject->HasField(TEXT("PinType"))) {
                // It is blueprint's own variable, generate it
                FEdGraphPinType PinType = CreateGraphPinType(FieldObject->GetObjectField(TEXT("PinType")).ToSharedRef());
                SML::Logging::info(TEXT("Adding member variable "), *FieldName, TEXT(" TO BP "), *Blueprint->GetPathName());
                FBlueprintEditorUtils::AddMemberVariable(Blueprint, *FieldName, PinType);
            }
        }
    }

    // Generate dummy method overrides & own methods now
    if (BlueprintJson->HasField(TEXT("Functions"))) {
        const TArray<TSharedPtr<FJsonValue>> Functions = BlueprintJson->GetArrayField(TEXT("Functions"));
        for (const TSharedPtr<FJsonValue>& Value : Functions) {
            const TSharedPtr<FJsonObject>& MethodObject = Value.Get()->AsObject();
            const FString& MethodName = MethodObject->GetStringField(TEXT("Name"));
            if (MethodName.StartsWith(TEXT("ExecuteUbergraph_"))) {
                continue; // Skip generated uber graph entry point function
            }
            UK2Node* ResultEntryNode = nullptr;
            if (MethodObject->HasField(TEXT("SuperClass"))) {
                // if this is override function, set super class for it
                const FString& SuperObjectPath = MethodObject->GetStringField(TEXT("SuperClass"));
                UClass* ParentFunctionClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *SuperObjectPath));
                check(ParentFunctionClass != nullptr);
                if (ParentFunctionClass->HasAllClassFlags(EClassFlags::CLASS_Interface)) {
                    continue; // Do not create overrides manually from interface classes - already implemented via ImplementNewInterface
                }
                if (FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, ParentFunctionClass, *MethodName) != nullptr) {
                    continue; // Function override is already implemented, skip it
                }
                UEdGraph* ExistingGraph = FindObject<UEdGraph>(Blueprint, *MethodName);
                if (ExistingGraph != nullptr) {
                    continue; // Graph is already there, no need to create it again
                }
                UFunction* Function = ParentFunctionClass->FindFunctionByName(*MethodName);
                check(Function != nullptr);
                ResultEntryNode = CreateFunctionOverride(Blueprint, Function);
            } else {
                ResultEntryNode = CreateCustomFunction(Blueprint, MethodName, MethodObject);
            }
            if (ResultEntryNode != nullptr) {
                ResultEntryNode->OnUpdateCommentText(TEXT("Function Stub: Implementation not available."));
                ResultEntryNode->ReconstructNode();
            }
        }

    }

    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint);
    if (WidgetBlueprint != nullptr) {
        // Deserialize widget tree with all nested widgets
        UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
        const TSharedPtr<FJsonObject>& RootWidgetObject = BlueprintJson->GetObjectField(TEXT("WidgetTreeRootWidget"));
        FDeserializationContext DeserializationContext;
        DeserializationContext.Outer = WidgetTree;
        DeserializationContext.SerializationStack.Add(0, WidgetTree);
        UWidget* RootWidget = Cast<UWidget>(DeserializeUObject(RootWidgetObject, DeserializationContext));
        if (RootWidget != nullptr) {
            WidgetTree->RootWidget = RootWidget;
        }
        // Clear out widgets with duplicate names
        // TODO should we merge them actually? I don't know, it's definitely possible,
        // TODO but we can run into issues with nested widget trees
        // TODO for now we just ensure unique names to avoid duplicate fields
        TArray<FString> DuplicateNames;
        TArray<UWidget*> Widgets = WidgetBlueprint->GetAllSourceWidgets();
        for (UWidget* Widget : Widgets) {
            const FString WidgetName = Widget->GetName();
            FString NewWidgetName = WidgetName;
            while (DuplicateNames.Contains(NewWidgetName)) {
                NewWidgetName.Append(TEXT("_DUP"));
                SML::Logging::info(TEXT("Trying widget name "), *NewWidgetName);
            }
            DuplicateNames.Add(NewWidgetName);
            if (WidgetName != NewWidgetName) {
                SML::Logging::info(TEXT("Renaming widget "), *Widget->GetPathName(), TEXT(" to avoid duplication issues"));
                Widget->Rename(*NewWidgetName);
            }
        }
    }

    Blueprint->MarkPackageDirty();
    if (bHasDependents) {
        // compile immediately if we have dependents
        FBPCompileRequest CompileRequest{ Blueprint, EBlueprintCompileOptions::None, nullptr };
        FBlueprintCompilationManager::CompileSynchronously(CompileRequest);
    } else {
        // Otherwise, queue recompilation
        FBlueprintCompilationManager::QueueForCompilation(Blueprint);
    }
}

UBlueprint* CreateBlueprint(UPackage* Package, const FString& Name, UClass* ParentClass, UClass* BlueprintClass, UClass* GeneratedClassClass) {
    EBlueprintType BlueprintType = BPTYPE_Normal;
    if (ParentClass == UBlueprintFunctionLibrary::StaticClass()) {
        BlueprintType = BPTYPE_FunctionLibrary;
    } else if (ParentClass == UInterface::StaticClass()) {
        BlueprintType = BPTYPE_Interface;
    }
    return FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, *Name, BlueprintType, BlueprintClass, GeneratedClassClass, NAME_None);
}

UBlueprint* CreateNormalBlueprint(UPackage* Package, const FString& Name, UClass* ParentClass) {
    return CreateBlueprint(Package, Name, ParentClass, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
}

UWidgetBlueprint* CreateWidgetBlueprint(UPackage* Package, const FString& Name, UClass* ParentClass) {
    UBlueprint* Blueprint = CreateBlueprint(Package, Name, ParentClass, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass());
    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint);
    check(WidgetBlueprint != nullptr);
    // TODO not always correct for inherited widgets, need to figure out when exactly it is needed
    if (WidgetBlueprint->WidgetTree->RootWidget == nullptr) {
    //     UWidget* Root = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(UCanvasPanel::StaticClass());
    //     WidgetBlueprint->WidgetTree->RootWidget = Root;
    }
    return WidgetBlueprint;
}

UAnimBlueprint* CreateAnimationBlueprint(UPackage* Package, const FString& Name, UClass* ParentClass, const TSharedRef<FJsonObject>& BlueprintJson) {
    UBlueprint* Blueprint = CreateBlueprint(Package, Name, ParentClass, UAnimBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Blueprint);
    check(AnimBlueprint != nullptr);
    // It's fine to set TargetSkeleton here since USkeleton object referenced is asset file itself
    USkeleton* TargetSkeleton = nullptr;
    if (BlueprintJson->HasField(TEXT("TargetSkeleton"))) {
        const TSharedPtr<FJsonObject> TargetSkeletonValue = BlueprintJson->GetObjectField(TEXT("TargetSkeleton"));
        // Should always have "$AssetPath" key, otherwise this entry is invalid, since BP cannot reference non-asset skeleton
        check(TargetSkeletonValue->HasField(TEXT("$AssetPath")));
        const FString SkeletonAssetName = TargetSkeletonValue->GetStringField(TEXT("$AssetPath"));
        TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonAssetName);
    }
    if (TargetSkeleton == nullptr) {
        // Fallback to default editor skeleton if original asset wasn't found
        TargetSkeleton = LoadObject<USkeleton>(nullptr, TEXT("/Engine/EditorMeshes/SkeletalMesh/DefaultSkeletalMesh_Skeleton.DefaultSkeletalMesh_Skeleton"));
    }
    check(TargetSkeleton);
    AnimBlueprint->TargetSkeleton = TargetSkeleton;
    return AnimBlueprint;
}


UBlueprint* CreateBlueprint(UPackage* Package, const FString& Name, const TSharedRef<FJsonObject>& BlueprintJson) {
    const FString BlueprintKind = BlueprintJson->GetStringField(TEXT("BlueprintKind"));
    const FString& ParentClassPath = BlueprintJson->GetStringField(TEXT("ParentClass"));
    UClass* ParentClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ParentClassPath));
    check(ParentClass != nullptr);
    if (BlueprintKind == TEXT("Normal")) {
        return CreateNormalBlueprint(Package, Name, ParentClass);
    } else if (BlueprintKind == TEXT("Widget")) {
        return CreateWidgetBlueprint(Package, Name, ParentClass);
    } else if (BlueprintKind == TEXT("Animation")) {
        return CreateAnimationBlueprint(Package, Name, ParentClass, BlueprintJson);
    }
    SML::Logging::fatal(TEXT("Encountered unknown blueprint type: "), *BlueprintKind);
    return nullptr;
}

UPackage* CreateBlueprintFromJson(const TSharedRef<FJsonObject>& BlueprintJson, bool bHasDependents) {
    // Remove _C from the object name because we got a class name, while we are creating blueprint object itself
    const FString& ObjectPath = BlueprintJson->GetStringField(TEXT("Blueprint"));
    FString PackageName, ObjectName;
    ParseObjectPath(ObjectPath, PackageName, ObjectName);
    ObjectName = ObjectName.Mid(0, ObjectName.Len() - 2);

    UPackage* TargetPackage = CreatePackage(nullptr, *PackageName);

    UBlueprint* Blueprint = LoadObject<UBlueprint>(TargetPackage, *ObjectName);
    if (Blueprint == nullptr) {
        Blueprint = CreateBlueprint(TargetPackage, ObjectName, BlueprintJson);
        SML::Logging::info(TEXT("Creating blueprint "), *ObjectName, TEXT(" at "), *PackageName, TEXT("; Has Dependents? "), bHasDependents);
        check(Blueprint != nullptr);
        TargetPackage->MarkPackageDirty();
    }
    // Force Blueprint synchronous compilation now if somebody depends on it
    if (bHasDependents) {
        const FBPCompileRequest CompileRequest{ Blueprint, EBlueprintCompileOptions::None, nullptr };
        FBlueprintCompilationManager::CompileSynchronously(CompileRequest);
    } else {
        // Otherwise, queue blueprint for further compilation with all other blueprints
        FBlueprintCompilationManager::QueueForCompilation(Blueprint);
    }
    return TargetPackage;
}

void InitializeUserStruct(UUserDefinedStruct* StructObject, const TSharedPtr<FJsonObject>& StructJson) {
    // create all required struct fields
    const TArray<TSharedPtr<FJsonValue>>& Fields = StructJson->GetArrayField(TEXT("Fields"));
    for (const TSharedPtr<FJsonValue>& Value : Fields) {
        const FJsonObject* ValueObject = Value.Get()->AsObject().Get();
        const FString& FieldName = ValueObject->GetStringField(TEXT("Name"));
        const FEdGraphPinType& PinType = CreateGraphPinType(ValueObject->GetObjectField(TEXT("PinType")).ToSharedRef());
        AddStructVariableWithName(StructObject, PinType, FieldName);
    }
    // fire structure change event now, after all variables have been added
    // it will cause structure recompile, and also mark package dirty.
    FStructureEditorUtils::OnStructureChanged(StructObject, FStructureEditorUtils::EStructureEditorChangeInfo::AddedVariable);
}

UPackage* CreateStructFromJson(const TSharedRef<FJsonObject>& StructJson) {
    const FString& ObjectPath = StructJson->GetStringField(TEXT("StructName"));
    SML::Logging::info(TEXT("Creating user defined struct "), *ObjectPath);

    UUserDefinedStruct* StructObject = LoadObject<UUserDefinedStruct>(nullptr, *ObjectPath);
    if (StructObject == nullptr) {
        FString PackageName, ObjectName;
        ParseObjectPath(ObjectPath, PackageName, ObjectName);
        UPackage* TargetPackage = CreatePackage(nullptr, *PackageName);
        const EObjectFlags Flags = RF_Public | RF_Standalone;
        FGuid PredefinedGuid = FGuid::NewGuid();
        FGuid::Parse(StructJson->GetStringField(TEXT("Guid")), PredefinedGuid);
        // create structure with predefined FGuid value
        StructObject = CreateUserDefinedStructWithGUID(TargetPackage, FName(*ObjectName), Flags, PredefinedGuid);
        check(StructObject != nullptr);
        TargetPackage->MarkPackageDirty();
    }

    InitializeUserStruct(StructObject, StructJson);
    return Cast<UPackage>(StructObject->GetTypedOuter(UPackage::StaticClass()));
}

UPackage* CreateEnumerationFromJson(const TSharedRef<FJsonObject>& EnumJson) {
    const FString& ObjectPath = EnumJson->GetStringField(TEXT("StructName"));
    SML::Logging::info(TEXT("Creating user defined enumeration "), *ObjectPath);

    UUserDefinedEnum* EnumObject = LoadObject<UUserDefinedEnum>(nullptr, *ObjectPath);
    if (EnumObject == nullptr) {
        FString PackageName, ObjectName;
        ParseObjectPath(ObjectPath, PackageName, ObjectName);
        UPackage* TargetPackage = CreatePackage(nullptr, *PackageName);
        const EObjectFlags Flags = RF_Public | RF_Standalone;
        EnumObject = Cast<UUserDefinedEnum>(FEnumEditorUtils::CreateUserDefinedEnum(TargetPackage, FName(*ObjectName), Flags));
        check(EnumObject != nullptr);
    }

    const TArray<TSharedPtr<FJsonValue>>& Values = EnumJson->GetArrayField(TEXT("Values"));
    const int32 EnumsCurrently = EnumObject->NumEnums();
    const int32 EnumsExpected = Values.Num();
    // Ensure enums holds exactly as much values as we need it to
    if (EnumsCurrently != EnumsExpected) {
        const bool ShouldTruncate = EnumsCurrently > EnumsExpected;
        const uint32 Difference = FMath::Abs(EnumsCurrently - EnumsExpected);
        for (uint32 i = 0; i < Difference; i++) {
            if (ShouldTruncate)
                FEnumEditorUtils::RemoveEnumeratorFromUserDefinedEnum(EnumObject, 0);
            else FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum(EnumObject);
        }
    }

    // after that, enumerate entries and update their display names
    for (const TSharedPtr<FJsonValue>& Value : Values) {
        const FJsonObject* ValueObject = Value.Get()->AsObject().Get();
        if (!ValueObject->HasField(TEXT("DisplayName"))) {
            continue; // skip entries without display name, including _MAX
        }
        const int32 EnumIndex = ValueObject->GetIntegerField(TEXT("Value"));
        const FString& DisplayName = ValueObject->GetStringField(TEXT("DisplayName"));
        const FString& CurrentName = EnumObject->GetDisplayNameText(EnumIndex).ToString();
        if (CurrentName != DisplayName) {
            FEnumEditorUtils::SetEnumeratorDisplayName(EnumObject, EnumIndex, FText::AsCultureInvariant(DisplayName));
        }
    }
    return Cast<UPackage>(EnumObject->GetTypedOuter(UPackage::StaticClass()));
}

void ParseObjectPath(const FString& ObjectPath, FString& PackageName, FString& ObjectName) {
    int32 LastDotIndex;
    ObjectPath.FindLastChar('.', LastDotIndex);
    check(LastDotIndex != INDEX_NONE);
    ObjectName = ObjectPath.Mid(LastDotIndex + 1);
    PackageName = ObjectPath.Mid(0, LastDotIndex);
}

void GatherPinTypeDependencies(const TSharedRef<FJsonObject>& PinJson, TArray<FString>& Dependencies) {
    if (PinJson->HasField(TEXT("PinSubCategoryObject"))) {
        const FString& ObjectPath = PinJson->GetStringField(TEXT("PinSubCategoryObject"));
        AddDependencyIfNeeded(ObjectPath, Dependencies);
    }
    if (PinJson->HasField(TEXT("PinSubCategoryMemberReference"))) {
        const TSharedPtr<FJsonObject>& MemberJson = PinJson->GetObjectField(TEXT("PinSubCategoryMemberReference"));
        if (MemberJson->HasField("MemberParent")) {
            const FString& ObjectPath = MemberJson->GetStringField(TEXT("MemberParent"));
            AddDependencyIfNeeded(ObjectPath, Dependencies);
        }
    }
    if (PinJson->HasField(TEXT("PinValueType"))) {
        const TSharedPtr<FJsonObject> ValueJson = PinJson->GetObjectField(TEXT("PinValueType"));
        if (ValueJson->HasField(TEXT("TerminalSubCategoryObject"))) {
            const FString& ObjectPath = ValueJson->GetStringField(TEXT("TerminalSubCategoryObject"));
            AddDependencyIfNeeded(ObjectPath, Dependencies);
        }
    }
}

FEdGraphPinType CreateGraphPinType(const TSharedRef<FJsonObject>& PinJson) {
    FEdGraphPinType GraphPinType;
    GraphPinType.PinCategory = *PinJson->GetStringField(TEXT("PinCategory"));
    GraphPinType.PinSubCategory = *PinJson->GetStringField(TEXT("PinSubCategory"));
    if (PinJson->HasField(TEXT("PinSubCategoryObject"))) {
        const FString& ObjectPath = PinJson->GetStringField(TEXT("PinSubCategoryObject"));
        UObject* PinSubCategoryObject = LoadObject<UObject>(GetTransientPackage(), *ObjectPath);
        check(PinSubCategoryObject != nullptr);
        GraphPinType.PinSubCategoryObject = PinSubCategoryObject;
    }
    if (PinJson->HasField(TEXT("PinSubCategoryMemberReference"))) {
        FSimpleMemberReference& MemberRef = GraphPinType.PinSubCategoryMemberReference;
        const TSharedPtr<FJsonObject>& MemberJson = PinJson->GetObjectField(TEXT("PinSubCategoryMemberReference"));
        if (MemberJson->HasField("MemberParent")) {
            const FString& ObjectPath = MemberJson->GetStringField(TEXT("MemberParent"));
            UObject* MemberParentObject = LoadObject<UObject>(GetTransientPackage(), *ObjectPath);
            check(MemberParentObject != nullptr);
            MemberRef.MemberParent = MemberParentObject;
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
            UObject* TerminalSubCategoryObject = LoadObject<UObject>(GetTransientPackage(), *ObjectPath);
            check(TerminalSubCategoryObject != nullptr);
            ValueType.TerminalSubCategoryObject = TerminalSubCategoryObject;
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

bool IsObjectAlreadyLoaded(const FString& ObjectPath) {
    return IsValid(StaticFindObject(UObject::StaticClass(), ANY_PACKAGE, *ObjectPath));
}

void AddDependencyIfNeeded(const FString& ObjectPath, TArray<FString>& Dependencies) {
    // add to required dependencies only if we couldn't find it
    if (!IsObjectAlreadyLoaded(ObjectPath)) {
        Dependencies.Add(ObjectPath);
    }
}

void DeserializeFields(UStruct* Struct, void* Object, const TSharedPtr<FJsonObject>& ObjectJson, FDeserializationContext& Context) {
    for (TFieldIterator<UProperty> It(Struct); It; ++It) {
        UProperty* Property = *It;
        void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Object);
        const FString PropertyName = Property->GetFName().ToString();
        if (ObjectJson->HasField(PropertyName)) {
            const TSharedPtr<FJsonValue>& JsonValue = ObjectJson->Values[PropertyName];
            DeserializePropertyValue(Property, PropertyValue, JsonValue, Context);
        }
    }
}

void DeserializeStruct(void* Value, UScriptStruct* Struct, const TSharedPtr<FJsonObject>& JsonValue, FDeserializationContext& Context) {
    // We don't need anything other than fields for struct deserialization
    DeserializeFields(Struct, Value, JsonValue.ToSharedRef(), Context);
}

UObject* DeserializeUObject(const TSharedPtr<FJsonObject>& Value, FDeserializationContext& Context) {
    if (Value->HasField(TEXT("$Empty"))) {
        return nullptr; // This is NULL UObject if empty key exists
    }
    if (Value->HasField(TEXT("$AssetPath"))) {
        // This is an asset reference, we just need to load UObject from that asset path
        const FString AssetPath = Value->GetStringField(TEXT("$AssetPath"));
        return StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
    }
    if (Value->HasField(TEXT("$ObjectRef"))) {
        // We already have that object in serialization stack, obtain it by index
        const uint32 ObjectIndex = Value->GetIntegerField(TEXT("$ObjectRef"));
        check(Context.SerializationStack.Contains(ObjectIndex));
        UObject* FoundObject = Context.SerializationStack[ObjectIndex];
        check(FoundObject != nullptr);
        return FoundObject;
    }
    // Otherwise, we need to deserialize UObject
    const FString ObjectClassName = Value->GetStringField(TEXT("$ObjectClass"));
    const EObjectFlags ObjectFlags = static_cast<EObjectFlags>(Value->GetIntegerField(TEXT("$ObjectFlags")));
    const FString ObjectName = Value->GetStringField(TEXT("$ObjectName"));
    UClass* ObjectClass = LoadObject<UClass>(nullptr, *ObjectClassName);
    SML::Logging::info(TEXT("Deserializing object with type "), *ObjectClassName);
    check(ObjectClass != nullptr);
    UObject* CurrentOuter = Context.Outer;
    UObject* AlreadyExistingObject = StaticFindObjectFast(nullptr, CurrentOuter, *ObjectName, true);
    UObject* NewlyCreatedUObject;
    if (AlreadyExistingObject == nullptr) {
        // Create object if it doesn't exist yet
        NewlyCreatedUObject = NewObject<UObject>(CurrentOuter, ObjectClass, *ObjectName, ObjectFlags);
    } else {
        // Otherwise use existing one, even if type is different - we are more likely to break even more things than fix some
        // if we try to remove old objects, as it will result in nullptrs in random places
        NewlyCreatedUObject = AlreadyExistingObject;
    }

    // Update current outer for new objects, add ourselves to serialization stack if required
    Context.Outer = NewlyCreatedUObject;
    const bool bHasObjectIndex = Value->HasField(TEXT("$ObjectIndex"));
    const uint32 ObjectIndex = bHasObjectIndex ? Value->GetIntegerField(TEXT("$ObjectIndex")) : 0;
    if (bHasObjectIndex) {
        Context.SerializationStack.Add(ObjectIndex, NewlyCreatedUObject);
    }
    DeserializeFields(ObjectClass, NewlyCreatedUObject, Value.ToSharedRef(), Context);
    // Restore last outer, remove ourselves from serialization stack
    if (bHasObjectIndex) {
        Context.SerializationStack.Remove(ObjectIndex);
    }
    Context.Outer = CurrentOuter;
    return NewlyCreatedUObject;
}

void DeserializePropertyValueInternal(const UProperty* TestProperty, void* Value, FDeserializationContext& Context, TSharedPtr<FJsonValue> JsonValue);

void DeserializePropertyValue(const UProperty* Property, void* Value, const TSharedPtr<FJsonValue>& JsonValue, FDeserializationContext& Context) {
    SML::Logging::info(TEXT("Deserializing Property "), *Property->GetFName().ToString(), TEXT(" Of Class "), *Property->GetOuter()->GetName());
    const UMapProperty* MapProperty = Cast<const UMapProperty>(Property);
    const USetProperty* SetProperty = Cast<const USetProperty>(Property);
    const UArrayProperty* ArrayProperty = Cast<const UArrayProperty>(Property);
    if (MapProperty) {
        const UProperty* KeyProperty = MapProperty->KeyProp;
        const UProperty* ValueProperty = MapProperty->ValueProp;
        FScriptMapHelper MapHelper(MapProperty, Value);
        const TArray<TSharedPtr<FJsonValue>>& PairArray = JsonValue->AsArray();

        for (int32 i = 0; i < PairArray.Num(); i++) {
            const TSharedPtr<FJsonObject>& Pair = PairArray[i]->AsObject();
            const TSharedPtr<FJsonValue>& EntryKey = Pair->Values.FindChecked(TEXT("Key"));
            const TSharedPtr<FJsonValue>& EntryValue = Pair->Values.FindChecked(TEXT("Value"));
            const int32 Index = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
            uint8* PairPtr = MapHelper.GetPairPtr(Index);
            // Copy over imported key and value from temporary storage
            DeserializePropertyValue(KeyProperty, PairPtr, EntryKey, Context);
            DeserializePropertyValue(ValueProperty, PairPtr + MapHelper.MapLayout.ValueOffset, EntryValue, Context);
        }
        MapHelper.Rehash();

    } else if (SetProperty) {
        const UProperty* ElementProperty = SetProperty->ElementProp;
        FScriptSetHelper SetHelper(SetProperty, Value);
        const TArray<TSharedPtr<FJsonValue>>& SetArray = JsonValue->AsArray();
        SetHelper.EmptyElements();
        uint8* TempElementStorage = static_cast<uint8*>(FMemory::Malloc(ElementProperty->ElementSize));
        ElementProperty->InitializeValue(TempElementStorage);

        for (int32 i = 0; i < SetArray.Num(); i++) {
            const TSharedPtr<FJsonValue>& Element = SetArray[i];
            DeserializePropertyValue(ElementProperty, TempElementStorage, Element, Context);

            const int32 NewElementIndex = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
            uint8* NewElementPtr = SetHelper.GetElementPtr(NewElementIndex);

            // Copy over imported key from temporary storage
            ElementProperty->CopyCompleteValue_InContainer(NewElementPtr, TempElementStorage);
        }
        SetHelper.Rehash();

        ElementProperty->DestroyValue(TempElementStorage);
        FMemory::Free(TempElementStorage);

    } else if (ArrayProperty) {
        const UProperty* ElementProperty = ArrayProperty->Inner;
        FScriptArrayHelper ArrayHelper(ArrayProperty, Value);
        const TArray<TSharedPtr<FJsonValue>>& SetArray = JsonValue->AsArray();
        ArrayHelper.EmptyValues();

        for (int32 i = 0; i < SetArray.Num(); i++) {
            const TSharedPtr<FJsonValue>& Element = SetArray[i];
            const uint32 AddedIndex = ArrayHelper.AddValue();
            uint8* ValuePtr = ArrayHelper.GetRawPtr(AddedIndex);
            DeserializePropertyValue(ElementProperty, ValuePtr, Element, Context);
        }

    } else {
        DeserializePropertyValueInternal(Property, Value, Context, JsonValue);
    }
}

void DeserializePropertyValueInternal(const UProperty* TestProperty, void* Value, FDeserializationContext& Context, TSharedPtr<FJsonValue> JsonValue) {
    if (TestProperty->IsA<UMulticastDelegateProperty>()) {
        // Delegates should not be serialized directly
    } else if (TestProperty->IsA<UDelegateProperty>()) {
        // Delegates should not be serialized directly
    } else if (const UInterfaceProperty* InterfaceProperty = Cast<const UInterfaceProperty>(TestProperty)) {
        // UObject is enough to re-create value, since we known property on deserialization
        FScriptInterface* Interface = reinterpret_cast<FScriptInterface*>(Value);
        UObject* Object = DeserializeUObject(JsonValue->AsObject(), Context);
        if (Object != nullptr) {
            void* InterfacePtr = Object->GetInterfaceAddress(InterfaceProperty->InterfaceClass);
            check(InterfacePtr != nullptr);
            Interface->SetObject(Object);
            Interface->SetInterface(InterfacePtr);
        }

    } else if (const UClassProperty* ClassProperty = Cast<const UClassProperty>(TestProperty)) {
        // For class it's enough just to have it's path name for deserialization
        const FString PathName = JsonValue->AsString();
        if (PathName != TEXT("None")) {
            UClass* ClassObject = LoadObject<UClass>(nullptr, *PathName);
            check(ClassObject != nullptr && ClassObject->IsChildOf(ClassProperty->MetaClass));
            ClassProperty->SetObjectPropertyValue(Value, ClassObject);
        }

    } else if (TestProperty->IsA<USoftObjectProperty>()) {
        // For soft object reference, path is enough too for deserialization.
        const FString PathString = JsonValue->AsString();
        FSoftObjectPtr* ObjectPtr = reinterpret_cast<FSoftObjectPtr*>(Value);
        *ObjectPtr = FSoftObjectPath(PathString);

    } else if (const UObjectPropertyBase* ObjectProperty = Cast<const UObjectPropertyBase>(TestProperty)) {
        // Need to serialize full UObject for object property
        UObject* Object = DeserializeUObject(JsonValue->AsObject(), Context);
        ObjectProperty->SetObjectPropertyValue(Value, Object);

    } else if (const UStructProperty* StructProperty = Cast<const UStructProperty>(TestProperty)) {
        // To serialize struct, we need it's type and value pointer, because struct value doesn't contain type information
        DeserializeStruct(Value, StructProperty->Struct, JsonValue->AsObject(), Context);

        // Primitives below, they are serialized as plain json values
    } else if (const UNumericProperty* NumberProperty = Cast<const UNumericProperty>(TestProperty)) {
        const double NumberValue = JsonValue->AsNumber();
        if (NumberProperty->IsFloatingPoint())
            NumberProperty->SetFloatingPointPropertyValue(Value, NumberValue);
        else NumberProperty->SetIntPropertyValue(Value, static_cast<int64>(NumberValue));

    } else if (const UBoolProperty* BoolProperty = Cast<const UBoolProperty>(TestProperty)) {
        const bool bBooleanValue = JsonValue->AsBool();
        BoolProperty->SetPropertyValue(Value, bBooleanValue);

    } else if (TestProperty->IsA<UStrProperty>()) {
        const FString StringValue = JsonValue->AsString();
        *reinterpret_cast<FString*>(Value) = StringValue;

    } else if (const UEnumProperty* EnumProperty = Cast<const UEnumProperty>(TestProperty)) {
        // K2 only supports byte enums right now - any violations should have been caught by UHT or the editor
        if (!EnumProperty->GetUnderlyingProperty()->IsA<UByteProperty>()) {
            SML::Logging::fatal(TEXT("Unsupported Underlying Enum Property Found: "), *EnumProperty->GetUnderlyingProperty()->GetClass()->GetName());
        }
        // Prefer readable enum names in result json to raw numbers
        const FString EnumName = JsonValue->AsString();
        const int64 UnderlyingValue = EnumProperty->GetEnum()->GetValueByNameString(EnumName);
        check(UnderlyingValue != INDEX_NONE);
        *reinterpret_cast<uint8*>(Value) = static_cast<uint8>(UnderlyingValue);

    } else if (TestProperty->IsA<UNameProperty>()) {
        // Name is perfectly representable as string
        const FString NameString = JsonValue->AsString();
        *reinterpret_cast<FName*>(Value) = *NameString;

    } else if (const UTextProperty* TextProperty = Cast<const UTextProperty>(TestProperty)) {
        // For FText, standard ExportTextItem is okay to use, because it's serialization is quite complex
        const FString SerializedValue = JsonValue->AsString();
        if (!SerializedValue.IsEmpty()) {
            FTextStringHelper::ReadFromBuffer(*SerializedValue, *reinterpret_cast<FText*>(Value));
        }
    } else {
        SML::Logging::fatal(TEXT("Found unsupported property type when deserializing value: "), *TestProperty->GetClass()->GetName());
    }
}

*/
