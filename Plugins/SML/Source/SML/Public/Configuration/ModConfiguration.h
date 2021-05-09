#pragma once
#include "Configuration/Properties/ConfigPropertySection.h"
#include "ModConfiguration.generated.h"

/** Describes an identifier for a single configuration tree */
USTRUCT(BlueprintType)
struct SML_API FConfigId {
    GENERATED_BODY()
    ;
public:
    /** Mod reference of the requested configuration owner */
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString ModReference;

    /** Category if this configuration. Leave empty if mod has only one configuration */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced")
    FString ConfigCategory;

    bool operator==(const FConfigId& ConfigId) const;
};

FORCEINLINE uint32 GetTypeHash(const FConfigId& ConfigId) {
    return HashCombine(GetTypeHash(ConfigId.ModReference), GetTypeHash(ConfigId.ConfigCategory));
}

FORCEINLINE bool FConfigId::operator==(const FConfigId& ConfigId) const {
    return ConfigId.ModReference == ModReference && ConfigId.ConfigCategory == ConfigCategory;
}

UCLASS(Abstract, Blueprintable)
class SML_API UModConfiguration : public UObject {
    GENERATED_BODY()
public:
    /** Id for configuration described by this class */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FConfigId ConfigId;

    /** Display name of this configuration, as it will be visible to user */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FString DisplayName;

    /** Description of this configuration, can be empty */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FString Description;

    /** Root property of this configuration describing it's values */
    UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly)
    UConfigPropertySection* RootSection;

    /** Custom Widget - placed at the Bottom of the Mod Config Widgets*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TSubclassOf<UUserWidget> CustomWidget;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
#endif
};
