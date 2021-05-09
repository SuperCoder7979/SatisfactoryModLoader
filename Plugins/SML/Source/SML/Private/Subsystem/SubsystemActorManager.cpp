#include "Subsystem/SubsystemActorManager.h"
#include "Command/ChatCommandLibrary.h"
#include "ModLoading/PluginModuleLoader.h"
#include "Registry/ModContentRegistry.h"
#include "Util/BlueprintAssetHelperLibrary.h"

DEFINE_LOG_CATEGORY(LogSubsystemManager)

void USubsystemActorManager::SpawnSubsystemActor(const TSubclassOf<AModSubsystem> SubsystemClass) {
    const ESubsystemReplicationPolicy ReplicationPolicy = SubsystemClass.GetDefaultObject()->ReplicationPolicy;

    const FString OwnerModReference = UBlueprintAssetHelperLibrary::FindPluginNameByObjectPath(SubsystemClass->GetOutermost()->GetPathName());
    const FString SubsystemName = FString::Printf(TEXT("%s_%s"), *OwnerModReference, *SubsystemClass->GetName());

    AModSubsystem* SpawnedSubsystem = FindSubsystemActorByName(SubsystemClass, *SubsystemName);

    // We only want to modify subsystem actor if we should spawn it with given policy at all
    if (ShouldSpawnSubsystemWithPolicy(ReplicationPolicy)) {

        // Spawn new actor if we don't have one loaded from the game save
        if (SpawnedSubsystem == NULL) {
            FActorSpawnParameters SpawnParameters{};
            SpawnParameters.Name = *SubsystemName;
            SpawnedSubsystem = GetWorld()->SpawnActor<AModSubsystem>(SubsystemClass, SpawnParameters);
        }

        // Set bIsReplicated to true on actor if our policy is SpawnOnServer_Replicated
        if (ReplicationPolicy == ESubsystemReplicationPolicy::SpawnOnServer_Replicate) {
            SpawnedSubsystem->SetReplicates(true);
        }
    }

    // If we actually got mod subsystem actor, we should make sure Init() is called on it
    // We want to call it manually here since often subsystems will be registered before World#BeginPlay is dispatched, meaning that BeginPlay will get dispatched much later
    // but we still want to get subsystems into the valid state usable by mod modules
    // And make sure it received actor spawned callback, fire it manually (it might've been loaded before we registered our callback pretty much)
    if (SpawnedSubsystem) {
        SpawnedSubsystem->DispatchInit();
        OnWorldActorCreated(SpawnedSubsystem);
    }
}

bool USubsystemActorManager::ShouldSpawnSubsystemWithPolicy(const ESubsystemReplicationPolicy Policy) {
    const ENetMode ActiveNetMode = GetWorld()->GetNetMode();

    // Local subsystems are always spawned on all sides
    if (Policy == ESubsystemReplicationPolicy::SpawnLocal) {
        return true;
    }

    // We want to spawn subsystem on client only if we actually represent client and not dedicated server
    if (Policy == ESubsystemReplicationPolicy::SpawnOnClient) {
        return ActiveNetMode != ENetMode::NM_DedicatedServer;
    }

    // We want to spawn server subsystems everywhere but on the connected remote clients
    if (Policy == ESubsystemReplicationPolicy::SpawnOnServer ||
        Policy == ESubsystemReplicationPolicy::SpawnOnServer_Replicate) {
        return ActiveNetMode != ENetMode::NM_Client;
    }

    checkf(0, TEXT("Unreachable statement"));
    return false;
}

void USubsystemActorManager::OnWorldActorCreated(AActor* SpawnedActor) {
    if (SpawnedActor->IsA<AModSubsystem>()) {
        const TSubclassOf<AModSubsystem> SubsystemClass = SpawnedActor->GetClass();
        if (RegisteredSubsystems.Contains(SubsystemClass) && !SubsystemActors.Contains(SubsystemClass)) {

            AModSubsystem* Subsystem = CastChecked<AModSubsystem>(SpawnedActor);
            this->SubsystemActors.Add(SubsystemClass, Subsystem);
            this->OnModSubsystemAvailable.Broadcast(Subsystem);
        }
    }
}

USubsystemActorManager::USubsystemActorManager() {
    this->bNativeSubsystemsRegistered = false;
}

