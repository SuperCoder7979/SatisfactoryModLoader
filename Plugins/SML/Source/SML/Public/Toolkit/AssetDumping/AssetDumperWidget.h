#pragma once
#include "AssetDumpProcessor.h"
#include "Slate.h"

class SAssetDumperWidget : public SCompoundWidget {
public:
SLATE_BEGIN_ARGS(SAssetDumperWidget) {
        }

    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
protected:
    TMap<FName, bool> AssetClassSerializationRules;
    TMap<FName, TArray<FName>> AdditionalAssetTypes;

    TSharedPtr<class SAssetRegistryViewWidget> AssetRegistryViewWidget;
    TSharedPtr<class SButton> AssetDumpButton;
    TSharedPtr<class SEditableTextBox> OutputPathText;
    FAssetDumpSettings AssetDumpSettings;

    TSharedRef<SHorizontalBox> CreateAssetTypesTab();
    TSharedRef<SVerticalBox> CreateSettingsCategory();
    FReply OnBrowseOutputPathPressed();
    FReply OnAssetDumpButtonPressed();
};
