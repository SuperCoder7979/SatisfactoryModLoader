#pragma once
#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "ModContentRemapper.generated.h"

UCLASS()
class SML_API UModContentRemapper : public UEngineSubsystem {
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable)
    void RegisterPackageRedirect(const FString& OriginalPackage, const FString& NewPackage, bool bMatchSubstring);

    UFUNCTION(BlueprintCallable)
    void RegisterClassRedirect(const FString& OldClassName, const FString& NewClassName);

    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
private:
    /** List of plugins which already have redirects applied to them */
    TSet<FString> PluginsAlreadyHandled;

    void OnNewPluginMounted(class IPlugin& Plugin);
};
