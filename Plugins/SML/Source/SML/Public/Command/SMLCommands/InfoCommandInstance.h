#pragma once
#include "Command/ChatCommandInstance.h"
#include "InfoCommandInstance.generated.h"

UCLASS(MinimalAPI)
class AInfoCommandInstance : public AChatCommandInstance {
    GENERATED_BODY()
public:
    AInfoCommandInstance();
    EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};
