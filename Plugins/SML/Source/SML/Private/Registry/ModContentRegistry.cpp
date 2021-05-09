#include "Registry/ModContentRegistry.h"

#include "FGGameInstance.h"
#include "FGGameState.h"
#include "FGRecipeManager.h"
#include "FGResearchManager.h"
#include "FGResourceSinkSettings.h"
#include "FGResourceSinkSubsystem.h"
#include "FGSchematicManager.h"
#include "FGTutorialIntroManager.h"
#include "Unlocks/FGUnlockRecipe.h"
#include "IPlatformFilePak.h"
#include "Patching/NativeHookManager.h"
#include "Reflection/ReflectionHelper.h"
#include "Engine/AssetManager.h"
#include "ModLoading/ModLoadingLibrary.h"
#include "Subsystem/SubsystemActorManager.h"
#include "Util/BlueprintAssetHelperLibrary.h"

DEFINE_LOG_CATEGORY(LogContentRegistry);

static bool GIsRegisteringVanillaContent = false;

/** Makes sure provided object instance is valid, crashes with both script call stack and native stack trace if it's not */
#define CHECK_PROVIDED_OBJECT_VALID(Object, Message, ...) \
    if (!IsValid(Object)) { \
        const FString Context = FString::Printf(Message, __VA_ARGS__); \
        /* Attempt to use cached script frame pointer first, then fallback to global script callstack (which is not available in shipping by default) */ \
        const FString ScriptCallstack = ActiveScriptFramePtr ? ActiveScriptFramePtr->GetStackTrace() : FFrame::GetScriptCallstack(); \
        UE_LOG(LogContentRegistry, Error, TEXT("Attempt to register invalid content: %s"), *Context); \
        UE_LOG(LogContentRegistry, Error, TEXT("Script Callstack: %s"), *ScriptCallstack); \
        UE_LOG(LogContentRegistry, Fatal, TEXT("Attempt to register invalid content in mod content registry: %s. Script callstack: %s"), *Context, *ScriptCallstack); \
    }

void ExtractRecipesFromSchematic(TSubclassOf<UFGSchematic> Schematic, TArray<TSubclassOf<UFGRecipe>>& OutRecipes) {
    const TArray<UFGUnlock*> Unlocks = UFGSchematic::GetUnlocks(Schematic);
    for (UFGUnlock* Unlock : Unlocks) {
        if (UFGUnlockRecipe* UnlockRecipe = Cast<UFGUnlockRecipe>(Unlock)) {
            OutRecipes.Append(UnlockRecipe->GetRecipesToUnlock());
        }
    }
}

void ExtractSchematicsFromResearchTree(TSubclassOf<UFGResearchTree> ResearchTree, TArray<TSubclassOf<UFGSchematic>>& OutSchematics) {

    static FStructProperty* NodeDataStructProperty = NULL;
    static FClassProperty* SchematicStructProperty = NULL;
    static UClass* ResearchTreeNodeClass = NULL;

    // Lazily initialize research tree node reflection properties for faster access
    if (ResearchTreeNodeClass == NULL) {
        ResearchTreeNodeClass = LoadClass<UFGResearchTreeNode>(NULL, TEXT("/Game/FactoryGame/Schematics/Research/BPD_ResearchTreeNode.BPD_ResearchTreeNode_C"));
        check(ResearchTreeNodeClass);
        // Make sure class is not garbage collected
        ResearchTreeNodeClass->AddToRoot();

        NodeDataStructProperty = FReflectionHelper::FindPropertyChecked<FStructProperty>(ResearchTreeNodeClass, TEXT("mNodeDataStruct"));
        SchematicStructProperty = FReflectionHelper::FindPropertyByShortNameChecked<FClassProperty>(NodeDataStructProperty->Struct, TEXT("Schematic"));

        check(SchematicStructProperty->MetaClass->IsChildOf(UFGSchematic::StaticClass()));
    }

    const TArray<UFGResearchTreeNode*> Nodes = UFGResearchTree::GetNodes(ResearchTree);
    for (UFGResearchTreeNode* Node : Nodes) {
        if (!Node->IsA(ResearchTreeNodeClass)) {
            UE_LOG(
                LogContentRegistry,
                Warning,
                TEXT("Unsupported node class %s for research tree %s"),
                *Node->GetClass()->GetPathName(),
                *ResearchTree->GetPathName()
            );
            continue;
        }
        const void* NodeDataStructPtr = NodeDataStructProperty->ContainerPtrToValuePtr<void>(Node);
        UClass* SchematicClass = Cast<UClass>(SchematicStructProperty->GetPropertyValue_InContainer(NodeDataStructPtr));
        if (SchematicClass == NULL) {
            UE_LOG(
                LogContentRegistry,
                Warning,
                TEXT("Schematic not set on research tree %s, node %s"),
                *ResearchTree->GetPathName(),
                *Node->GetClass()->GetPathName()
            );
            continue;
        }
        OutSchematics.Add(SchematicClass);
    }
}