void USubsystemActorManager::RegisterSubsystemActor(TSubclassOf<AModSubsystem> SubsystemClass) {
    checkf(SubsystemClass, TEXT("Attempt to register NULL ModSubsystem"));
    checkf(!SubsystemClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Attempt to register Abstract ModSubsystem: '%s'"), *SubsystemClass->GetPathName());

    if (!RegisteredSubsystems.Contains(SubsystemClass)) {
        UE_LOG(LogSubsystemManager, Log, TEXT("Registered subsystem class '%s'"), *SubsystemClass->GetPathName());

        this->RegisteredSubsystems.Add(SubsystemClass);
        SpawnSubsystemActor(SubsystemClass);
    }
}

void USubsystemActorManager::WaitForSubsystem(TSubclassOf<AModSubsystem> SubsystemClass, FLatentActionInfo& LatentInfo) {
    checkf(SubsystemClass, TEXT("Attempt to WaitForSubsystem on NULL SubsystemClass"));
    checkf(RegisteredSubsystems.Contains(SubsystemClass), TEXT("Attempt to WaitForSubsystem on Unregistered SubsystemClass '%s'"), *SubsystemClass->GetPathName());

    FLatentActionManager& ActionManager = GetWorld()->GetLatentActionManager();
    if (ActionManager.FindExistingAction<FWaitForSubsystemLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr) {
        ActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FWaitForSubsystemLatentAction(LatentInfo, SubsystemClass, this));
    }
}

AModSubsystem* USubsystemActorManager::K2_GetSubsystemActor(TSubclassOf<AModSubsystem> SubsystemClass) {
    AModSubsystem** SubsystemEntry = SubsystemActors.Find(SubsystemClass);
    return SubsystemEntry ? *SubsystemEntry : NULL;
}

bool USubsystemActorManager::ShouldCreateSubsystem(UObject* Outer) const {
    UWorld* WorldOuter = CastChecked<UWorld>(Outer);
    return FPluginModuleLoader::ShouldLoadModulesForWorld(WorldOuter);
}

void USubsystemActorManager::Initialize(FSubsystemCollectionBase& Collection) {
    GetWorld()->OnActorsInitialized.AddUObject(this, &USubsystemActorManager::OnWorldActorsInitialized);
    GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &USubsystemActorManager::OnWorldActorCreated));
}

void USubsystemActorManager::MakeSureNativeSubsystemsRegistered() {
    // Register native SML actor subsystems
    if (!FPluginModuleLoader::IsMainMenuWorld(GetWorld()) && !bNativeSubsystemsRegistered) {
        UE_LOG(LogSubsystemManager, Log, TEXT("Registering default SML subsystems"));
        RegisterSubsystemActor(AModContentRegistry::StaticClass());
        RegisterSubsystemActor(AChatCommandSubsystem::StaticClass());
        this->bNativeSubsystemsRegistered = true;
    }
}

void USubsystemActorManager::OnWorldActorsInitialized(const UWorld::FActorsInitializedParams&) {
    MakeSureNativeSubsystemsRegistered();
}

AModSubsystem* USubsystemActorManager::FindSubsystemActorByName(TSubclassOf<AModSubsystem> ActorClass, const FName ActorName) const {
    for (TActorIterator<AModSubsystem> It(GetWorld(), ActorClass, EActorIteratorFlags::AllActors); It; ++It) {
        AModSubsystem* CurrentActor = *It;
        if (CurrentActor->GetFName() == ActorName) {
            return CurrentActor;
        }
    }
    return NULL;
}

void FWaitForSubsystemLatentAction::UpdateOperation(FLatentResponse& Response) {
    USubsystemActorManager* SubsystemActorManager = SubsystemManager.Get();
    const TSubclassOf<AModSubsystem> SubsystemClassPinned = SubsystemClass.Get();

    bool bHasCompletedTask = false;

    // Check whenever GetSubsystemActor returns something different from NULL, then we are done
    if (SubsystemActorManager && SubsystemClassPinned) {
        AModSubsystem* TargetSubsystem = SubsystemActorManager->K2_GetSubsystemActor(SubsystemClassPinned);
        if (TargetSubsystem) {
            bHasCompletedTask = true;
        }
    } else {
        // Subsystem Actor Manager or Subsystem Class have been Garbage Collected,
        // we should stop now and just return NULL because otherwise we would never complete
        bHasCompletedTask = true;
    }

    Response.FinishAndTriggerIf(bHasCompletedTask, ExecutionFunction, OutputLink, CallbackTarget);
}

#if WITH_EDITOR
FString FWaitForSubsystemLatentAction::GetDescription() const {
    const TSubclassOf<AModSubsystem> SubsystemClassPinned = SubsystemClass.Get();
    return FString::Printf(TEXT("Wait Until Mod Subsystem '%s' is Available"), *SubsystemClassPinned->GetPathName());
}
#endif
