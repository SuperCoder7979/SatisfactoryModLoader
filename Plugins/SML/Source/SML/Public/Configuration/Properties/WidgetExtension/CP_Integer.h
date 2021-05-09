#pragma once
#include "Configuration/Properties/ConfigPropertyInteger.h"
#include "CP_Integer.generated.h"

UENUM(Blueprinttype)
enum class ECP_IntegerWidgetType : uint8 {
    CPI_Text UMETA(Displayname = "Text Box", Tooltip = "A simple Text entry Box"),
    CPI_Slider UMETA(Displayname = "Slider", Tooltip = "A Slider with a Min and Max Value"),
    CPI_Spinbox UMETA(Displayname = "Spinbox", Tooltip = "A Spinbox (drag & type) with a Min and Max Value"),
    CPI_Enum UMETA(Displayname = "Enum", Tooltip = "A DropDown list of Enum Field Names"),
};

UCLASS(EditInlineNew)
class SML_API UCP_Integer : public UConfigPropertyInteger {
    GENERATED_BODY()
public:

    UCP_Integer();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayAfter = "DefaultValue"))
    ECP_IntegerWidgetType WidgetType;

    UPROPERTY(EditAnywhere, Category = "Default", BlueprintReadOnly, meta = (DisplayAfter = "WidgetType"))
    int32 MinValue;
    UPROPERTY(EditAnywhere, Category = "Default", BlueprintReadOnly, meta = (DisplayAfter = "MaxValue"))
    int32 MaxValue;

    // Custom Enum Class to use for the display Widget
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "WidgetType == ECP_IntegerWidgetType::CPI_Enum", EditConditionHides, DisplayAfter = "WidgetType"))
    class UEnum* EnumClass;

    UFUNCTION(BlueprintPure)
    TArray<FName> GetEnumNames() const;

};
