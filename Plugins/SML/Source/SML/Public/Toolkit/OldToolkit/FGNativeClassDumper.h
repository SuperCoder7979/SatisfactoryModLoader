#pragma once
#include "Logging/LogMacros.h"
#include "Containers/UnrealString.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNativeClassDumper, Log, All);

class SML_API FGameNativeClassDumper {
private:
    friend class FSatisfactoryModLoader;

    /** Registers Unreal console commands */
    static void RegisterConsoleCommands();
};
