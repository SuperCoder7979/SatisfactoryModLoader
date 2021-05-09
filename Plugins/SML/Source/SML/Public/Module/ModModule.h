#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"
#include "ModModule.generated.h"

/** Describes lifecycle phase of mod module */
UENUM(BlueprintType)
enum class ELifecyclePhase : uint8 {
    /** Dispatched right after module is loaded */
    CONSTRUCTION UMETA(DisplayName = "Construction"),

    /** Dispatched when all mod modules were loaded and constructed */
    INITIALIZATION UMETA(DisplayName = "Initialization"),

    /** Called after all mod modules have been initialized */
    POST_INITIALIZATION UMETA(DisplayName = "Post Initialization")
};

/** Describes abstract module loaded by mod loading system */
UCLASS(Blueprintable)
class SML_API UModModule : public UObject {
    GENERATED_BODY()
private:
    /** Private field holding owner mod reference, accessible directly only by mod loader */
    FName OwnerModReference;

    /** Child modules of this module */
    UPROPERTY()
    TMap<FName, UModModule*> ChildModules;

    /** Events that we already received. Will be dispatched immediately on child modules */
    TArray<ELifecyclePhase> EventsReceived;
public:
    /**
    * Indicates that this module is considered a root module
    * Each mod can only contain one root module of each particular type,
    * e.g GameWorld, MenuWorld and GameInstance types.
    * Root modules will be automatically discovered and loaded for every mod, regardless of their asset name
    */
    UPROPERTY(AssetRegistrySearchable, EditDefaultsOnly)
    bool bRootModule;

    /** ModReference of the mod this module belongs to */
    UFUNCTION(BlueprintPure)
    FORCEINLINE FName GetOwnerModReference() const { return OwnerModReference; }

    /** Spawns child module and dispatches lifecycle events on it */
    UFUNCTION(BlueprintCallable)
    UModModule* SpawnChildModule(FName ModuleName, TSoftClassPtr<UModModule> ModuleClass);

    /** Retrieves child module with specified name from this module. Does automatic type checking */
    UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "ModuleClass"))
    UModModule* GetChildModule(FName ModuleName, TSubclassOf<UModModule> ModuleClass);

    /** Handles received lifecycle event and dispatches it to all modules */
    virtual void DispatchLifecycleEvent(ELifecyclePhase Phase);

    static FString LifecyclePhaseToString(ELifecyclePhase Phase);
protected:
    /** Called when module receives lifetime event */
    UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Lifecycle Event"))
    void K2_OnLifecycleEvent(ELifecyclePhase Phase);

    /** Updates associated mod reference. !!! DO NOT CALL DIRECTLY !!! */
    FORCEINLINE void SetOwnerModReference(const FName NewOwnerModReference) { this->OwnerModReference = NewOwnerModReference; }
};