template<typename T>
TArray<TSubclassOf<T>> DiscoverVanillaContentOfType() {
    UClass* PrimaryAssetClass = T::StaticClass();
    UAssetManager& AssetManager = UAssetManager::Get();

    const FPrimaryAssetType AssetType = PrimaryAssetClass->GetFName();
    TArray<FAssetData> FoundVanillaAssets;
    AssetManager.GetPrimaryAssetDataList(AssetType, FoundVanillaAssets);
    TArray<TSubclassOf<T>> OutVanillaContent;

    for (const FAssetData& AssetData : FoundVanillaAssets) {
        FAssetDataTagMapSharedView::FFindTagResult GeneratedClassTextPath = AssetData.TagsAndValues.FindTag(FBlueprintTags::GeneratedClassPath);
        if (GeneratedClassTextPath.IsSet()) {
            const FString BlueprintClassPath = FPackageName::ExportTextPathToObjectPath(GeneratedClassTextPath.GetValue());
            UClass* LoadedClass = LoadClass<T>(NULL, *BlueprintClassPath);
            if (LoadedClass != NULL) {
                OutVanillaContent.Add(LoadedClass);
            }
        }
    }
    UE_LOG(LogContentRegistry, Display, TEXT("Discovered %d vanilla assets of type %s"), OutVanillaContent.Num(), *PrimaryAssetClass->GetName());
    return OutVanillaContent;
}

void AModContentRegistry::DisableVanillaContentRegistration() {
    // Prevent unnecessary vanilla schematic list population -
    // we are overriding it from content registry anyway, so let's save some processing time
    SUBSCRIBE_METHOD(
        AFGSchematicManager::PopulateSchematicsLists,
        [](auto& Call, AFGSchematicManager*) { Call.Cancel(); }
    );
    SUBSCRIBE_METHOD(
        AFGSchematicManager::PopulateAvailableSchematicsList,
        [](auto& Call, AFGSchematicManager*) { Call.Cancel(); }
    );
    SUBSCRIBE_METHOD(
        AFGResearchManager::PopulateResearchTreeList,
        [](auto& Call, AFGResearchManager*) { Call.Cancel(); }
    );
}

FName AModContentRegistry::FindContentOwnerFast(UClass* ContentClass) {
    checkf(ContentClass, TEXT("NULL ContentClass passed to FindContentOwnerFast"));

    // Shortcut used for quickly registering vanilla content
    if (GIsRegisteringVanillaContent) {
        return FACTORYGAME_MOD_NAME;
    }

    // Use GetName on package instead of GetPathName() because it's faster and avoids string concat
    const FString ContentOwnerName = UBlueprintAssetHelperLibrary::FindPluginNameByObjectPath(ContentClass->GetOuterUPackage()->GetName());
    if (ContentOwnerName.IsEmpty()) {
        UE_LOG(LogContentRegistry, Error, TEXT("Failed to determine content owner for object %s. This is an error, report to mod author!"), *ContentClass->GetPathName());
        return FACTORYGAME_MOD_NAME;
    }
    return *ContentOwnerName;
}

void AModContentRegistry::NotifyModuleRegistrationFinished() {
    UE_LOG(LogContentRegistry, Log, TEXT("Module content registration finished notify received"));
    FreezeRegistryState();
}

void AModContentRegistry::FlushStateToSchematicManager(AFGSchematicManager* SchematicManager) const {
    const TArray<TSharedPtr<FSchematicRegistrationInfo>> RegisteredSchematics = SchematicRegistryState.GetAllObjects();

    // Empty lists while maintaining enough capacity to re-populate it later
    SchematicManager->mAllSchematics.Empty(RegisteredSchematics.Num());
    SchematicManager->mAvailableSchematics.Empty(RegisteredSchematics.Num());

    for (const TSharedPtr<FSchematicRegistrationInfo>& RegistrationInfo : RegisteredSchematics) {
        TSubclassOf<UFGSchematic> Schematic = RegistrationInfo->RegisteredObject;
        SchematicManager->mAllSchematics.Add(Schematic);

        if ((UFGSchematic::GetType(Schematic) == ESchematicType::EST_Milestone ||
                UFGSchematic::GetType(Schematic) == ESchematicType::EST_Tutorial ||
                UFGSchematic::GetType(Schematic) == ESchematicType::EST_ResourceSink) &&
            SchematicManager->CanGiveAccessToSchematic(Schematic)) {
            SchematicManager->mAvailableSchematics.Add(Schematic);
        }
    }
}

