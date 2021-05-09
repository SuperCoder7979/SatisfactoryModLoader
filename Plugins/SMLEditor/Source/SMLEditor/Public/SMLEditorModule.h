#pragma once
#include "Modules/ModuleManager.h"

class FSMLEditorModule : public FDefaultGameModuleImpl {
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
