#include "Command/SMLCommands/HelpCommandInstance.h"
#include "Command/ChatCommandLibrary.h"
#include "Command/CommandSender.h"

AHelpCommandInstance::AHelpCommandInstance() {
    CommandName = TEXT("help");
    Usage = TEXT("/help - Command help");
    Aliases.Add(TEXT("?"));
}

EExecutionStatus AHelpCommandInstance::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) {
    AChatCommandSubsystem* CommandSubsystem = AChatCommandSubsystem::Get(this);
    check(CommandSubsystem);
    if (Arguments.Num() >= 2) {
        const FString& TargetCommandName = Arguments[1];
        AChatCommandInstance* CommandEntry = CommandSubsystem->FindCommandByName(TargetCommandName);
        if (!CommandEntry) {
            Sender->SendChatMessage(FString(TEXT("Command not found: ")) += TargetCommandName, FLinearColor::Red);
            return EExecutionStatus::BAD_ARGUMENTS;
        }
        Sender->SendChatMessage(CommandEntry->Usage);
        return EExecutionStatus::COMPLETED;
    }
    Sender->SendChatMessage(TEXT("Command List:"));
    for (const AChatCommandInstance* CommandEntry : CommandSubsystem->GetRegisteredCommands()) {
        Sender->SendChatMessage(CommandEntry->Usage);
    }
    return EExecutionStatus::COMPLETED;
}
