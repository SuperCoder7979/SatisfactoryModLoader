#pragma once

#include "AlpakitModEntryList.h"
#include "Slate.h"

class SAlpakitWidget : public SCompoundWidget {
public:
SLATE_BEGIN_ARGS(SAlpakitWidget) {
        }

    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    TSharedPtr<IDetailsView> DetailsView;
    TSharedPtr<SAlpakitModEntryList> ModList;
};
