#include "Module/GameWorldModule.h"
#include "Command/ChatCommandLibrary.h"
#include "Registry/ModContentRegistry.h"
#include "Subsystem/SubsystemActorManager.h"

void UGameWorldModule::DispatchLifecycleEvent(ELifecyclePhase Phase) {
    // Register default content before calling blueprint event logic
    if (Phase == ELifecyclePhase::CONSTRUCTION) {
        RegisterConstructionPhaseContent();
    }

    if (Phase == ELifecyclePhase::INITIALIZATION) {
        RegisterDefaultContent();
    }
    Super::DispatchLifecycleEvent(Phase);
}

void UGameWorldModule::RegisterConstructionPhaseContent() {
    // Register mod subsystem actors very early so other mods can access them during Initialization phase
    UWorld* WorldObject = GetWorld();
    USubsystemActorManager* SubsystemActorManager = WorldObject->GetSubsystem<USubsystemActorManager>();
    check(SubsystemActorManager);

    for (const TSubclassOf<AModSubsystem>& SubsystemClass : ModSubsystems) {
        SubsystemActorManager->RegisterSubsystemActor(SubsystemClass);
    }
}

void UGameWorldModule::RegisterDefaultContent() {
    // Register default content
    UWorld* WorldObject = GetWorld();
    AModContentRegistry* ModContentRegistry = AModContentRegistry::Get(WorldObject);
    AChatCommandSubsystem* ChatCommandSubsystem = AChatCommandSubsystem::Get(WorldObject);
    check(ModContentRegistry);

    // Register schematics
    for (const TSubclassOf<UFGSchematic>& Schematic : mSchematics) {
        ModContentRegistry->RegisterSchematic(GetOwnerModReference(), Schematic);
    }

    // Register research trees
    for (const TSubclassOf<UFGResearchTree>& ResearchTree : mResearchTrees) {
        ModContentRegistry->RegisterResearchTree(GetOwnerModReference(), ResearchTree);
    }

    // Register resource sink table points
    UDataTable* ModResourceSinkPointsTable = mResourceSinkItemPointsTable.LoadSynchronous();
    if (ModResourceSinkPointsTable != NULL) {
        ModContentRegistry->RegisterResourceSinkItemPointTable(GetOwnerModReference(), ModResourceSinkPointsTable);
    }

    // Register chat commands (on server side only)
    if (ChatCommandSubsystem != NULL) {
        for (const TSubclassOf<AChatCommandInstance>& ChatCommand : mChatCommands) {
            ChatCommandSubsystem->RegisterCommand(GetOwnerModReference().ToString(), ChatCommand);
        }
    }
}
