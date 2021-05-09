#pragma once
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "CP_Float.generated.h"

UENUM(Blueprinttype)
enum class ECP_FloatWidgetType : uint8 {
    CPF_Text UMETA(Displayname = "Text Box", Tooltip = "A simple Text entry Box"),
    CPF_Slider UMETA(Displayname = "Slider", Tooltip = "A Slider with a Min and Max Value"),
    CPF_Spinbox UMETA(Displayname = "Spinbox", Tooltip = "A Spinbox (drag & type) with a Min and Max Value"),
};

UCLASS(EditInlineNew)
class SML_API UCP_Float : public UConfigPropertyFloat {
    GENERATED_BODY()

    UCP_Float();

public:

    UPROPERTY(EditAnywhere, Category = "Default", BlueprintReadOnly, meta = (DisplayAfter = "DefaultValue"))
    ECP_FloatWidgetType WidgetType;
    UPROPERTY(EditAnywhere, Category = "Default", BlueprintReadOnly, meta = (DisplayAfter = "WidgetType"))
    float MinValue;
    UPROPERTY(EditAnywhere, Category = "Default", BlueprintReadOnly, meta = (DisplayAfter = "MaxValue"))
    float MaxValue;

};
