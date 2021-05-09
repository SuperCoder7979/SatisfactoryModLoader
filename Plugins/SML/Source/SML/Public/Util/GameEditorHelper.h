#pragma once
#include "CoreMinimal.h"

/**
 * This class provides certain routines making some aspects of
 * programming for editor and shipping game easier.
 * It attempts to simplify certain aspects and provide configuration-agnostic
 * APIs to depend on to build stuff working in all configurations
 */
class SML_API FGameEditorHelper {
public:
    /**
     * Returns main top-level window of the UE
     * In Unreal Editor, it corresponds to the editor mainframe root window
     * In Game, it is equal to the main game window opened at the start
     * and housing all game contents
     */
    static TSharedPtr<class SWindow> GetMainWindow();
};
