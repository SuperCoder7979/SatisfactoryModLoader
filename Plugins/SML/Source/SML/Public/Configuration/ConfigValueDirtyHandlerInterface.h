#pragma once
#include "CoreMinimal.h"
#include "ConfigValueDirtyHandlerInterface.generated.h"

/** Blueprint wrapper for the IUConfigValueDirtyHandler interface */
UINTERFACE(Blueprintable)
class SML_API UConfigValueDirtyHandlerInterface : public UInterface {
    GENERATED_BODY()
};

/**
* Used to mark configuration value dirty.
* First instance of the Outer object implementing this interface is selected
* upon UConfigValue.MarkDirty() call and then called to process
* marking configuration dirty
*/
class SML_API IConfigValueDirtyHandlerInterface {
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent)
    void MarkDirty();
};
