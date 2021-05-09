#pragma once
#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FAlpakitCommands : public TCommands<FAlpakitCommands> {
public:
    FAlpakitCommands();

    virtual void RegisterCommands() override;
public:
    TSharedPtr<FUICommandInfo> OpenPluginWindow;
};
