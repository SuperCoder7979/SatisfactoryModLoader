#include "Patching/Patch/MainMenuPatch.h"
#include "Patching/BlueprintHookHelper.h"
#include "Patching/BlueprintHookManager.h"
#include "Reflection/ReflectionHelper.h"
#include "SatisfactoryModLoader.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "ModLoading/ModLoadingLibrary.h"

#define SML_MENU_PAGE_ASSET_PATH TEXT("/SML/ModList/Widget_ModList.Widget_ModList_C")

UWidget* CreateModSubMenuWidget(UUserWidget* OwningWidget) {
    UClass* MenuBaseClass = LoadObject<UClass>(NULL, SML_MENU_PAGE_ASSET_PATH);
    if (MenuBaseClass == NULL) {
        // Class is not accessible. It is definitely an error, but we can at least recover from it
        UE_LOG(LogSatisfactoryModLoader, Error, TEXT("Failed to load SML menu page asset from path %s"), SML_MENU_PAGE_ASSET_PATH);
        return NULL;
    }
    UUserWidget* NewWidget = UUserWidget::CreateWidgetInstance(*OwningWidget, MenuBaseClass, TEXT("ModListSubMenu"));
    FReflectionHelper::SetPropertyValue<FObjectProperty>(NewWidget, TEXT("mOwningMenu"), OwningWidget);
    return NewWidget;
}

TArray<FString> FMainMenuPatch::CreateMenuInformationText() {
    UModLoadingLibrary* ModLoadingLibrary = GEngine->GetEngineSubsystem<UModLoadingLibrary>();
    const int32 ModsLoaded = ModLoadingLibrary->GetLoadedMods().Num();
    TArray<FString> ResultText;

    ResultText.Add(FString::Printf(TEXT("Satisfactory Mod Loader v.%s"), *FSatisfactoryModLoader::GetModLoaderVersion().ToString()));
    ResultText.Add(FString::Printf(TEXT("%lu mod(s) loaded"), ModsLoaded));

    if (FSatisfactoryModLoader::GetSMLConfiguration().bDevelopmentMode) {
        ResultText.Add(TEXT("Development mode enabled."));
    }
    return ResultText;
}

void FMainMenuPatch::ApplyMainMenuPatch(UUserWidget* MenuWidget) {
    UTextBlock* UsernameLabel = FReflectionHelper::GetObjectPropertyValue<UTextBlock>(MenuWidget, TEXT("mEpicUsernameLabel"));
    checkf(UsernameLabel, TEXT("mEpicUsernameLabel not found"));
    UVerticalBox* ParentVerticalBox = Cast<UVerticalBox>(UsernameLabel->GetParent());
    checkf(ParentVerticalBox, TEXT("mEpicUsernameLabel parent is not a vertical box"));

    UUserWidget* OptionsButton = FReflectionHelper::GetObjectPropertyValue<UUserWidget>(MenuWidget, TEXT("mButtonOptions"));
    UPanelWidget* SwitcherWidget = FReflectionHelper::GetObjectPropertyValue<UPanelWidget>(MenuWidget, TEXT("mSwitcher"));
    checkf(OptionsButton, TEXT("OptionsButton not found"));
    checkf(SwitcherWidget, TEXT("Switcher Widget not found"));

    UWidget* ModSubMenuWidget = CreateModSubMenuWidget(MenuWidget);
    if (ModSubMenuWidget != NULL) {
        SwitcherWidget->AddChild(ModSubMenuWidget);
    }

    UPanelWidget* ParentPanel = FReflectionHelper::GetObjectPropertyValue<UPanelWidget>(MenuWidget, TEXT("mMainMenuList"));

    UClass* FrontEndButtonClass = LoadObject<UClass>(NULL, TEXT("/Game/FactoryGame/Interface/UI/Menu/Widget_FrontEnd_Button.Widget_FrontEnd_Button_C"));
    checkf(FrontEndButtonClass, TEXT("FrontEndButton class not found"));
    UUserWidget* ButtonWidget = UUserWidget::CreateWidgetInstance(*MenuWidget, FrontEndButtonClass, TEXT("mModListButton"));
    FText ButtonText = FText::FromString(TEXT("Mods"));
    FReflectionHelper::CallScriptFunction(ButtonWidget, TEXT("SetTitle"), &ButtonText);

    if (ModSubMenuWidget != NULL) {
        FReflectionHelper::SetPropertyValue<FObjectProperty>(ButtonWidget, TEXT("mSwitcherWidget"), SwitcherWidget);
        FReflectionHelper::SetPropertyValue<FObjectProperty>(ButtonWidget, TEXT("mTargetWidget"), ModSubMenuWidget);
        FReflectionHelper::SetPropertyValue<FObjectProperty>(ButtonWidget, TEXT("mParentList"), ParentPanel);
    }
    ParentPanel->AddChild(ButtonWidget);

    UTextBlock* SMLTextBlock = NewObject<UTextBlock>(ParentVerticalBox);
    SMLTextBlock->Text = FText::FromString(FString::Join(CreateMenuInformationText(), TEXT("\n")));
    SMLTextBlock->SetFont(UsernameLabel->Font);
    SMLTextBlock->SetColorAndOpacity(UsernameLabel->ColorAndOpacity);
    ParentVerticalBox->AddChild(SMLTextBlock);
}

void FMainMenuPatch::RegisterPatch() {
    UClass* MenuWidgetClass = LoadObject<UClass>(NULL, TEXT("/Game/FactoryGame/Interface/UI/Menu/MainMenu/BP_MainMenuWidget.BP_MainMenuWidget_C"));
    checkf(MenuWidgetClass, TEXT("MainMenuWidget blueprint not found"));
    UFunction* ConstructFunction = MenuWidgetClass->FindFunctionByName(TEXT("Construct"));

    UBlueprintHookManager* HookManager = GEngine->GetEngineSubsystem<UBlueprintHookManager>();
    HookManager->HookBlueprintFunction(
        ConstructFunction,
        [](FBlueprintHookHelper& HookHelper) {
            FMainMenuPatch::ApplyMainMenuPatch(CastChecked<UUserWidget>(HookHelper.GetContext()));
        },
        EPredefinedHookOffset::Return
    );
}