void AModContentRegistry::FlushStateToResearchManager(AFGResearchManager* ResearchManager) const {
    const TArray<TSharedPtr<FResearchTreeRegistrationInfo>> RegisteredResearchTrees = ResearchTreeRegistryState.GetAllObjects();

    // Empty lists while maintaining enough capacity to re-populate it later
    ResearchManager->mAvailableResearchTrees.Empty(RegisteredResearchTrees.Num());

    for (const TSharedPtr<FResearchTreeRegistrationInfo>& RegistrationInfo : RegisteredResearchTrees) {
        TSubclassOf<UFGResearchTree> ResearchTree = RegistrationInfo->RegisteredObject;
        ResearchManager->mAvailableResearchTrees.Add(ResearchTree);
    }
    // Update unlocked research trees
    ResearchManager->UpdateUnlockedResearchTrees();
}

void AModContentRegistry::SubscribeToSchematicManager(AFGSchematicManager* SchematicManager) {
    FScriptDelegate ScriptDelegate;
    ScriptDelegate.BindUFunction(this, GET_FUNCTION_NAME_STRING_CHECKED(AModContentRegistry, OnSchematicPurchased));
    SchematicManager->PurchasedSchematicDelegate.AddUnique(ScriptDelegate);
}

void AModContentRegistry::Init() {
    // Register vanilla content in the registry
    const FName FactoryGame = FACTORYGAME_MOD_NAME;

    UE_LOG(LogContentRegistry, Display, TEXT("Initializing mod content registry"));
    const TArray<TSubclassOf<UFGSchematic>> AllSchematics = DiscoverVanillaContentOfType<UFGSchematic>();
    const TArray<TSubclassOf<UFGResearchTree>> AllResearchTrees = DiscoverVanillaContentOfType<UFGResearchTree>();

    // Start registering vanilla content now
    GIsRegisteringVanillaContent = true;

    for (const TSubclassOf<UFGSchematic>& Schematic : AllSchematics) {
        RegisterSchematic(FactoryGame, Schematic);
    }

    for (const TSubclassOf<UFGResearchTree>& ResearchTree : AllResearchTrees) {
        RegisterResearchTree(FactoryGame, ResearchTree);
    }

    // Stop registering vanilla content at this point
    GIsRegisteringVanillaContent = false;
}

void AModContentRegistry::FreezeRegistryState() {
    checkf(!bIsRegistryFrozen, TEXT("Attempt to re-freeze already frozen registry"));

    UE_LOG(LogContentRegistry, Display, TEXT("Freezing content registry"));
    this->bIsRegistryFrozen = true;
}

void AModContentRegistry::EnsureRegistryUnfrozen() const {
    if (bIsRegistryFrozen) {
        UE_LOG(
            LogContentRegistry,
            Fatal,
            TEXT("Attempt to register object in frozen mod content registry. "
                "Make sure your modded content registration is happening in the 'Initialization' Lifecycle Phase and not 'Post Initialization'")
        );
    }
}

void AModContentRegistry::CheckSavedDataForMissingObjects() {
    checkf(bIsRegistryFrozen, TEXT("CheckSavedDataForMissingObjects called before registry is frozen"));

    AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(this);
    AFGSchematicManager* SchematicManager = AFGSchematicManager::Get(this);
    AFGResearchManager* ResearchManager = AFGResearchManager::Get(this);

    TArray<FMissingObjectStruct> MissingObjects;
    if (ResearchManager != NULL) {
        FindMissingResearchTrees(ResearchManager, MissingObjects);
    }
    if (SchematicManager != NULL) {
        FindMissingSchematics(SchematicManager, MissingObjects);
    }
    if (RecipeManager != NULL) {
        FindMissingRecipes(RecipeManager, MissingObjects);
    }

    if (MissingObjects.Num() > 0) {
        WarnAboutMissingObjects(MissingObjects);
    }
}

