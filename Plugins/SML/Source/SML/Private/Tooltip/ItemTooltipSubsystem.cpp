#include "Tooltip/ItemTooltipSubsystem.h"
#include "Patching/BlueprintHookHelper.h"
#include "Patching/BlueprintHookManager.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Tooltip/ItemStackContextWidget.h"
#include "Tooltip/SMLItemDisplayInterface.h"
#include "Tooltip/SMLItemTooltipProvider.h"

// Overwrites delegates bound to title & description widgets to use FTooltipHookHelper, add custom item widget
void UItemTooltipSubsystem::ApplyItemOverridesToTooltip(UWidget* TooltipWidget, APlayerController* OwningPlayer, const FInventoryStack& InventoryStack) {
    // Gather UProperty exposed by tooltip widget
    UClass* TooltipWidgetClass = TooltipWidget->GetClass();
    UObjectProperty* TitleWidgetProperty = Cast<UObjectProperty>(TooltipWidgetClass->FindPropertyByName(TEXT("mTitle")));
    UObjectProperty* DescriptionWidgetProperty = Cast<UObjectProperty>(TooltipWidgetClass->FindPropertyByName(TEXT("mDescription")));
    check(TitleWidgetProperty && DescriptionWidgetProperty);

    // Retrieve references to some stuff
    UTextBlock* NameBlock = Cast<UTextBlock>(TitleWidgetProperty->GetObjectPropertyValue_InContainer(TooltipWidget));
    UTextBlock* DescriptionBlock = Cast<UTextBlock>(DescriptionWidgetProperty->GetObjectPropertyValue_InContainer(TooltipWidget));
    // Retrieve parent panel, it will hold name, description and recipe blocks
    UPanelWidget* ParentPanel = NameBlock->GetParent();

    // Spawn custom widget in parent panel and add it
    UItemStackContextWidget* ContextWidget = NewObject<UItemStackContextWidget>(ParentPanel);
    ContextWidget->ItemTooltipSubsystem = this;
    ContextWidget->InventoryStack = InventoryStack;
    ContextWidget->PlayerController = OwningPlayer;
    ContextWidget->Visibility = ESlateVisibility::Collapsed;
    ParentPanel->AddChild(ContextWidget);
    // Rebind text delegates to custom widget
    NameBlock->TextDelegate.BindUFunction(ContextWidget, TEXT("GetItemName"));
    DescriptionBlock->TextDelegate.BindUFunction(ContextWidget, TEXT("GetItemDescription"));

    // Append custom widgets to description
    TArray<UWidget*> Widgets = CreateDescriptionWidgets(OwningPlayer, InventoryStack);
    for (UWidget* Widget : Widgets) {
        ParentPanel->AddChild(Widget);
    }
}

FInventoryStack GetStackFromSlot(UObject* SlotWidget) {
    // Retrieve fields relevant to owner inventory
    UObjectProperty* InventoryProperty = Cast<UObjectProperty>(SlotWidget->GetClass()->FindPropertyByName(TEXT("mCachedInventoryComponent")));
    UIntProperty* SlotIndexProperty = Cast<UIntProperty>(SlotWidget->GetClass()->FindPropertyByName(TEXT("mSlotIdx")));
    check(InventoryProperty && SlotIndexProperty);

    FInventoryStack ResultStack{};
    // Access inventory if it's not a null pointer
    UFGInventoryComponent* InventoryComponent = Cast<UFGInventoryComponent>(InventoryProperty->GetObjectPropertyValue_InContainer(SlotWidget));
    if (InventoryComponent) {
        const int32 SlotIndex = SlotIndexProperty->GetPropertyValue_InContainer(SlotWidget);
        InventoryComponent->GetStackFromIndex(SlotIndex, ResultStack);
    }
    return ResultStack;
}

