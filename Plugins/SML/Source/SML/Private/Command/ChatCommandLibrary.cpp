#include "Command/ChatCommandLibrary.h"
#include "Command/CommandSender.h"
#include "FGPlayerController.h"
#include "Command/SMLCommands/HelpCommandInstance.h"
#include "Command/SMLCommands/InfoCommandInstance.h"
#include "Command/SMLCommands/PlayerListCommandInstance.h"
#include "SatisfactoryModLoader.h"
#include "Subsystem/SubsystemActorManager.h"

DEFINE_LOG_CATEGORY(LogChatCommand);

FString ParseCommandArgument(const FString& Line, int32& Off);

void PrintCommandNotFound(UCommandSender* Player) {
    Player->SendChatMessage(TEXT("Unknown command. Type /help for a list of commands."), FLinearColor::Red);
}

void PrintCommandOnlyUsableByPlayer(UCommandSender* Player) {
    Player->SendChatMessage(TEXT("This command is only usable by players."), FLinearColor::Red);
}

AChatCommandSubsystem::AChatCommandSubsystem() {
    this->ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer;
}

AChatCommandSubsystem* AChatCommandSubsystem::Get(UObject* WorldContext) {
    UWorld* WorldObject = GEngine->GetWorldFromContextObjectChecked(WorldContext);
    USubsystemActorManager* SubsystemActorManager = WorldObject->GetSubsystem<USubsystemActorManager>();
    check(SubsystemActorManager);

    return SubsystemActorManager->GetSubsystemActor<AChatCommandSubsystem>();
}

AChatCommandInstance* AChatCommandSubsystem::FindCommandByName(const FString& Name) {
    AChatCommandInstance** ChatCommandInstance = CommandByNameMap.Find(Name);
    return ChatCommandInstance ? *ChatCommandInstance : NULL;
}

TArray<AChatCommandInstance*> AChatCommandSubsystem::GetRegisteredCommands() const {
    return RegisteredCommands;
}

void AChatCommandSubsystem::Init() {
    RegisterCommand(TEXT("SML"), AHelpCommandInstance::StaticClass());
    RegisterCommand(TEXT("SML"), AInfoCommandInstance::StaticClass());
    RegisterCommand(TEXT("SML"), APlayerListCommandInstance::StaticClass());
}

TArray<AFGPlayerController*> AChatCommandSubsystem::ParsePlayerName(UCommandSender* Caller, const FString& Name, UObject* WorldContext) {
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::Assert);
    TArray<AFGPlayerController*> PlayerControllers;

    if (Name == TEXT("@self") || Name == TEXT("@s")) {
        if (Caller->IsPlayerSender()) {
            PlayerControllers.Add(Caller->GetPlayer());
        }
    } else if (Name == TEXT("@all") || Name == TEXT("@a")) {
        for (TPlayerControllerIterator<AFGPlayerController>::ServerAll It(World); It; ++It) {
            PlayerControllers.Add(*It);
        }
    } else {
        for (TPlayerControllerIterator<AFGPlayerController>::ServerAll It(World); It; ++It) {
            AFGPlayerController* Controller = *It;
            APlayerState* PlayerState = Controller->GetPlayerState<APlayerState>();
            if (PlayerState != NULL && PlayerState->GetPlayerName() == Name) {
                PlayerControllers.Add(Controller);
            }
        }
    }
    return PlayerControllers;
}

FString MakeFQCommandName(const FString& ModId, const FString& Name) {
    return FString::Printf(TEXT("%s:%s"), *ModId, *Name);
}

void AChatCommandSubsystem::RegisterCommand(const FString& ModReference, TSubclassOf<AChatCommandInstance> CommandClass) {
    // Ensure that mod with specified ModId is loaded and active
    AChatCommandInstance* CommandCDO = CommandClass->GetDefaultObject<AChatCommandInstance>();
    const FString FqCommandName = MakeFQCommandName(ModReference, CommandCDO->CommandName);
    // Only register command if it's not already registered
    if (!CommandByNameMap.Contains(FqCommandName)) {
        const FString ActorName = FString::Printf(TEXT("ChatCommand_%s_%s"), *ModReference, *CommandCDO->CommandName);
        // Spawn command actor with predefined name
        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = *ActorName;
        AChatCommandInstance* Command = GetWorld()->SpawnActor<AChatCommandInstance>(CommandClass, SpawnParams);
        check(Command);
        // Make sure ModReference is set for spawned command actor
        Command->ModReference = *ModReference;

        // register all command aliases
        TArray<FString> AllCommandNames;
        AllCommandNames.Add(Command->CommandName);
        AllCommandNames.Append(Command->Aliases);

        for (const FString& CommandAlias : AllCommandNames) {
            const FString FqCommandAlias = MakeFQCommandName(ModReference, CommandAlias);
            CommandByNameMap.Add(CommandAlias.ToLower(), Command);
            CommandByNameMap.Add(FqCommandAlias.ToLower(), Command);
        }
        // Add command actor to the registered actor list
        RegisteredCommands.Add(Command);
        UE_LOG(LogChatCommand, Display, TEXT("Registered chat command %s:%s"), *ModReference, *Command->CommandName);
    }
}