void AModContentRegistry::UnlockTutorialSchematics() {
    UFGGameInstance* GameInstance = Cast<UFGGameInstance>(GetGameInstance());
    AFGTutorialIntroManager* TutorialIntroManager = AFGTutorialIntroManager::Get(this);
    AFGSchematicManager* SchematicManager = AFGSchematicManager::Get(this);

    if (SchematicManager && (GameInstance && GameInstance->GetSkipOnboarding() || TutorialIntroManager && TutorialIntroManager->GetIsTutorialCompleted())) {
        for (const TSharedPtr<FSchematicRegistrationInfo>& RegistrationInfo : SchematicRegistryState.GetAllObjects()) {
            const TSubclassOf<UFGSchematic> Schematic = RegistrationInfo->RegisteredObject;

            // TODO using GiveAccessToSchematic has some side effects, for example unwanted ADA messages
            // We should probably look into modifying list directly, but that would essentially mean conflicts with AFGUnlockSystem
            // and other kinds of nasty stuff, so for now i'm leaving as it is while we're looking for a better solution
            if (UFGSchematic::GetType(Schematic) == ESchematicType::EST_Tutorial &&
                SchematicManager->mPurchasedSchematics.Find(Schematic) == INDEX_NONE) {
                SchematicManager->GiveAccessToSchematic(Schematic);
            }
        }
    }
}

void AModContentRegistry::OnSchematicPurchased(TSubclassOf<UFGSchematic> Schematic) {
    // Update research trees in case they depend on schematic unlocked dependency
    AFGResearchManager* ResearchManager = AFGResearchManager::Get(this);
    if (ResearchManager != NULL && ResearchManager->HasAuthority()) {
        ResearchManager->UpdateUnlockedResearchTrees();
    }
}

void AModContentRegistry::MarkItemDescriptorsFromRecipe(const TSubclassOf<UFGRecipe>& Recipe, const FName ModReference) {
    TArray<FItemAmount> AllReferencedItems;
    AllReferencedItems.Append(UFGRecipe::GetIngredients(Recipe));
    AllReferencedItems.Append(UFGRecipe::GetProducts(Recipe));

    for (const FItemAmount& ItemAmount : AllReferencedItems) {
        const TSubclassOf<UFGItemDescriptor>& ItemDescriptor = ItemAmount.ItemClass;

        CHECK_PROVIDED_OBJECT_VALID(
            ItemDescriptor,
            TEXT("Recipe '%s' registered by %s contains invalid NULL ItemDescriptor in it's Ingredients or Results"),
            *Recipe->GetPathName(),
            *ModReference.ToString()
        );

        TSharedPtr<FItemRegistrationInfo> ItemRegistrationInfo = ItemRegistryState.FindObject(ItemDescriptor);
        if (!ItemRegistrationInfo.IsValid()) {
            const FName OwnerModReference = FindContentOwnerFast(ItemDescriptor);
            ItemRegistrationInfo = RegisterItemDescriptor(OwnerModReference, ModReference, ItemDescriptor);
        }
        // Associate item registration info with this recipe
        ItemRegistrationInfo->ReferencedBy.AddUnique(Recipe);
    }
}

TSharedPtr<FItemRegistrationInfo> AModContentRegistry::RegisterItemDescriptor(const FName OwnerModReference, const FName RegistrarModReference, const TSubclassOf<UFGItemDescriptor>& ItemDescriptor) {
    checkf(ItemDescriptor, TEXT("Attempt to register NULL ItemDescriptor, mod reference: %s"), *RegistrarModReference.ToString());
    return ItemRegistryState.RegisterObject(MakeRegistrationInfo<FItemRegistrationInfo>(ItemDescriptor, OwnerModReference, RegistrarModReference));
}

void AModContentRegistry::FindMissingSchematics
(
    AFGSchematicManager* SchematicManager,
    TArray<FMissingObjectStruct>& MissingObjects
) const {
    // Clear references to unlocked schematics if they are not registered
    TArray<TSubclassOf<UFGSchematic>> PurchasedSchematics = SchematicManager->mPurchasedSchematics;
    for (const TSubclassOf<UFGSchematic> Schematic : PurchasedSchematics) {
        if (!IsSchematicRegistered(Schematic)) {
            MissingObjects.Add(FMissingObjectStruct{TEXT("schematic"), Schematic->GetPathName()});
            SchematicManager->mPurchasedSchematics.Remove(Schematic);
        }
    }
    // Do same thing for incomplete schematic progress
    SchematicManager->mPaidOffSchematic.RemoveAll(
        [&](const FSchematicCost& SchematicCost) {
            return !IsSchematicRegistered(SchematicCost.Schematic);
        }
    );
}

