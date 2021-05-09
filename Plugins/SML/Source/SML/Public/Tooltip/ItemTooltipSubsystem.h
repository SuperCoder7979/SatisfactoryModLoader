#pragma once
#include "FGInventoryComponent.h"
#include "Internationalization/Text.h"
#include "Components/VerticalBox.h"
#include "Components/Widget.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ItemTooltipSubsystem.generated.h"

UCLASS()
class SML_API UItemTooltipSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    /**
     * Register tooltip provider that will be called for all items registered
     * Please be careful with implementation as it will be called very often
     * Object should implement ISMLItemTooltipProvider
     */
    UFUNCTION(BlueprintCallable)
    void RegisterGlobalTooltipProvider(const FString& ModReference, UObject* ItemTooltipProvider);

    /**
     * Returns formatted item name obtained from InventoryStack
     * For normal items it just returns UFGItemDescriptor::GetItemDescription
     * For items implementing ISMLItemDisplayInterface, this is routed to GetItemName
     */
    UFUNCTION(BlueprintPure)
    FText GetItemName(APlayerController* OwningPlayer, const FInventoryStack& InventoryStack);

    /**
     * Retrieves correct item description for given inventory stack
     * Call semantics are similar to GetItemName()
     */
    UFUNCTION(BlueprintPure)
    FText GetItemDescription(APlayerController* OwningPlayer, const FInventoryStack& InventoryStack);

    TArray<UWidget*> CreateDescriptionWidgets(APlayerController* OwningPlayer, const FInventoryStack& InventoryStack);
private:
    friend class FSatisfactoryModLoader;

    void ApplyItemOverridesToTooltip(UWidget* TooltipWidget, APlayerController* OwningPlayer, const FInventoryStack& InventoryStack);

    static void InitializePatches();

    /** Array of registered tooltip providers, UPROPERTY to avoid garbage collection */
    UPROPERTY()
    TArray<UObject*> GlobalTooltipProviders;
};
