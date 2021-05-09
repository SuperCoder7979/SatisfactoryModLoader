#pragma once
#include "Command/ChatCommandInstance.h"
#include "PlayerListCommandInstance.generated.h"

UCLASS(MinimalAPI)
class APlayerListCommandInstance : public AChatCommandInstance {
    GENERATED_BODY()
public:
    APlayerListCommandInstance();
    EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};
