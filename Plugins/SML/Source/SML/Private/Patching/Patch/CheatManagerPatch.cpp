#include "Patching/Patch/CheatManagerPatch.h"
#include "FGCheatManager.h"
#include "FGPlayerController.h"
#include "Patching/NativeHookManager.h"
#include "FGCharacterMovementComponent.h"

void FCheatManagerPatch::HandlePlayerFly(UCheatManager* CheatManager, bool bFlyModeEnabled) {
    APlayerController* PlayerController = CheatManager->GetOuterAPlayerController();
    if (PlayerController->IsA<AFGPlayerController>()) {
        ACharacter* Character = PlayerController->GetCharacter();
        if (Character->IsA<AFGCharacterPlayer>()) {

            UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement();
            Character->SetActorEnableCollision(true);
            if (CharacterMovement) {
                CharacterMovement->bCheatFlying = bFlyModeEnabled;
                CharacterMovement->SetMovementMode(bFlyModeEnabled ? MOVE_Flying : MOVE_Falling);
            }

        }
    }
}

void FCheatManagerPatch::HandleNoClipModeOnFly(UCheatManager* CheatManager, bool bGhostMode) {
    APlayerController* PlayerController = CheatManager->GetOuterAPlayerController();
    if (PlayerController->IsA<AFGPlayerController>()) {
        ACharacter* Character = PlayerController->GetCharacter();
        if (Character->IsA<AFGCharacterPlayer>()) {

            UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement();
            if (CharacterMovement->IsA<UFGCharacterMovementComponent>()) {
                Character->SetActorEnableCollision(!bGhostMode);
            }
        }
    }
}

void FCheatManagerPatch::RegisterPatch() {
    UFGCheatManager* CheatManagerCDO = GetMutableDefault<UFGCheatManager>();

    // Hook FactoryGame-specific cheats accepting booleans as arguments
    SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGCheatManager::PlayerFly, CheatManagerCDO, &FCheatManagerPatch::HandlePlayerFly);
    SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGCheatManager::PlayerNoClipModeOnFly, CheatManagerCDO, &FCheatManagerPatch::HandleNoClipModeOnFly);

    // Hook UE4 vanilla cheats acting as a shortcuts and not accepting any arguments
    SUBSCRIBE_METHOD_VIRTUAL_AFTER(
        UCheatManager::Fly,
        CheatManagerCDO,
        [](UCheatManager* CheatManager) {
        HandlePlayerFly(CheatManager, true);
        }
    );
    SUBSCRIBE_METHOD_VIRTUAL_AFTER(
        UCheatManager::Walk,
        CheatManagerCDO,
        [](UCheatManager* CheatManager) {
        HandlePlayerFly(CheatManager, false);
        }
    );
    SUBSCRIBE_METHOD_VIRTUAL_AFTER(
        UCheatManager::Ghost,
        CheatManagerCDO,
        [](UCheatManager* CheatManager) {
        HandlePlayerFly(CheatManager, true);
        HandleNoClipModeOnFly(CheatManager, true);
        }
    );
}
