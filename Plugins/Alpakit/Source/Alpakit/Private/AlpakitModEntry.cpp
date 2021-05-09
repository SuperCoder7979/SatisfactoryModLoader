#include "AlpakitModEntry.h"
#include "Alpakit.h"
#include "AlpakitSettings.h"
#include "AlpakitStyle.h"
#include "Async/Async.h"
#include "UATHelper/Public/IUATHelperModule.h"

#define LOCTEXT_NAMESPACE "AlpakitModListEntry"

void SAlpakitModEntry::Construct(const FArguments& Args, TSharedRef<IPlugin> InMod, TSharedRef<SAlpakitModEntryList> InOwner) {
    Mod = InMod;

    UAlpakitSettings* Settings = UAlpakitSettings::Get();
    const FString PluginName = Mod->GetName();

    Checkbox = SNew(SCheckBox)
        .OnCheckStateChanged(this, &SAlpakitModEntry::OnEnableCheckboxChanged)
        .IsChecked(Settings->ModSelection.FindOrAdd(PluginName, false));

    ChildSlot[
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)[
            Checkbox.ToSharedRef()
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)[
            SNew(SButton)
            .Text(LOCTEXT("PackageModAlpakit", "Alpakit!"))
            .OnClicked_Lambda(
                             [this]() {
                                 PackageMod(TArray<TSharedPtr<SAlpakitModEntry>>());
                                 return FReply::Handled();
                             }
                         )
            .ToolTipText_Lambda(
                             [this]() {
                                 return FText::FromString(FString::Printf(TEXT("Alpakit %s"), *this->Mod->GetName()));
                             }
                         )
        ]
        + SHorizontalBox::Slot().FillWidth(1)[
            SNew(STextBlock)
            .Text_Lambda(
                                [InMod]() {
                                    const FString DisplayText = FString::Printf(TEXT("%s (%s)"), *InMod->GetDescriptor().FriendlyName, *InMod->GetName());
                                    return FText::FromString(DisplayText);
                                }
                            )
            .HighlightText_Lambda(
                                [InOwner]() {
                                    return FText::FromString(InOwner->GetLastFilter());
                                }
                            )
        ]
    ];
}

FString GetArgumentForLaunchType(EAlpakitStartGameType LaunchMode) {
    switch (LaunchMode) {
    case EAlpakitStartGameType::STEAM: return TEXT("-Steam");
    case EAlpakitStartGameType::EPIC_EARLY_ACCESS: return TEXT("-EpicEA");
    case EAlpakitStartGameType::EPIC_EXPERIMENTAL: return TEXT("-EpicExp");
    default: return TEXT("");
    }
}

FText GetCurrentPlatformName() {
#if PLATFORM_WINDOWS
    return LOCTEXT("PlatformName_Windows", "Windows");
#elif PLATFORM_MAC
    return LOCTEXT("PlatformName_Mac", "Mac");
#elif PLATFORM_LINUX
    return LOCTEXT("PlatformName_Linux", "Linux");
#else
    return LOCTEXT("PlatformName_Other", "Other OS");
#endif
}

void SAlpakitModEntry::PackageMod(const TArray<TSharedPtr<SAlpakitModEntry>>& NextEntries) const {
    UAlpakitSettings* Settings = UAlpakitSettings::Get();
    const FString PluginName = Mod->GetName();
    const FString GamePath = Settings->SatisfactoryGamePath.Path;

    const FString ProjectPath = FPaths::IsProjectFilePathSet()
                                    ? FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath())
                                    : FPaths::RootDir() / FApp::GetProjectName() / FApp::GetProjectName() + TEXT(".uproject");

    FString AdditionalUATArguments;
    if (Settings->bCopyModsToGame) {
        AdditionalUATArguments.Append(TEXT("-CopyToGameDir "));
    }
    if (Settings->LaunchGameAfterPacking != EAlpakitStartGameType::NONE && NextEntries.Num() == 0) {
        AdditionalUATArguments.Append(TEXT("-LaunchGame "));
        AdditionalUATArguments.Append(GetArgumentForLaunchType(Settings->LaunchGameAfterPacking)).Append(TEXT(" "));
    }

    const FString LaunchGameArgument = GetArgumentForLaunchType(Settings->LaunchGameAfterPacking);

    UE_LOG(LogAlpakit, Display, TEXT("Packaging plugin \"%s\". %d remaining"), *PluginName, NextEntries.Num());

    const FString CommandLine = FString::Printf(
        TEXT("-ScriptsForProject=\"%s\" PackagePlugin -Project=\"%s\" -PluginName=\"%s\" -GameDir=\"%s\" %s"),
        *ProjectPath,
        *ProjectPath,
        *PluginName,
        *Settings->SatisfactoryGamePath.Path,
        *AdditionalUATArguments
    );

    const FText PlatformName = GetCurrentPlatformName();
    IUATHelperModule::Get().CreateUatTask(
        CommandLine,
        PlatformName,
        LOCTEXT("PackageModTaskName", "Packaging Mod"),
        LOCTEXT("PackageModTaskShortName", "Package Mod Task"),
        FAlpakitStyle::Get().GetBrush("Alpakit.OpenPluginWindow"),
        NextEntries.Num() == 0
            ? (IUATHelperModule::UatTaskResultCallack)nullptr
            : [NextEntries](FString resultType, double runTime) {
                AsyncTask(
                    ENamedThreads::GameThread,
                    [NextEntries]() {
                        TSharedPtr<SAlpakitModEntry> NextMod = NextEntries[0];

                        TArray<TSharedPtr<SAlpakitModEntry>> RemainingEntries = NextEntries.FilterByPredicate(
                            [NextMod](const TSharedPtr<SAlpakitModEntry>& X) {
                                return X != NextMod;
                            }
                        );

                        NextMod->PackageMod(RemainingEntries);
                    }
                );
            }
    );
}

void SAlpakitModEntry::OnEnableCheckboxChanged(ECheckBoxState NewState) {
    // Save new checked state for the mod at the
    UAlpakitSettings* Settings = UAlpakitSettings::Get();
    const FString PluginName = Mod->GetName();

    Settings->ModSelection.Add(PluginName, NewState == ECheckBoxState::Checked);

    Settings->SaveSettings();
}

#undef LOCTEXT_NAMESPACE
