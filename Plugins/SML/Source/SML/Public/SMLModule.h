#pragma once
#include "Modules/ModuleManager.h"

extern "C" DLLEXPORT IModuleInterface* InitializeModule();

class SML_API FSMLModule : public FDefaultGameModuleImpl {
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
