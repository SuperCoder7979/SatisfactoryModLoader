#include "Command/SMLCommands/PlayerListCommandInstance.h"
#include "Command/CommandSender.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "UnrealEngine.h"

APlayerListCommandInstance::APlayerListCommandInstance() {
    CommandName = TEXT("list");
    Usage = TEXT("/list - List players online");
    Aliases.Add(TEXT("players"));
}

EExecutionStatus APlayerListCommandInstance::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) {
    TArray<FString> PlayersList;
    for (TPlayerControllerIterator<APlayerController>::ServerAll It(GetWorld()); It; ++It) {
        APlayerController* Controller = *It;
        PlayersList.Add(Controller->PlayerState->GetPlayerName());
    }
    const FString PlayerListString = FString::Join(PlayersList, TEXT(", "));
    Sender->SendChatMessage(FString::Printf(TEXT("Players Online: %s"), *PlayerListString));
    return EExecutionStatus::COMPLETED;
}
