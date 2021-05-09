#pragma once
#include "CoreMinimal.h"

/** Handles enabling cheats for players joining when enableCheatConsoleCommands SML config option is enabled */
class FPlayerCheatManagerHandler {
public:
    /** Called when player joins game mode */
    static void HandlePlayerJoined(class AGameModeBase* GameMode, class APlayerController* PlayerController);
private:
    friend class FSatisfactoryModLoader;

    /** Registers cheat manager handler in game mode callbacks */
    static void RegisterHandler();
};
