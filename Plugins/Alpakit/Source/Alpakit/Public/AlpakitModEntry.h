#pragma once
#include "AlpakitModEntryList.h"
#include "Interfaces/IPluginManager.h"

/**
 * Entry of a mod within the mod list of the alpakit window
 */
class SAlpakitModEntry : public SCompoundWidget {
SLATE_BEGIN_ARGS(SAlpakitModEntry) {
        }

    SLATE_END_ARGS()

    void Construct(const FArguments& Args, TSharedRef<IPlugin> InMod, TSharedRef<SAlpakitModEntryList> InOwner);

    void PackageMod(const TArray<TSharedPtr<SAlpakitModEntry>>& NextEntries) const;
    void OnEnableCheckboxChanged(ECheckBoxState NewState);

    FORCEINLINE bool IsSelected() {
        return Checkbox && Checkbox->IsChecked();
    }

private:
    TSharedPtr<IPlugin> Mod;
    TSharedPtr<SAlpakitModEntryList> Owner;
    TSharedPtr<class SCheckBox> Checkbox;
};