void AModContentRegistry::FindMissingResearchTrees
(
    AFGResearchManager* ResearchManager,
    TArray<FMissingObjectStruct>& MissingObjects
) const {
    // Clear unlocked research trees
    TArray<TSubclassOf<UFGResearchTree>> UnlockedResearchTrees = ResearchManager->mUnlockedResearchTrees;
    for (const TSubclassOf<UFGResearchTree>& ResearchTree : UnlockedResearchTrees) {
        if (!IsResearchTreeRegistered(ResearchTree)) {
            ResearchManager->mUnlockedResearchTrees.Remove(ResearchTree);
            MissingObjects.Add(FMissingObjectStruct{TEXT("research_tree"), ResearchTree->GetPathName()});
        }
    }

    // Clear completed, but unclaimed researches
    ResearchManager->mCompletedResearch.RemoveAll(
        [&](const FResearchData& ResearchData) {
            return !IsResearchTreeRegistered(ResearchData.InitiatingResearchTree) ||
                !IsSchematicRegistered(ResearchData.Schematic);
        }
    );

    // Clear ongoing research data
    ResearchManager->mOngoingResearch.RemoveAll(
        [&](const FResearchTime& ResearchTime) {
            return !IsResearchTreeRegistered(ResearchTime.ResearchData.InitiatingResearchTree) ||
                !IsSchematicRegistered(ResearchTime.ResearchData.Schematic);
        }
    );
}

void AModContentRegistry::FindMissingRecipes
(
    AFGRecipeManager* RecipeManager,
    TArray<FMissingObjectStruct>& MissingObjects
) const {
    // Clear unlocked recipes
    TArray<TSubclassOf<UFGRecipe>> UnlockedRecipes = RecipeManager->mAvailableRecipes;
    for (const TSubclassOf<UFGRecipe>& Recipe : UnlockedRecipes) {
        if (!IsRecipeRegistered(Recipe)) {
            RecipeManager->mAvailableRecipes.Remove(Recipe);
            MissingObjects.Add(FMissingObjectStruct{TEXT("recipe"), Recipe->GetPathName()});
        }
    }
}

void AModContentRegistry::WarnAboutMissingObjects(const TArray<FMissingObjectStruct>& MissingObjects) {
    UE_LOG(LogContentRegistry, Error, TEXT("---------------------------------------------"));
    UE_LOG(LogContentRegistry, Error, TEXT("Found unregistered objects referenced in savegame:"));
    for (const FMissingObjectStruct& ObjectStruct : MissingObjects) {
        UE_LOG(LogContentRegistry, Error, TEXT("%s: %s"), *ObjectStruct.ObjectType, *ObjectStruct.ObjectPath);
    }
    UE_LOG(LogContentRegistry, Error, TEXT("They will be cleared out"));
    UE_LOG(LogContentRegistry, Error, TEXT("---------------------------------------------"));
}

void AModContentRegistry::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector) {
    AModContentRegistry* ModContentRegistry = Cast<AModContentRegistry>(InThis);
    // Register all non-UPROPERTY referenced objects from registry states (registry states cannot be UPROPERTY() because they are templates!)
    ModContentRegistry->ItemRegistryState.AddReferencedObjects(InThis, Collector);
    ModContentRegistry->RecipeRegistryState.AddReferencedObjects(InThis, Collector);
    ModContentRegistry->SchematicRegistryState.AddReferencedObjects(InThis, Collector);
    ModContentRegistry->ResearchTreeRegistryState.AddReferencedObjects(InThis, Collector);
}

void AModContentRegistry::RegisterSchematic(const FName ModReference, const TSubclassOf<UFGSchematic> Schematic) {
    CHECK_PROVIDED_OBJECT_VALID(Schematic, TEXT("Attempt to register NULL Schematic. Mod Reference: %s"), *ModReference.ToString());

    if (!SchematicRegistryState.ContainsObject(Schematic)) {
        EnsureRegistryUnfrozen();

        // Create registration entry and register
        const FName OwnerModReference = FindContentOwnerFast(Schematic);
        const TSharedPtr<FSchematicRegistrationInfo> RegistrationInfo = SchematicRegistryState.RegisterObject(
            MakeRegistrationInfo<FSchematicRegistrationInfo>(Schematic, OwnerModReference, ModReference)
        );

        // Register referenced recipes automatically and associate schematic with them
        TArray<TSubclassOf<UFGRecipe>> OutReferencedRecipes;
        ExtractRecipesFromSchematic(Schematic, OutReferencedRecipes);

        for (const TSubclassOf<UFGRecipe>& Recipe : OutReferencedRecipes) {
            CHECK_PROVIDED_OBJECT_VALID(
                Recipe,
                TEXT("Schematic '%s' registered by %s references invalid NULL Recipe in it's Unlocks Array"),
                *Schematic->GetPathName(),
                *ModReference.ToString()
            );

            RegisterRecipe(ModReference, Recipe);
            const TSharedPtr<FRecipeRegistrationInfo> RecipeRegistrationInfo = RecipeRegistryState.FindObject(Recipe);
            RecipeRegistrationInfo->ReferencedBy.Add(Schematic);
        }

        // Process registration callback delegate
        OnSchematicRegistered.Broadcast(Schematic, *RegistrationInfo);
    }
}

