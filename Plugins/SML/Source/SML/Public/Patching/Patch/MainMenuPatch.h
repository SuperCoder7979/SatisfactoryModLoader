#pragma once
#include "CoreMinimal.h"

class SML_API FMainMenuPatch {
public:
    static TArray<FString> CreateMenuInformationText();
    static void ApplyMainMenuPatch(class UUserWidget* MainMenuWidget);
private:
    friend class FSatisfactoryModLoader;

    static void RegisterPatch();
};
