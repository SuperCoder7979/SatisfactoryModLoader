#pragma once
#include "CoreMinimal.h"
#include "Module/WorldModule.h"
#include "GameWorldModule.generated.h"

/** World module initialized for normal game world only (e.g skipped for menu world) */
UCLASS(Blueprintable)
class SML_API UGameWorldModule : public UWorldModule {
    GENERATED_BODY()
public:
    /**
    * List of schematics that will be automatically registered
    * by the SML during the loading phase
    */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<TSubclassOf<class UFGSchematic>> mSchematics;

    /**
    * List of MAM research trees that will be registered
    * by the SML during the loading phase
    */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<TSubclassOf<class UFGResearchTree>> mResearchTrees;

    /**
    * List of chat commands to register automatically on startup
    */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<TSubclassOf<class AChatCommandInstance>> mChatCommands;

    /**
    * Table to use for obtaining AWESOME resource sink item point values
    * for items added by this mod
    */
    UPROPERTY(EditDefaultsOnly, Category = Advanced)
    TSoftObjectPtr<class UDataTable> mResourceSinkItemPointsTable;

    /** Mod subsystem actors to be registered automatically during construction phase */
    UPROPERTY(EditDefaultsOnly, Category= Advanced)
    TArray<TSubclassOf<class AModSubsystem>> ModSubsystems;

    /** Register content from properties here */
    virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
protected:
    /** Registers content at the construction phase */
    void RegisterConstructionPhaseContent();

    /** Registers default content from properties specified above */
    void RegisterDefaultContent();
};