void AModContentRegistry::RegisterResearchTree(const FName ModReference, const TSubclassOf<UFGResearchTree> ResearchTree) {
    CHECK_PROVIDED_OBJECT_VALID(ResearchTree, TEXT("Attempt to register NULL ResearchTree. Mod Reference: %s"), *ModReference.ToString());

    if (!ResearchTreeRegistryState.ContainsObject(ResearchTree)) {
        EnsureRegistryUnfrozen();

        // Create registration entry and register
        const FName OwnerModReference = FindContentOwnerFast(ResearchTree);
        const TSharedPtr<FResearchTreeRegistrationInfo> RegistrationInfo = ResearchTreeRegistryState.RegisterObject(
            MakeRegistrationInfo<FResearchTreeRegistrationInfo>(ResearchTree, OwnerModReference, ModReference)
        );

        // Register referenced schematics automatically and associate research tree with them
        TArray<TSubclassOf<UFGSchematic>> OutReferencedSchematics;
        ExtractSchematicsFromResearchTree(ResearchTree, OutReferencedSchematics);

        for (const TSubclassOf<UFGSchematic>& Schematic : OutReferencedSchematics) {
            CHECK_PROVIDED_OBJECT_VALID(
                Schematic,
                TEXT("ResearchTree '%s' registered by %s references invalid NULL Schematic in one of it's Nodes"),
                *ResearchTree->GetPathName(),
                *ModReference.ToString()
            );

            RegisterSchematic(ModReference, Schematic);
            const TSharedPtr<FSchematicRegistrationInfo> SchematicRegistrationInfo = SchematicRegistryState.FindObject(Schematic);
            SchematicRegistrationInfo->ReferencedBy.Add(ResearchTree);
        }

        // Process registration callback
        OnResearchTreeRegistered.Broadcast(ResearchTree, *RegistrationInfo);
    }
}

void AModContentRegistry::RegisterRecipe(const FName ModReference, const TSubclassOf<UFGRecipe> Recipe) {
    CHECK_PROVIDED_OBJECT_VALID(Recipe, TEXT("Attempt to register NULL Recipe. Mod Reference: %s"), *ModReference.ToString());

    if (!RecipeRegistryState.ContainsObject(Recipe)) {
        EnsureRegistryUnfrozen();

        // Create registration entry and register
        const FName OwnerModReference = FindContentOwnerFast(Recipe);
        const TSharedPtr<FRecipeRegistrationInfo> RegistrationInfo = RecipeRegistryState.RegisterObject(
            MakeRegistrationInfo<FRecipeRegistrationInfo>(Recipe, OwnerModReference, ModReference)
        );

        // Process registration callback
        OnRecipeRegistered.Broadcast(Recipe, *RegistrationInfo);

        // Associate referenced item descriptors with this recipe registrar
        MarkItemDescriptorsFromRecipe(Recipe, ModReference);
    }
}

void AModContentRegistry::RegisterResourceSinkItemPointTable(FName ModReference, UDataTable* PointTable) {
    CHECK_PROVIDED_OBJECT_VALID(PointTable, TEXT("Attempt to register NULL ResourceSinkPointTable. Mod Reference: %s"), *ModReference.ToString());

    checkf(
        PointTable->RowStruct != nullptr && PointTable->RowStruct->IsChildOf(FResourceSinkPointsData::StaticStruct()),
        TEXT("Invalid AWESOME Sink item points table in mod %s (%s): Row Type should be Resource Sink Points Data"),
        *ModReference.ToString(),
        *PointTable->GetPathName()
    );

    this->PendingItemSinkPointsRegistrations.Add(PointTable, ModReference);
    FlushPendingResourceSinkRegistrations();
}

