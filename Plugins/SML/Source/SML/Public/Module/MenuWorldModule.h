#pragma once
#include "CoreMinimal.h"
#include "Module/WorldModule.h"
#include "MenuWorldModule.generated.h"

/** World module initialized only for the main menu world */
UCLASS(Blueprintable)
class SML_API UMenuWorldModule : public UWorldModule {
    GENERATED_BODY()
};
