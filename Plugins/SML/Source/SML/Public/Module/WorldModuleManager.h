#pragma once
#include "CoreMinimal.h"
#include "LatentActions.h"
#include "Module/WorldModule.h"
#include "WorldModuleManager.generated.h"

/** Manages registered world modules and their lifecycle events */
UCLASS(NotBlueprintable)
class SML_API UWorldModuleManager : public UWorldSubsystem {
    GENERATED_BODY()
private:
    /** Root map of modules for every registered mod reference */
    UPROPERTY()
    TMap<FName, UWorldModule*> RootModuleMap;

    /** Root module list for fast iteration according to order of registration */
    UPROPERTY()
    TArray<UWorldModule*> RootModuleList;
public:
    /** Retrieves world module by provided mod reference */
    UFUNCTION(BlueprintPure)
    UWorldModule* FindModule(const FName& ModReference) const;

    /**
     * Waits until Game State Actor is fully Replicated and Available for retrieval via Get Game State function.
     * Also waits until Factory Game client subsystems are fully replicated before returning
     */
    UFUNCTION(BlueprintCallable, meta = (Latent, LatentInfo = "LatentInfo"))
    static void WaitForGameState(struct FLatentActionInfo& LatentInfo);

    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
private:
    /** Called very early to construct module objects, right after world initialization */
    void ConstructModules();

    /** Called when world actors have been initialized */
    void InitializeModules(const UWorld::FActorsInitializedParams& Params);

    /** Called when world post initialization has been completed */
    void PostInitializeModules();

    /** Notifies content registry that modded content registration has been finished */
    void NotifyContentRegistry();

    /** Allocates root module object for instance and registers it */
    void CreateRootModule(const FName& ModReference, TSubclassOf<UWorldModule> ObjectClass);

    /** Dispatches lifecycle event to all registered modules */
    void DispatchLifecycleEvent(ELifecyclePhase Phase);
};

class SML_API FWaitForGameStateLatentAction final : public FPendingLatentAction {
private:
    FName ExecutionFunction;
    int32 OutputLink;
    FWeakObjectPtr CallbackTarget;
    TWeakObjectPtr<UWorld> TargetWorld;

public:
    FORCEINLINE explicit FWaitForGameStateLatentAction(const FLatentActionInfo& LatentInfo, UWorld* InTargetWorld):
        ExecutionFunction(LatentInfo.ExecutionFunction),
        OutputLink(LatentInfo.Linkage),
        CallbackTarget(LatentInfo.CallbackTarget),
        TargetWorld(InTargetWorld) {
    }

    virtual void UpdateOperation(FLatentResponse& Response) override;

#if WITH_EDITOR
    virtual FString GetDescription() const override;
#endif
};
