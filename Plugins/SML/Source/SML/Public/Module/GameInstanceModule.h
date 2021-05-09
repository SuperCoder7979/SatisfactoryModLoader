#pragma once
#include "Module/ModModule.h"
#include "GameFramework/PlayerInput.h"
#include "Templates/SubclassOf.h"
#include "Engine/GameInstance.h"

#include "GameInstanceModule.generated.h"

class UModSubsystemHolder;
class UModConfiguration;

/** Holds information about individual key binding registration */
USTRUCT(BlueprintType)
struct SML_API FModKeyBindingInfo {
    GENERATED_BODY()

    /** Name of the action this key bindings corresponds to. Should be prefixed with ModReference. */
    UPROPERTY(EditAnywhere)
    FName ActionName;

    /** Information about key binding being registered */
    UPROPERTY(EditAnywhere)
    FInputActionKeyMapping KeyMapping;

    /** Display name of Key Binding in Options|Controls menu */
    UPROPERTY(EditAnywhere)
    FText DisplayName;
};

/** Holds information about individual axis binding registration */
USTRUCT(BlueprintType)
struct SML_API FModAxisBindingInfo {
    GENERATED_BODY()

    /** Name of the axis this key bindings corresponds to. Should be prefixed with ModReference. */
    UPROPERTY(EditAnywhere)
    FName AxisName;

    /** Information about Positive (Scale > 0) axis mapping. AxisName should be the same and prefixed with ModReference. */
    UPROPERTY(EditAnywhere)
    FInputAxisKeyMapping PositiveAxisMapping;
    /** Information about Negative (Scale < 0) axis mapping. AxisName should be the same and prefixed with ModReference. */
    UPROPERTY(EditAnywhere)
    FInputAxisKeyMapping NegativeAxisMapping;

    /** Display name of *Positive* Axis Binding in Options|Controls menu */
    UPROPERTY(EditAnywhere)
    FText PositiveAxisDisplayName;
    /** Display name of *Negative* Axis Binding in Options|Controls menu */
    UPROPERTY(EditAnywhere)
    FText NegativeAxisDisplayName;
};

/** Describes module loaded with game instance and active as long as game instance is loaded */
UCLASS(Blueprintable)
class SML_API UGameInstanceModule : public UModModule {
    GENERATED_BODY()
public:
    /** Returns game instance this module is attached to */
    UFUNCTION(BlueprintPure)
    UGameInstance* GetGameInstance() const;

    /** Configurations defined and used by this mod */
    UPROPERTY(EditDefaultsOnly, Category = Default)
    TArray<TSubclassOf<UModConfiguration>> ModConfigurations;

    /** Key Bindings for this mod to be registered */
    UPROPERTY(EditDefaultsOnly, Category = Default)
    TArray<FModKeyBindingInfo> ModKeyBindings;

    /**
    * List of classes for objects implementing ISMLItemTooltipProvider
    * These will be registered on startup and used to obtain additional description
    * text/widget for all items
    */
    UPROPERTY(EditDefaultsOnly, Category = Advanced)
    TArray<UClass*> GlobalItemTooltipProviders;

    /**
    * Axis Bindings for this mod to be registered
    * They are very similar to key bindings in behavior, but there are some differences:
    *  - Axis Bindings provide /degrees of input/ and not discrete 0/1 values
    *  - Axis Bindings are continuously polled and not triggered on action (so they are good for movement)
    *  So use them for movement basically
    */
    UPROPERTY(EditDefaultsOnly, Category = Advanced)
    TArray<FModAxisBindingInfo> ModAxisBindings;

    /** Game instance modules can access world context from game instance */
    virtual UWorld* GetWorld() const override;

    /** Register content from properties here */
    virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
protected:
    /** Allow SetOwnerModReference access to game instance module manager */
    friend class UGameInstanceModuleManager;

    /** Registers default content from properties specified above */
    void RegisterDefaultContent();
};
