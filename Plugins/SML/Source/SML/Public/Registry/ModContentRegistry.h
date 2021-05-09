#pragma once
#include "FGResearchTree.h"
#include "FGSchematic.h"
#include "FGRecipe.h"
#include "Engine/DataTable.h"
#include "Subsystem/ModSubsystem.h"
#include "ModContentRegistry.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogContentRegistry, Log, All);

/** Holds basic information about a single content registration entry */
// Should be very lightweight for network serialization, that's why we use FName and not FString here
USTRUCT(BlueprintType)
struct SML_API FBasicRegistrationInfo {
    GENERATED_BODY()
public:
    /** Mod reference of the plugin which actually performed the object registration. Usually same as OwnedByModReference */
    UPROPERTY(BlueprintReadOnly)
    FName RegistrarModReference;
    /** Mod reference of the plugin which owns the actual registered object */
    UPROPERTY(BlueprintReadOnly)
    FName OwnedByModReference;
    /** Object that was registered */
    UPROPERTY(BlueprintReadOnly)
    UClass* RegisteredObject;
};

/** Holds information about item registration entry */
USTRUCT(BlueprintType)
struct SML_API FItemRegistrationInfo : public FBasicRegistrationInfo {
    GENERATED_BODY()
public:
    /**
     * List of all recipes that reference this item descriptor
     */
    UPROPERTY(BlueprintReadOnly)
    TArray<TSubclassOf<UFGRecipe>> ReferencedBy;
};

/** Holds information about recipe registration entry */
USTRUCT(BlueprintType)
struct SML_API FRecipeRegistrationInfo : public FBasicRegistrationInfo {
    GENERATED_BODY()
public:
    /**
     * List of all schematics that reference this recipe
     */
    UPROPERTY(BlueprintReadOnly)
    TArray<TSubclassOf<UFGSchematic>> ReferencedBy;
};

/** Holds information about schematic registration entry */
USTRUCT(BlueprintType)
struct SML_API FSchematicRegistrationInfo : public FBasicRegistrationInfo {
    GENERATED_BODY()
public:
    /**
     * List of all research trees that reference this schematic
     */
    UPROPERTY(BlueprintReadOnly)
    TArray<TSubclassOf<UFGResearchTree>> ReferencedBy;
};

/** Holds information about research tree registration entry */
USTRUCT(BlueprintType)
struct SML_API FResearchTreeRegistrationInfo : public FBasicRegistrationInfo {
    GENERATED_BODY()
public:
};

// Type T has to be serializable and should have serializable member with name RegisteredObject
// Warning! You should manually call AddReferencedObjects so objects contained in this registry don't get GC'd
template<typename T>
struct SML_API TInternalRegistryState {
private:
    using KeyType = decltype(((T*)0)->RegisteredObject);

    TArray<TSharedPtr<T>> RegistrationList;
    TMap<KeyType, TSharedPtr<T>> RegistrationMap;

    // Used for fast AddReferencedObjects implementation
    // It cannot be UPROPERTY() because UHT won't understand UPROPERTY() declaration inside template struct
    TArray<UObject*> ReferencedObjects;

    /** Registration counter for this registry. Incremented every time registration occurs */
    int64 RegistrationCounter = 0;
public:
    FORCEINLINE bool ContainsObject(const KeyType& KeyType) const {
        return RegistrationMap.Contains(KeyType);
    }

    FORCEINLINE TSharedPtr<T> RegisterObject(const T& ObjectInfo) {
        check(!ContainsObject(ObjectInfo.RegisteredObject));
        TSharedPtr<T> RegistrationEntry = MakeShareable(new T{ObjectInfo});
        RegistrationList.Add(RegistrationEntry);
        RegistrationMap.Add(RegistrationEntry->RegisteredObject, RegistrationEntry);
        ReferencedObjects.Add(RegistrationEntry->RegisteredObject);
        RegistrationCounter++;
        return RegistrationEntry;
    }

    FORCEINLINE TSharedPtr<T> FindObject(const KeyType& KeyType) const {
        TSharedPtr<T> const* Object = RegistrationMap.Find(KeyType);
        return Object ? *Object : TSharedPtr<T>();
    }

    FORCEINLINE const TArray<TSharedPtr<T>>& GetAllObjects() const {
        return RegistrationList;
    }

    FORCEINLINE int64 GetRegistrationCounter() const {
        return RegistrationCounter;
    }

