#pragma once
#include "Configuration/Properties/ConfigPropertyArray.h"
#include "CP_Array.generated.h"

UENUM(Blueprinttype)
enum class ECP_ArrayWidgetType : uint8 {
    CPA_Horizontal UMETA(Displayname = "Horizontal List", Tooltip = "Nested Properties will be Listed in a Horizontal Layout"),
    CPA_Vertical UMETA(Displayname = "Vertical List", Tooltip = "Nested Properties will be Listed in a Vertical Layout"),
};

UCLASS(EditInlineNew)
class SML_API UCP_Array : public UConfigPropertyArray {
    GENERATED_BODY()
public:

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category= "Default", meta = (DisplayAfter = "DefaultValue"))
    ECP_ArrayWidgetType WidgetType;

};
