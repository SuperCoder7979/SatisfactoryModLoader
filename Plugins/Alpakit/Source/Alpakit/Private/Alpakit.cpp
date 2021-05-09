#include "Alpakit.h"
#include "AlpakitStyle.h"
#include "AlpakitCommands.h"
#include "AlpakitWidget.h"
#include "AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "ISettingsContainer.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "LevelEditor.h"

static const FName AlpakitTabName("Alpakit");

#define LOCTEXT_NAMESPACE "FAlpakitModule"

DEFINE_LOG_CATEGORY(LogAlpakit)

void FAlpakitModule::StartupModule() {
    // Register editor settings
    RegisterSettings();

    // Initialize Slate stuff, including commands
    FAlpakitStyle::Initialize();
    FAlpakitStyle::ReloadTextures();
    FAlpakitCommands::Register();

    PluginCommands = MakeShareable(new FUICommandList);
    PluginCommands->MapAction(
        FAlpakitCommands::Get().OpenPluginWindow,
        FExecuteAction::CreateLambda(
            []() {
                FGlobalTabmanager::Get()->InvokeTab(AlpakitTabName);
            }
        ),
        FCanExecuteAction()
    );
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

    // Register Alpakit Settings in Editor's Project Menu
    TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
    MenuExtender->AddMenuExtension(
        TEXT("FileProject"),
        EExtensionHook::After,
        PluginCommands,
        FMenuExtensionDelegate::CreateLambda(
            [](FMenuBuilder& Builder) {
                Builder.AddMenuEntry(FAlpakitCommands::Get().OpenPluginWindow);
            }
        )
    );
    LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);

    // Register Alpakit Settings in Editor's Toolbar
    TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
    ToolbarExtender->AddToolBarExtension(
        TEXT("Settings"),
        EExtensionHook::After,
        PluginCommands,
        FToolBarExtensionDelegate::CreateLambda(
            [](FToolBarBuilder& Builder) {
                Builder.AddToolBarButton(FAlpakitCommands::Get().OpenPluginWindow);
            }
        )
    );
    LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);

    // Register normal alpakit settings tab spawner
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
                                AlpakitTabName,
                                FOnSpawnTab::CreateLambda(
                                    [](const FSpawnTabArgs&) {
                                        const TSharedRef<SAlpakitWidget> AlpakitWidget = SNew(SAlpakitWidget);
                                        return SNew(SDockTab)
                                            .TabRole(NomadTab)
                                            // TODO: Save settings when tab is closed automatically
                                            // .OnTabClosed_Lambda([AlpakitWidget](TSharedRef<SDockTab>) { AlpakitWidget->SaveSettings(); })
                                            [AlpakitWidget];
                                    }
                                )
                            )
                            .SetDisplayName(LOCTEXT("FAlpakitTabTitle", "Alpakit"))
                            .SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FAlpakitModule::ShutdownModule() {
    if (UObjectInitialized()) {
        UnregisterSettings();
    }

    FAlpakitStyle::Shutdown();
    FAlpakitCommands::Unregister();

    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AlpakitTabName);
}

void FAlpakitModule::RegisterSettings() const {
    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings"))) {
        // Create the new category
        ISettingsContainerPtr SettingsContainer = SettingsModule->GetContainer(TEXT("Project"));
        SettingsContainer->DescribeCategory(
            TEXT("Alpakit Settings"),
            LOCTEXT("RuntimeWDCategoryName", "Alpakit Settings"),
            LOCTEXT("RuntimeWDCategoryDescription", "Alpakit Settings. Can also be edited in the Alpakit menu")
        );

        // Register the settings
        ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings(
            TEXT("Project"),
            TEXT("Alpakit"),
            TEXT("General"),
            LOCTEXT("RuntimeGeneralSettingsName", "General"),
            LOCTEXT("RuntimeGeneralSettingsDescription", "Alpakit Settings. Can also be edited in the Alpakit menu"),
            UAlpakitSettings::Get()
        );
        // Register the save handler to your settings, you might want to use it to
        // validate those or just act to settings changes.
        if (SettingsSection.IsValid()) {
            SettingsSection->OnModified().BindLambda(
                []() {
                    UAlpakitSettings::Get()->SaveSettings();
                    return true;
                }
            );
        }
    }
}

void FAlpakitModule::UnregisterSettings() const {
    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings"))) {
        SettingsModule->UnregisterSettings(TEXT("Project"), TEXT("Alpakit"), TEXT("General"));
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAlpakitModule, Alpakit)
