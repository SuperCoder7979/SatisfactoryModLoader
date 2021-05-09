#include "Module/WorldModule.h"

UWorld* UWorldModule::GetWorld() const {
    // Return nullptr on class default object to trick ImplementsGetWorld()
    if (HasAllFlags(RF_ClassDefaultObject)) {
        return nullptr;
    }

    // Otherwise lookup Outer hierarchy to find UWorld object pointer
    // UWorldModule are maintained by module manager subsystem in the world, which will always implement GetWorld()
    return GetOuter()->GetWorld();
}
