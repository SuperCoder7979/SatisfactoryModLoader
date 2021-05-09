#include "SMLEditorModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "CodeGeneration/NativeCodeGenerator.h"
#include "CodeGeneration/UserDefinedStructCodeGenerator.h"
#include "Configuration/ModConfiguration.h"
#define LOCTEXT_NAMESPACE "SML"

bool ContainsAtLeastOneModConfigBlueprint(TArray<FAssetData> SelectedAssets) {
    for (const FAssetData& AssetData : SelectedAssets) {
        UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
        if (Blueprint && Blueprint->GeneratedClass &&
            Blueprint->GeneratedClass->IsChildOf<UModConfiguration>()) {
            return true;
        }
    }
    return false;
}

void GenerateStructsForSelectedBlueprints(const TArray<FAssetData> SelectedAssets, bool bGenerateNativeStruct) {
    for (const FAssetData& AssetData : SelectedAssets) {
        UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
        if (Blueprint && Blueprint->GeneratedClass &&
            Blueprint->GeneratedClass->IsChildOf<UModConfiguration>()) {
            if (bGenerateNativeStruct) {
                FNativeCodeGenerator::GenerateConfigStructForConfigurationAsset(Blueprint);
            } else {
                FUserDefinedStructCodeGenerator::GenerateConfigStructForConfigurationAsset(Blueprint);
            }
        }
    }
}

void PopulateModConfigBlueprintAssetActionsMenu(FMenuBuilder& MenuBuilder, const TArray<FAssetData>& SelectedAssets) {
    MenuBuilder.AddMenuEntry(
        LOCTEXT("GenerateBlueprintStruct", "Regenerate Config Struct"),
        LOCTEXT("GenerateBlueprintStructTooltip", "Regenerates User-Defined Struct from the Configuration layout"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(GenerateStructsForSelectedBlueprints, SelectedAssets, false), EUIActionRepeatMode::RepeatEnabled)
    );

    MenuBuilder.AddMenuEntry(
        LOCTEXT("GenerateNativeBlueprintStruct", "Generate C++ Config Struct Header"),
        LOCTEXT("GenerateNativeBlueprintStructTooltip", "Generates a C++ header containing struct definitions generated from the Configuration layout"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(GenerateStructsForSelectedBlueprints, SelectedAssets, true), EUIActionRepeatMode::RepeatEnabled)
    );
}

void PopulateBlueprintAssetActionsMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets) {
    if (ContainsAtLeastOneModConfigBlueprint(SelectedAssets)) {
        PopulateModConfigBlueprintAssetActionsMenu(MenuBuilder, SelectedAssets);
    }
}

TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets) {
    TSharedRef<FExtender> Extender(new FExtender());

    // Only add additional handlers if we're dealing with blueprints
    bool bHaveAnyBlueprints = false;
    for (const FAssetData& AssetData : SelectedAssets) {
        bHaveAnyBlueprints |= AssetData.AssetClass == UBlueprint::StaticClass()->GetFName();
    }

    if (bHaveAnyBlueprints) {
        Extender->AddMenuExtension(
            "GetAssetActions",
            EExtensionHook::After,
            nullptr,
            FMenuExtensionDelegate::CreateStatic(&PopulateBlueprintAssetActionsMenu, SelectedAssets)
        );
    }

    return Extender;
}

void FSMLEditorModule::StartupModule() {
    FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
    CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&OnExtendContentBrowserAssetSelectionMenu));
}

void FSMLEditorModule::ShutdownModule() {
}

IMPLEMENT_GAME_MODULE(FSMLEditorModule, SMLEditor);
