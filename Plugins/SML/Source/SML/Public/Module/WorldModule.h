#pragma once
#include "CoreMinimal.h"
#include "Module/ModModule.h"
#include "WorldModule.generated.h"

/** World modules are initialized when world is initialized and are bound to world lifetime */
UCLASS(Blueprintable)
class SML_API UWorldModule : public UModModule {
    GENERATED_BODY()
public:
    /** Allow world module manager to set mod reference */
    friend class UWorldModuleManager;

    /** World modules have their world context from attached world */
    virtual UWorld* GetWorld() const override;
};