    FORCEINLINE void AddReferencedObjects(UObject* Outer, FReferenceCollector& ReferenceCollector) {
        ReferenceCollector.AddReferencedObjects(ReferencedObjects, Outer);
    }
};

struct FMissingObjectStruct {
    FString ObjectType;
    FString ObjectPath;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSchematicRegistered, TSubclassOf<UFGSchematic>, Schematic, FSchematicRegistrationInfo, RegistrationInfo);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnResearchTreeRegistered, TSubclassOf<UFGResearchTree>, ResearchTree, FResearchTreeRegistrationInfo, RegistrationInfo);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRecipeRegistered, TSubclassOf<UFGRecipe>, Recipe, FRecipeRegistrationInfo, RegistrationInfo);

/**
 * Manages registration and lifetime of the modded content
 *
 * All modded content of supported types should be registered there
 * to be correctly saved, loaded and identified by the game
 * and other information mods
 *
 * If you don't need any dynamic behaviors, use properties in InitGameWorld instead
 * of calling methods on this object directly
 *
 * Content can only be registered before save is loaded,
 * after that moment registry is frozen and no changes can be made after that
 */
UCLASS()
class SML_API AModContentRegistry : public AModSubsystem {
    GENERATED_BODY()
public:
    AModContentRegistry();

    /** Retrieves global mod content registry instance */
    UFUNCTION(BlueprintPure, DisplayName = "GetModContentRegistry", meta = (DefaultToSelf = "WorldContext"))
    static AModContentRegistry* Get(UObject* WorldContext);

    /**
     * Registers schematic to be usable by the game
     *    All recipes referenced by schematic are registered automatically
     *    All items referenced by registered recipes are associated with passed mod reference too
     *
     * @param ModReference identifier of the mod who is performing this registration
     * @param Schematic class of schematic to be registered
     */
    UFUNCTION(BlueprintCallable, CustomThunk)
    void RegisterSchematic(const FName ModReference, TSubclassOf<UFGSchematic> Schematic);

    /**
     * Registers research tree to be usable by the game
     *     Also updates research trees so tree can be unlocked instantly if criteria are met
     *     All schematics referenced by this research tree are registered automatically
     *     Nodes should be of default BPD_ResearchTreeNode type to be discoverable for schematics
     *
     * @param ModReference identifier of the mod who is performing this registration
     * @param ResearchTree class of research tree being registered
     */
    UFUNCTION(BlueprintCallable, CustomThunk)
    void RegisterResearchTree(const FName ModReference, TSubclassOf<UFGResearchTree> ResearchTree);

    /**
     * Registers recipe to be usable by the game
     *     Also associates items referenced by this recipe with passed mod reference if they are not
     *     associated with any other mod reference yet
     *
     * @param ModReference identifier of the mod who is performing this registration
     * @param Recipe class of recipe being registered
     */
    UFUNCTION(BlueprintCallable, CustomThunk)
    void RegisterRecipe(const FName ModReference, TSubclassOf<UFGRecipe> Recipe);

    /** Register resource sink item points for each item row in the passed table object */
    UFUNCTION(BlueprintCallable, CustomThunk)
    void RegisterResourceSinkItemPointTable(const FName ModReference, UDataTable* PointTable);

    /** Retrieves list of all currently loaded item descriptors */
    UFUNCTION(BlueprintPure)
    TArray<FItemRegistrationInfo> GetLoadedItemDescriptors();

    /** Retrieves list of all obtainable item descriptors, e.g ones referenced by any recipe */
    UFUNCTION(BlueprintPure)
    TArray<FItemRegistrationInfo> GetObtainableItemDescriptors() const;

    /** Retrieves registration entry for item descriptor */
    UFUNCTION(BlueprintPure)
    FItemRegistrationInfo GetItemDescriptorInfo(TSubclassOf<UFGItemDescriptor> ItemDescriptor);

    /** Retrieves list of all currently registered recipes*/
    UFUNCTION(BlueprintPure)
    FORCEINLINE TArray<FRecipeRegistrationInfo> GetRegisteredRecipes() const {
        TArray<FRecipeRegistrationInfo> RegistrationInfos;
        for (const TSharedPtr<FRecipeRegistrationInfo>& RegistrationInfo : RecipeRegistryState.GetAllObjects()) {
            RegistrationInfos.Add(*RegistrationInfo);
        }
        return RegistrationInfos;
    }

