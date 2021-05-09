#include "AlpakitCommands.h"
#include "AlpakitStyle.h"

#define LOCTEXT_NAMESPACE "FAlpakitModule"

FAlpakitCommands::FAlpakitCommands() : TCommands<FAlpakitCommands>(
    TEXT("Alpakit"),
    NSLOCTEXT("Contexts", "Alpakit", "Alpakit Plugin"),
    NAME_None,
    FAlpakitStyle::GetStyleSetName()
) {
}

void FAlpakitCommands::RegisterCommands() {
    UI_COMMAND(OpenPluginWindow, "Alpakit", "Open the alpakit window", EUserInterfaceActionType::Button, FInputChord{});
}

#undef LOCTEXT_NAMESPACE
