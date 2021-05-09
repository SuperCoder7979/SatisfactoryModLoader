#include "Configuration/Properties/WidgetExtension/CP_Integer.h"

UCP_Integer::UCP_Integer() {
    MinValue = 0;
    MaxValue = 1;
}

TArray<FName> UCP_Integer::GetEnumNames() const {
    if (EnumClass) {
        TArray<FName> Out;
        for (int32 i = 0; i < EnumClass->NumEnums(); i++) Out.Add(EnumClass->GetNameByIndex(i));
        return Out;
    }
    return TArray<FName>();
}