TArray<FItemRegistrationInfo> AModContentRegistry::GetLoadedItemDescriptors() {
    // Since we don't have consistent registry, we have to iterate all loaded classes and generate information from them
    // We also keep all referenced classes loaded, so they will be included there too
    TArray<FItemRegistrationInfo> OutRegistrationInfo;

    UClass* ItemDescriptorClass = UFGItemDescriptor::StaticClass();
    ForEachObjectOfClass(
        UClass::StaticClass(),
        [&](UObject* LoadedClassObject) {
            UClass* Class = Cast<UClass>(LoadedClassObject);
            if (Class->IsChildOf(ItemDescriptorClass)) {
                const TSubclassOf<UFGItemDescriptor> ItemDescriptor = Class;
                OutRegistrationInfo.Add(GetItemDescriptorInfo(ItemDescriptor));
            }
        }
    );
    return OutRegistrationInfo;
}

TArray<FItemRegistrationInfo> AModContentRegistry::GetObtainableItemDescriptors() const {
    // All obtainable item descriptors are guaranteed to be present in ItemDescriptorRegistrationList,
    // So we can just iterate it and filter item descriptors without associated recipes out
    TArray<FItemRegistrationInfo> OutRegistrationInfo;

    for (const TSharedPtr<FItemRegistrationInfo>& RegistrationInfo : ItemRegistryState.GetAllObjects()) {
        if (RegistrationInfo->ReferencedBy.Num() > 0) {
            OutRegistrationInfo.Add(*RegistrationInfo);
        }
    }
    return OutRegistrationInfo;
}

FItemRegistrationInfo AModContentRegistry::GetItemDescriptorInfo(const TSubclassOf<UFGItemDescriptor> ItemDescriptor) {
    // Remove blank registration info if provided item descriptor is not valid
    if (!IsValid(ItemDescriptor)) {
        return FItemRegistrationInfo{};
    }

    // Use cached registration information if it's available
    const TSharedPtr<FItemRegistrationInfo> CachedRegistrationInfo = ItemRegistryState.FindObject(ItemDescriptor);
    if (CachedRegistrationInfo.IsValid()) {
        return *CachedRegistrationInfo;
    }

    // Item descriptor was not referenced in any registered recipe, fallback to registrar name = owner name logic
    const FName OwnerModReference = FindContentOwnerFast(ItemDescriptor);
    return *RegisterItemDescriptor(OwnerModReference, OwnerModReference, ItemDescriptor);
}

AModContentRegistry::AModContentRegistry() {
    bIsRegistryFrozen = false;
    SchematicManagerInternalState = -1;
    ResearchManagerInternalState = -1;
    bSubscribedToSchematicManager = false;
    PrimaryActorTick.bCanEverTick = true;
    ActiveScriptFramePtr = NULL;

    // Mod Content Registry is always Local and Spawned on both Client and Server separately
    this->ReplicationPolicy = ESubsystemReplicationPolicy::SpawnLocal;
}

AModContentRegistry* AModContentRegistry::Get(UObject* WorldContext) {
    UWorld* WorldObject = GEngine->GetWorldFromContextObjectChecked(WorldContext);
    USubsystemActorManager* SubsystemActorManager = WorldObject->GetSubsystem<USubsystemActorManager>();
    check(SubsystemActorManager);

    return SubsystemActorManager->GetSubsystemActor<AModContentRegistry>();
}

void AModContentRegistry::BeginPlay() {
    Super::BeginPlay();

    // We should be frozen at this point already on host clients (freezing there happens before BeginPlay is dispatched to world actors)
    // For remote clients we are not frozen yet most likely, but checking save data for remote clients is pointless anyway
    if (HasAuthority()) {
        CheckSavedDataForMissingObjects();
    }

    // Give instant access to tutorial schematics if we have finished tutorial or have been asked to skip onboarding
    // We have to do it here since SchematicManager does it in BeginPlay by iterating mAllSchematics, and since we overwrite vanilla
    // function so all schematic is empty, it ends up not unlocking any tutorial schematics
    if (HasAuthority()) {
        UnlockTutorialSchematics();
    }
}

