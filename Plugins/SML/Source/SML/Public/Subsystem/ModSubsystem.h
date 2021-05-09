#pragma once
#include "CoreMinimal.h"
#include "FGSubsystem.h"
#include "ModSubsystem.generated.h"

/** Describes subsystem policy regarding replication and spawning */
UENUM(BlueprintType)
enum class ESubsystemReplicationPolicy : uint8 {
    /** Subsystem will be spawned on server and will not be replicated to remote clients */
    SpawnOnServer UMETA(DisplayName = "Spawn on Server, Do Not Replicate"),

    /** Subsystem will be spawned on server and replicated to remote clients */
    SpawnOnServer_Replicate UMETA(DisplayName = "Spawn on Server, Replicate"),

    /** Subsystem will be spawned on remote clients only (like SpawnLocal, but excludes dedicated servers) */
    SpawnOnClient UMETA(DisplayName = "Spawn on Client Only"),

    /** Subsystem will be spawned on both server and clients, instances of subsystem will be local and never replicated */
    SpawnLocal UMETA(DisplayName = "Spawn on Client and Server, Never Replicate")
};

/**
 * Describes abstract mod subsystem with lifetime bound to world's lifetime and lifecycle managed by SubsystemActorManager.
 * MAKE SURE TO CALL SUPER FUNCTIONS IF YOU"RE OVERRIDING BEGIN PLAY AND END PLAY IN C++ MODS!
 */
UCLASS(Blueprintable, HideCategories = ("Replication"))
class SML_API AModSubsystem : public AFGSubsystem {
    GENERATED_BODY()
private:
    /** True when this actor has already received Init() call */
    bool bInitDispatched;
public:
    /** Replication policy used by this mod subsystem */
    UPROPERTY(EditDefaultsOnly, Category = "Mod Subsystem")
    ESubsystemReplicationPolicy ReplicationPolicy;

    /** Override NetLoadOnClient to false by default because it messes with our replication policy */
    AModSubsystem();
protected:
    /** Override BeginPlay to ensure that Init is dispatched before child classes receive BeginPlay */
    virtual void BeginPlay() override;

    /** Called right after system has been spawned, always before BeginPlay is called */
    virtual void Init() {
    };
private:
    friend class USubsystemActorManager;

    /** Dispatches Init() call to this subsystem if it has not been dispatched already */
    void DispatchInit();

HIDE_ACTOR_TRANSFORM_FUNCTIONS();
};