EExecutionStatus AChatCommandSubsystem::RunChatCommand(const FString& CommandLine, UCommandSender* Sender) {
    // Empty command line, print help
    if (CommandLine.IsEmpty()) {
        PrintCommandNotFound(Sender);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    // Split command into the separate arguments respecting escaped spaces and quotes
    TArray<FString> ResultArgArray;
    int32 Offset = 0;
    while (CommandLine.Len() > Offset) {
        FString resultArg = ParseCommandArgument(CommandLine, Offset);
        ResultArgArray.Add(resultArg);
        // skip argument separator
        Offset++;
    }

    // First element in the argument array is always a command name
    const FString CommandAliasUsed = ResultArgArray[0];

    // Remove command name from arguments array
    ResultArgArray.RemoveAt(0);
    AChatCommandInstance* CommandEntry = FindCommandByName(CommandAliasUsed);
    if (CommandEntry == nullptr) {
        PrintCommandNotFound(Sender);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    const FString CommandFQName = MakeFQCommandName(CommandEntry->GetOwnerModReference().ToString(), CommandEntry->CommandName);
    const FSMLConfiguration Configuration = FSatisfactoryModLoader::GetSMLConfiguration();

    // Check if command has been disabled in SML configuration
    if (Configuration.DisabledChatCommands.Contains(CommandFQName) && Sender->IsPlayerSender()) {
        Sender->SendChatMessage(TEXT("This command has been disabled by server owner."), FLinearColor::Red);
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;
    }

    // Check if command is play only, and we are trying to run it as server
    if (CommandEntry->bOnlyUsableByPlayer && !Sender->IsPlayerSender()) {
        PrintCommandOnlyUsableByPlayer(Sender);
        return EExecutionStatus::UNCOMPLETED;
    }

    // Check if command doesn't have enough arguments
    if (CommandEntry->MinNumberOfArguments > ResultArgArray.Num()) {
        CommandEntry->PrintCommandUsage(Sender);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    // Actually run command now and return response
    EExecutionStatus ExecutionStatus = CommandEntry->ExecuteCommand(Sender, ResultArgArray, CommandAliasUsed);

    // Print some logging information about command executed and response
    UEnum* ExecutionStatusEnum = StaticEnum<EExecutionStatus>();
    const FString StatusString = ExecutionStatusEnum->GetNameStringByValue((int64)ExecutionStatus);
    UE_LOG(LogChatCommand, Display, TEXT("%s: /%s [%s]"), *Sender->GetSenderName(), *CommandLine, *StatusString);

    return ExecutionStatus;
}

FString ParseCommandArgument(const FString& Line, int32& Off) {
    if (Off >= Line.Len()) return TEXT("");
    const wchar_t EscapeChar = TEXT('\\');
    const wchar_t FirstChar = Line[Off++];
    bool bPreviousCharIsEscape = FirstChar == EscapeChar;
    const bool bIsQuoted = FirstChar == TEXT('"');
    const wchar_t breakCharacter = bIsQuoted ? TEXT('"') : TEXT(' ');
    FString Result;
    // if first char is not a quote, append it too!
    if (!bIsQuoted && !bPreviousCharIsEscape) {
        Result.AppendChar(FirstChar);
    }
    while (Line.Len() > Off) {
        const wchar_t Character = Line[Off];
        if (!bPreviousCharIsEscape) {
            // break on the control character
            if (Character == breakCharacter) break;
            // set last character as escape
            if (Character == EscapeChar) {
                bPreviousCharIsEscape = true;
                // continue here to skip resetting prevIsEscape
                // and adding character to the result
                continue;
            }
        }
        Result.AppendChar(Character);
        bPreviousCharIsEscape = false;
        Off++;
    }
    return Result;
}
