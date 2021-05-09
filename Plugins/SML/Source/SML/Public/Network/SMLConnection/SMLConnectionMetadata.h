#pragma once
#include "UObject/Object.h"
#include "Util/SemVersion.h"
#include "SMLConnectionMetadata.generated.h"

UCLASS()
class SML_API USMLConnectionMetadata : public UObject {
    GENERATED_BODY()
public:
    bool bIsInitialized;
    TMap<FString, FVersion> InstalledClientMods;
};
