#pragma once
#include "Configuration/Properties/ConfigPropertySection.h"
#include "CP_Section.generated.h"

UENUM(Blueprinttype)
enum class ECP_SectionWidgetType : uint8 {
    CPS_Horizontal UMETA(Displayname = "Horizontal List", Tooltip = "Nested Properties will be Listed in a Horizontal Layout"),
    CPS_Vertical UMETA(Displayname = "Vertical List", Tooltip = "Nested Properties will be Listed in a Vertical Layout"),
};

UCLASS(EditInlineNew)
class SML_API UCP_Section : public UConfigPropertySection {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Default", BlueprintReadOnly, meta = (DisplayAfter = "DefaultValue"))
    ECP_SectionWidgetType WidgetType;

    UPROPERTY(EditAnywhere, Category = "Default", BlueprintReadOnly, meta = (DisplayAfter = "DefaultValue"))
    bool HasHeader;
    UPROPERTY(EditAnywhere,Category= "Default", BlueprintReadOnly, meta = (DisplayAfter = "DefaultValue"))
    FText HeaderText;

};
