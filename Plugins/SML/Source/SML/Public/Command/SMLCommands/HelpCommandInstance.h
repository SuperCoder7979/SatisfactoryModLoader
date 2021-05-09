#pragma once
#include "Command/ChatCommandInstance.h"
#include "HelpCommandInstance.generated.h"

UCLASS(MinimalAPI)
class AHelpCommandInstance : public AChatCommandInstance {
    GENERATED_BODY()
public:
    AHelpCommandInstance();
    EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};
