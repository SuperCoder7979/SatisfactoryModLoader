#pragma once
#include "CoreMinimal.h"

class UCheatManager;

/**
 * Patch allowing use of PlayerFly, Fly and PlayerFly_GhostMode cheat commands in shipping
 * They don't work by default because ACharacter::ClientCheatWalk/Fly/Ghost_Implementation methods
 * are left empty in shipping configuration. We cannot really fix them since they're empty -> no place for hooking,
 * so instead we hook cheat manager and replace behaviour of commands to calling contents of these methods directly
 */
class SML_API FCheatManagerPatch {
private:
    friend class FSatisfactoryModLoader;

    static void HandlePlayerFly(UCheatManager* CheatManager, bool bFlyModeEnabled);
    static void HandleNoClipModeOnFly(UCheatManager* CheatManager, bool bGhostMode);

    static void RegisterPatch();
};
