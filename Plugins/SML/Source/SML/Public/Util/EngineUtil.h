#pragma once
#include "CoreMinimal.h"

class SML_API FEngineUtil {
public:
    /**
     * Returns global timer manager for the active engine instance
     * This should never return NULL except in very early setup stages
     *
     * In Editor, it will return global editor timer manager (ticked by engine)
     * In Game, it will return global game instance timer manager (ticked by currently loaded world)
     */
    static class FTimerManager* GetGlobalTimerManager();

    /** Dispatches passed delegate when timer manager instance is ready */
    static void DispatchWhenTimerManagerIsReady(const TBaseDelegate<void, FTimerManager*>& Delegate);
};