    /** Retrieves registration entry for recipe */
    UFUNCTION(BlueprintPure)
    FORCEINLINE FRecipeRegistrationInfo GetRecipeInfo(TSubclassOf<UFGRecipe> Recipe) const {
        const TSharedPtr<FRecipeRegistrationInfo> RegistrationInfo = RecipeRegistryState.FindObject(Recipe);
        return RegistrationInfo.IsValid() ? *RegistrationInfo : FRecipeRegistrationInfo{};
    }

    /** Retrieves list of all currently registered research trees */
    UFUNCTION(BlueprintPure)
    FORCEINLINE TArray<FResearchTreeRegistrationInfo> GetRegisteredResearchTrees() const {
        TArray<FResearchTreeRegistrationInfo> RegistrationInfos;
        for (const TSharedPtr<FResearchTreeRegistrationInfo>& RegistrationInfo : ResearchTreeRegistryState.GetAllObjects()) {
            RegistrationInfos.Add(*RegistrationInfo);
        }
        return RegistrationInfos;
    }

    /** Retrieves registration entry for research tree */
    UFUNCTION(BlueprintPure)
    FORCEINLINE FResearchTreeRegistrationInfo GetResearchTreeRegistrationInfo(TSubclassOf<UFGResearchTree> ResearchTree) const {
        const TSharedPtr<FResearchTreeRegistrationInfo> RegistrationInfo = ResearchTreeRegistryState.FindObject(ResearchTree);
        return RegistrationInfo.IsValid() ? *RegistrationInfo : FResearchTreeRegistrationInfo{};
    }

    /** Retrieves list of all currently registered research trees */
    UFUNCTION(BlueprintPure)
    FORCEINLINE TArray<FSchematicRegistrationInfo> GetRegisteredSchematics() const {
        TArray<FSchematicRegistrationInfo> RegistrationInfos;
        for (const TSharedPtr<FSchematicRegistrationInfo>& RegistrationInfo : SchematicRegistryState.GetAllObjects()) {
            RegistrationInfos.Add(*RegistrationInfo);
        }
        return RegistrationInfos;
    }

    /** Retrieves registration entry for schematic */
    UFUNCTION(BlueprintPure)
    FORCEINLINE FSchematicRegistrationInfo GetSchematicRegistrationInfo(TSubclassOf<UFGSchematic> Schematic) const {
        const TSharedPtr<FSchematicRegistrationInfo> RegistrationInfo = SchematicRegistryState.FindObject(Schematic);
        return RegistrationInfo.IsValid() ? *RegistrationInfo : FSchematicRegistrationInfo{};
    }

    /** Returns true when given recipe is registered */
    UFUNCTION(BlueprintPure)
    FORCEINLINE bool IsRecipeRegistered(TSubclassOf<UFGRecipe> Recipe) const {
        return Recipe != NULL && RecipeRegistryState.ContainsObject(Recipe);
    }

    /** Returns true when given schematic is registered */
    UFUNCTION(BlueprintPure)
    FORCEINLINE bool IsSchematicRegistered(TSubclassOf<UFGSchematic> Schematic) const {
        return Schematic != NULL && SchematicRegistryState.ContainsObject(Schematic);
    }

    /** Returns true when given research tree is registered */
    UFUNCTION(BlueprintPure)
    FORCEINLINE bool IsResearchTreeRegistered(TSubclassOf<UFGResearchTree> ResearchTree) const {
        return ResearchTree != NULL && ResearchTreeRegistryState.ContainsObject(ResearchTree);
    }

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // Add objects from registry states to reference collector
    static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

    // Callbacks to be fired when new entries are registered

    /** Called when recipe is registered into content registry */
    UPROPERTY(BlueprintAssignable)
    FOnRecipeRegistered OnRecipeRegistered;

    /** Called when schematic is registered into content registry */
    UPROPERTY(BlueprintAssignable)
    FOnSchematicRegistered OnSchematicRegistered;

    /** Called when research tree is registered into the registry */
    UPROPERTY(BlueprintAssignable)
    FOnResearchTreeRegistered OnResearchTreeRegistered;
protected:
    /** Called early when subsystem is spawned */
    virtual void Init() override;
private:
    friend class FSatisfactoryModLoader;
    friend class UWorldModuleManager;

    /** True if content registry is frozen and does not accept registrations anymore */
    bool bIsRegistryFrozen;

    /** Internal state counters for vanilla managers */
    int64 SchematicManagerInternalState;
    int64 ResearchManagerInternalState;