void UItemTooltipSubsystem::InitializePatches() {
    // Hook into InventorySlot widget to apply tooltip overrides
    UClass* InventorySlot = LoadObject<UClass>(NULL, TEXT("/Game/FactoryGame/Interface/UI/InGame/InventorySlots/Widget_InventorySlot.Widget_InventorySlot_C"));
    check(InventorySlot);
    UFunction* Function = InventorySlot->FindFunctionByName(TEXT("GetTooltipWidget"));

    UBlueprintHookManager* HookManager = GEngine->GetEngineSubsystem<UBlueprintHookManager>();
    HookManager->HookBlueprintFunction(
        Function,
        [](FBlueprintHookHelper& HookHelper) {
            UUserWidget* TooltipWidget = Cast<UUserWidget>(*HookHelper.GetOutVariablePtr<UObjectProperty>());
            UUserWidget* SlotWidget = Cast<UUserWidget>(HookHelper.GetContext());

            if (TooltipWidget != nullptr) {
                APlayerController* OwningPlayer = SlotWidget->GetOwningPlayer();
                const FInventoryStack InventoryStack = GetStackFromSlot(SlotWidget);

                if (InventoryStack.Item.IsValid()) {
                    UGameInstance* GameInstance = SlotWidget->GetWorld()->GetGameInstance();
                    UItemTooltipSubsystem* TooltipSubsystem = GameInstance->GetSubsystem<UItemTooltipSubsystem>();
                    TooltipSubsystem->ApplyItemOverridesToTooltip(TooltipWidget, OwningPlayer, InventoryStack);
                }
            }
        },
        EPredefinedHookOffset::Return
    );
}

void UItemTooltipSubsystem::RegisterGlobalTooltipProvider(const FString& ModReference, UObject* ItemTooltipProvider) {
    if (ItemTooltipProvider->Implements<USMLItemTooltipProvider>()) {
        GlobalTooltipProviders.AddUnique(ItemTooltipProvider);
    }
}

FText UItemTooltipSubsystem::GetItemName(APlayerController* OwningPlayer, const FInventoryStack& InventoryStack) {
    UClass* ItemClass = InventoryStack.Item.ItemClass;
    if (ItemClass != NULL) {
        UObject* ItemObject = ItemClass->GetDefaultObject();
        if (ItemObject->Implements<USMLItemDisplayInterface>()) {
            return ISMLItemDisplayInterface::Execute_GetOverridenItemName(ItemObject, OwningPlayer, InventoryStack);
        }
    }
    return UFGItemDescriptor::GetItemName(ItemClass);
}

FText UItemTooltipSubsystem::GetItemDescription(APlayerController* OwningPlayer, const FInventoryStack& InventoryStack) {
    UClass* ItemClass = InventoryStack.Item.ItemClass;
    TArray<FString> DescriptionText;

    const FString VanillaDescription = UFGItemDescriptor::GetItemDescription(ItemClass).ToString();
    if (!VanillaDescription.IsEmpty()) {
        DescriptionText.Add(VanillaDescription);
    }

    if (ItemClass != NULL) {
        UObject* ItemObject = ItemClass->GetDefaultObject();
        if (ItemObject->Implements<USMLItemDisplayInterface>()) {
            const FString ItemModdedDescription = ISMLItemDisplayInterface::Execute_GetOverridenItemDescription(ItemObject, OwningPlayer, InventoryStack).ToString();
            if (!ItemModdedDescription.IsEmpty()) {
                DescriptionText.Add(ItemModdedDescription);
            }
        }
    }

    for (UObject* GlobalTooltipProvider : GlobalTooltipProviders) {
        const FString GlobalItemDescription = ISMLItemTooltipProvider::Execute_GetItemDescription(GlobalTooltipProvider, OwningPlayer, InventoryStack).ToString();
        if (!GlobalItemDescription.IsEmpty()) {
            DescriptionText.Add(GlobalItemDescription);
        }
    }

    return FText::FromString(FString::Join(DescriptionText, TEXT("\n")));
}

TArray<UWidget*> UItemTooltipSubsystem::CreateDescriptionWidgets(APlayerController* OwningPlayer, const FInventoryStack& InventoryStack) {
    TArray<UWidget*> ResultWidgets;
    UClass* ItemClass = InventoryStack.Item.ItemClass;

    if (ItemClass != NULL) {
        UObject* ItemObject = ItemClass->GetDefaultObject();
        if (ItemObject->Implements<USMLItemDisplayInterface>()) {
            UWidget* NewWidget = ISMLItemDisplayInterface::Execute_CreateDescriptionWidget(ItemObject, OwningPlayer, InventoryStack);
            if (NewWidget) {
                ResultWidgets.Add(NewWidget);
            }
        }
    }

    for (UObject* GlobalTooltipProvider : GlobalTooltipProviders) {
        UWidget* ProviderWidget = ISMLItemTooltipProvider::Execute_CreateDescriptionWidget(GlobalTooltipProvider, OwningPlayer, InventoryStack);
        if (ProviderWidget) {
            ResultWidgets.Add(ProviderWidget);
        }
    }

    return ResultWidgets;
}
