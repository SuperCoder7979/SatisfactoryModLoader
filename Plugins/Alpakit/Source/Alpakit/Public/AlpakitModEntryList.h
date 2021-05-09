#pragma once
#include "Interfaces/IPluginManager.h"

/**
 * Widget that lists all plugins that might be able to get packed as mods
 */
class SAlpakitModEntryList : public SCompoundWidget {
SLATE_BEGIN_ARGS(SAlpakitModEntryList) {
        }

    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    /**
     * Updates the mod list.
     */
    void LoadMods();

    /**
     * Filters the mods for the given string
     */
    void Filter(const FString& InFilter);

    /**
     * Returns the string which was use previously to filter
     */
    FString GetLastFilter() const;

    /**
     * True if engine plugins are considered being mods
     */
    void SetShowEngine(bool bInShowEngine);

private:
    TSharedPtr<SListView<TSharedRef<IPlugin>>> ModList;
    TArray<TSharedRef<IPlugin>> Mods;
    TArray<TSharedRef<IPlugin>> FilteredMods;
    FString LastFilter;
    bool bShowEngine = false;

    FReply PackageAllMods();
};
