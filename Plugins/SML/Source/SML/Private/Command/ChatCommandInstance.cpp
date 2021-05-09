#include "Command/ChatCommandInstance.h"
#include "Command/CommandSender.h"
#include "Net/UnrealNetwork.h"

AChatCommandInstance::AChatCommandInstance() {
    bOnlyUsableByPlayer = false;
    MinNumberOfArguments = 0;
}

// Default implementation that will call CommandExecuted if it is bound
EExecutionStatus AChatCommandInstance::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) {
    return EExecutionStatus::UNCOMPLETED;
}

void AChatCommandInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
    DOREPLIFETIME(AChatCommandInstance, ModReference);
}

void AChatCommandInstance::PrintCommandUsage(UCommandSender* Player) const {
    Player->SendChatMessage(FString::Printf(TEXT("Usage: %s"), *Usage), FLinearColor::Red);
}