void AModContentRegistry::Tick(float DeltaSeconds) {
    // Make sure client subsystems are fully replicated
    AFGGameState* GameState = Cast<AFGGameState>(GetWorld()->GetGameState());
    if (GameState == NULL || !GameState->AreClientSubsystemsValid()) return;

    // Make sure vanilla states are up to date with registry
    AFGSchematicManager* SchematicManager = AFGSchematicManager::Get(this);
    AFGResearchManager* ResearchManager = AFGResearchManager::Get(this);

    if (SchematicManager != NULL) {
        const int64 SchematicRegistryCounter = SchematicRegistryState.GetRegistrationCounter();

        if (SchematicRegistryCounter > SchematicManagerInternalState) {
            FlushStateToSchematicManager(SchematicManager);
            SchematicManagerInternalState = SchematicRegistryCounter;
        }

        if (!bSubscribedToSchematicManager) {
            SubscribeToSchematicManager(SchematicManager);
            bSubscribedToSchematicManager = true;
        }
    }

    if (ResearchManager != NULL) {
        const int64 ResearchTreeRegistryCounter = ResearchTreeRegistryState.GetRegistrationCounter();

        if (ResearchTreeRegistryCounter > ResearchManagerInternalState) {
            FlushStateToResearchManager(ResearchManager);
            ResearchManagerInternalState = ResearchTreeRegistryCounter;
        }
    }

    if (PendingItemSinkPointsRegistrations.Num()) {
        FlushPendingResourceSinkRegistrations();
    }
}

void AModContentRegistry::FlushPendingResourceSinkRegistrations() {
    AFGResourceSinkSubsystem* ResourceSinkSubsystem = AFGResourceSinkSubsystem::Get(this);

    if (ResourceSinkSubsystem != NULL) {
        for (const TPair<UDataTable*, FName>& Pair : PendingItemSinkPointsRegistrations) {
            UE_LOG(LogContentRegistry, Log, TEXT("Registering Resource Sink Points Table '%s' from Mod %s"), *Pair.Key->GetPathName(), *Pair.Value.ToString());;

            TArray<FResourceSinkPointsData*> OutModPointsData;
            Pair.Key->GetAllRows(TEXT("ResourceSinkPointsData"), OutModPointsData);
            for (FResourceSinkPointsData* ModItemRow : OutModPointsData) {
                int32 Points = FMath::Max(ModItemRow->Points, ModItemRow->OverriddenResourceSinkPoints);
                ResourceSinkSubsystem->mResourceSinkPoints.Add(ModItemRow->ItemClass, Points);
            }
        }

        PendingItemSinkPointsRegistrations.Empty();
    }
}

#define P_SET_ACTIVE_FRAME P_THIS->ActiveScriptFramePtr = &Stack;
#define P_RESET_ACTIVE_FRAME P_THIS->ActiveScriptFramePtr = NULL;

DEFINE_FUNCTION(AModContentRegistry::execRegisterResourceSinkItemPointTable) {
    P_GET_PROPERTY(FNameProperty, ModReference);
    P_GET_OBJECT(UDataTable, PointTable);
    P_FINISH;
    P_NATIVE_BEGIN;
        P_SET_ACTIVE_FRAME;
        P_THIS->RegisterResourceSinkItemPointTable(ModReference, PointTable);
        P_RESET_ACTIVE_FRAME;
    P_NATIVE_END;
}

DEFINE_FUNCTION(AModContentRegistry::execRegisterRecipe) {
    P_GET_PROPERTY(FNameProperty, Z_Param_ModReference);
    P_GET_OBJECT(UClass, Z_Param_Recipe);
    P_FINISH;
    P_NATIVE_BEGIN;
        P_SET_ACTIVE_FRAME;
        P_THIS->RegisterRecipe(Z_Param_ModReference, Z_Param_Recipe);
        P_RESET_ACTIVE_FRAME;
    P_NATIVE_END;
}

DEFINE_FUNCTION(AModContentRegistry::execRegisterResearchTree) {
    P_GET_PROPERTY(FNameProperty, ModReference);
    P_GET_OBJECT(UClass, ResearchTree);
    P_FINISH;
    P_NATIVE_BEGIN;
        P_SET_ACTIVE_FRAME;
        P_THIS->RegisterResearchTree(ModReference, ResearchTree);
        P_RESET_ACTIVE_FRAME;
    P_NATIVE_END;
}

DEFINE_FUNCTION(AModContentRegistry::execRegisterSchematic) {
    P_GET_PROPERTY(FNameProperty, ModReference);
    P_GET_OBJECT(UClass, Schematic);
    P_FINISH;
    P_NATIVE_BEGIN;
        P_SET_ACTIVE_FRAME;
        P_THIS->RegisterSchematic(ModReference, Schematic);
        P_RESET_ACTIVE_FRAME;
    P_NATIVE_END;
}
