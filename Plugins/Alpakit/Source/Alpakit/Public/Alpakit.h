#pragma once
#include "CoreMinimal.h"
#include "ContentBrowserDelegates.h"
#include "AlpakitSettings.h"
#include "AssetData.h"
#include "Dom/JsonObject.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAlpakit, Verbose, All);

class FAlpakitModule : public IModuleInterface {
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
private:
    TSharedPtr<class FUICommandList> PluginCommands;

    void RegisterSettings() const;
    void UnregisterSettings() const;
};
