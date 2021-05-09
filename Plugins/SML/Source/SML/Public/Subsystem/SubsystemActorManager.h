#pragma once
#include "CoreMinimal.h"
#include "LatentActions.h"
#include "Subsystem/ModSubsystem.h"
#include "SubsystemActorManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSubsystemManager, Log, All)

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModSubsystemAvailable, AModSubsystem*, Subsystem);

UCLASS()
class SML_API USubsystemActorManager : public UWorldSubsystem {
    GENERATED_BODY()
private:
    /** Map of subsystem class to the actor of the corresponding type. Contains both replicated and spawned subsystems */
    UPROPERTY()
    TMap<TSubclassOf<AModSubsystem>, AModSubsystem*> SubsystemActors;

    /** Array of registered subsystem classes and their corresponding policies, used to verify WaitForSubsystem call */
    UPROPERTY()
    TSet<TSubclassOf<AModSubsystem>> RegisteredSubsystems;

    /** Called when modded subsystem instance has been spawned and initialized */
    UPROPERTY()
    FOnModSubsystemAvailable OnModSubsystemAvailable;

    bool bNativeSubsystemsRegistered;
public:
    USubsystemActorManager();

    /** Registers subsystem actor and spawns it in the world instantly, if it's applicable to spawning with provided policy */
    UFUNCTION(BlueprintCallable)
    void RegisterSubsystemActor(TSubclassOf<AModSubsystem> SubsystemClass);

    /** Waits until provided Modded SubsystemClass is Available and can be retrieved through GetSubsystemActor */
    UFUNCTION(BlueprintCallable, meta = (Latent, LatentInfo = "LatentInfo"))
    void WaitForSubsystem(TSubclassOf<AModSubsystem> SubsystemClass, struct FLatentActionInfo& LatentInfo);

    /** Retrieves subsystem actor of the provided class, or NULL if it has not been created or replicated yet */
    UFUNCTION(BlueprintPure, meta = (DisplayName = "GetSubsystemActor", DeterminesOutputType = "SubsystemClass"))
    AModSubsystem* K2_GetSubsystemActor(TSubclassOf<AModSubsystem> SubsystemClass);

    /** Retrieves subsystem actor of the provided class or NULL if it does not exist, template version for C++ mods */
    template<typename T>
    FORCEINLINE T* GetSubsystemActor() {
        return Cast<T>(K2_GetSubsystemActor(T::StaticClass()));
    }

    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Makes sure native SML subsystems are registered and ready for use */
    void MakeSureNativeSubsystemsRegistered();
private:
    friend AModSubsystem;

    /** Called when world actors are initialized */
    void OnWorldActorsInitialized(const UWorld::FActorsInitializedParams& Params);

    /** Spawns subsystem actor with the registered replication policy */
    void SpawnSubsystemActor(TSubclassOf<AModSubsystem> SubsystemClass);

    /** Determines whenever we should spawn subsystem with given policy on our side */
    bool ShouldSpawnSubsystemWithPolicy(ESubsystemReplicationPolicy Policy);

    /** Called when new actor is spawned in the world, happens both when actor is spawned through replication and on client */
    void OnWorldActorCreated(AActor* SpawnedActor);

    /** Tries to find existing subsystem actor in the world by name */
    AModSubsystem* FindSubsystemActorByName(TSubclassOf<AModSubsystem> ActorClass, const FName ActorName) const;
};

class SML_API FWaitForSubsystemLatentAction final : public FPendingLatentAction {
private:
    FName ExecutionFunction;
    int32 OutputLink;
    FWeakObjectPtr CallbackTarget;

    TWeakObjectPtr<UClass> SubsystemClass;
    TWeakObjectPtr<USubsystemActorManager> SubsystemManager;
public:
    FORCEINLINE FWaitForSubsystemLatentAction(const FLatentActionInfo& LatentInfo, const TSubclassOf<AModSubsystem> SubsystemActorClass, USubsystemActorManager* SubsystemActorManager):
        ExecutionFunction(LatentInfo.ExecutionFunction),
        OutputLink(LatentInfo.Linkage),
        CallbackTarget(LatentInfo.CallbackTarget),
        SubsystemClass(SubsystemActorClass.Get()),
        SubsystemManager(SubsystemActorManager) {
    }

    virtual void UpdateOperation(FLatentResponse& Response) override;

#if WITH_EDITOR
    virtual FString GetDescription() const override;
#endif
};
