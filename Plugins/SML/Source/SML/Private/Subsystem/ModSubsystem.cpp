#include "Subsystem/ModSubsystem.h"

AModSubsystem::AModSubsystem() {
    this->bNetLoadOnClient = false;
    this->bReplicates = false;
    this->bInitDispatched = false;
}

void AModSubsystem::BeginPlay() {
    Super::BeginPlay();

    // Dispatch init if it hasn't been dispatched already
    // It can happen for example when actor is spawned after world already had BeginPlay dispatched
    // In that case we still want to have Init() called before BeginPlay
    DispatchInit();
}

void AModSubsystem::DispatchInit() {
    if (!bInitDispatched) {
        Init();
        this->bInitDispatched = true;
    }
}
