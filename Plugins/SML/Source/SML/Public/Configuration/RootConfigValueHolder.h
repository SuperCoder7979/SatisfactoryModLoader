#pragma once
#include "ConfigValueDirtyHandlerInterface.h"
#include "Configuration/ConfigManager.h"
#include "RootConfigValueHolder.generated.h"

UCLASS(NotBlueprintable)
class SML_API URootConfigValueHolder : public UObject, public IConfigValueDirtyHandlerInterface {
    GENERATED_BODY()

    // Begin IConfigValueDirtyHandlerInterface
    virtual void MarkDirty_Implementation() override;
    // End IConfigValueDirtyHandlerInterface

    FORCEINLINE UConfigPropertySection* GetWrappedValue() const { return RootWrappedValue; }
private:
    friend class UConfigManager;
    friend class URuntimeBlueprintFunctionLibrary;
    void SetupRootValue(UConfigManager* ConfigManager, const FConfigId& ConfigId);

    void UpdateWrappedValue(UConfigPropertySection* RootValueTemplate);

    FConfigId ConfigId;
    UPROPERTY()
    UConfigPropertySection* RootWrappedValue;
    UPROPERTY()
    UConfigManager* ConfigManager;
};