    /** True when we have subscribed to schematic manager delegates already */
    bool bSubscribedToSchematicManager;

    /** Pending registrations of item sink point tables */
    UPROPERTY()
    TMap<UDataTable*, FName> PendingItemSinkPointsRegistrations;

    /** Pointer to the currently active script callstack frame, used for debugging purposes */
    FFrame* ActiveScriptFramePtr;

    /**
     * Map with cached registration info for each discovered item descriptor class
     * Since item descriptors are discovered in runtime and don't have unified registry,
     * this map is not guaranteed to contain all item descriptors in the game
     * Although it will contain all item descriptors referenced by recipes
     */
    TInternalRegistryState<FItemRegistrationInfo> ItemRegistryState;

    /** List of all registered recipes */
    TInternalRegistryState<FRecipeRegistrationInfo> RecipeRegistryState;

    /** List of all registered schematics */
    TInternalRegistryState<FSchematicRegistrationInfo> SchematicRegistryState;

    /** List of all registered research trees */
    TInternalRegistryState<FResearchTreeRegistrationInfo> ResearchTreeRegistryState;

    /** Flushes schematic registry state into schematic manager */
    void FlushStateToSchematicManager(class AFGSchematicManager* SchematicManager) const;

    /** Flushes research tree registry state into research manager */
    void FlushStateToResearchManager(class AFGResearchManager* ResearchManager) const;

    /** Subscribes to schematic manager delegates */
    void SubscribeToSchematicManager(AFGSchematicManager* SchematicManager);

    /** Injects into vanilla schematic/research managers to override vanilla content registration to registry usage */
    static void DisableVanillaContentRegistration();

    /** Quick version of UBlueprintAssetHelperLibrary, using predefined mod reference for vanilla content */
    static FName FindContentOwnerFast(UClass* ContentClass);

    /** Called when module content registration is finished and registry can be frozen */
    void NotifyModuleRegistrationFinished();

    /** Flushed pending resource sink registrations into the resource sink subsystem, if it is available */
    void FlushPendingResourceSinkRegistrations();

    /** Freezes registry in place and clears out all unreferenced objects */
    void FreezeRegistryState();
    /** Ensures that registry is not frozen and we can perform registration */
    void EnsureRegistryUnfrozen() const;
    /** Checks SaveGame fields for unregistered objects and NULLs. Requires registry to be frozen already */
    void CheckSavedDataForMissingObjects();
    /** Unlocks tutorial schematics if it's needed */
    void UnlockTutorialSchematics();

    /** Called when schematic is purchased in schematic manager */
    UFUNCTION()
    void OnSchematicPurchased(TSubclassOf<UFGSchematic> Schematic);

    /** Associate items referenced in recipe with given mod reference if they are not associated already */
    void MarkItemDescriptorsFromRecipe(const TSubclassOf<UFGRecipe>& Recipe, const FName ModReference);

    TSharedPtr<FItemRegistrationInfo> RegisterItemDescriptor(const FName OwnerModReference, const FName RegistrarModReference, const TSubclassOf<UFGItemDescriptor>& ItemDescriptor);

    void FindMissingSchematics(class AFGSchematicManager* SchematicManager, TArray<FMissingObjectStruct>& MissingObjects) const;
    void FindMissingResearchTrees(class AFGResearchManager* ResearchManager, TArray<FMissingObjectStruct>& MissingObjects) const;
    void FindMissingRecipes(class AFGRecipeManager* RecipeManager, TArray<FMissingObjectStruct>& MissingObjects) const;
    static void WarnAboutMissingObjects(const TArray<FMissingObjectStruct>& MissingObjects);

    template<typename T>
    FORCEINLINE static T MakeRegistrationInfo(UClass* Class, const FName OwnedByModReference, const FName RegisteredByModReference) {
        T RegistrationInfo;
        RegistrationInfo.RegistrarModReference = RegisteredByModReference;
        RegistrationInfo.OwnedByModReference = OwnedByModReference;
        RegistrationInfo.RegisteredObject = Class;
        return RegistrationInfo;
    }

    // Custom Thunks for calling Register functions through Reflection, primary point of which
    // is providing FFrame callstack context to the registration methods for debugging in seamless manner
    DECLARE_FUNCTION(execRegisterResourceSinkItemPointTable);
    DECLARE_FUNCTION(execRegisterRecipe);
    DECLARE_FUNCTION(execRegisterResearchTree);
    DECLARE_FUNCTION(execRegisterSchematic);
};
