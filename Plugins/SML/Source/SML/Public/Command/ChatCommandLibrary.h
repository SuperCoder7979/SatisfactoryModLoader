#pragma once
#include "CoreMinimal.h"
#include "command/ChatCommandInstance.h"
#include "Subsystem/ModSubsystem.h"
#include "ChatCommandLibrary.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChatCommand, Log, All);

UCLASS(NotBlueprintable)
class SML_API AChatCommandSubsystem : public AModSubsystem {
    GENERATED_BODY()
private:
    // Array of registered command actors
    UPROPERTY()
    TArray<AChatCommandInstance*> RegisteredCommands;
    // Map to lookup command instances fast
    UPROPERTY()
    TMap<FString, AChatCommandInstance*> CommandByNameMap;
public:
    AChatCommandSubsystem();

    /**
     * Retrieves Chat Command Subsystem instance for a given world
     * Will always return something unless called really early
     */
    UFUNCTION(BlueprintPure)
    static AChatCommandSubsystem* Get(UObject* WorldContext);

    /**
     * Searches command register entry by given command name
     *
     * @param Name Name to search for, can be command name or one of aliases. Accepts both full and short names
     * @return Found entry, or nullptr if not found
     */
    UFUNCTION(BlueprintPure, Category = "Utilities|ChatCommand")
    AChatCommandInstance* FindCommandByName(const FString& Name);

    /*
     * Returns array of all registered commands
     */
    UFUNCTION(BlueprintPure, Category = "Utilities|ChatCommand")
    TArray<AChatCommandInstance*> GetRegisteredCommands() const;

    /**
    * Registers a chat command to a chat command subsystem
    * Will return silently if command with same qualified name already exists
    *
    * @param Command Command data to register
    * @param ModReference mod reference
    */
    UFUNCTION(BlueprintCallable, Category = "Utilities|ChatCommand")
    void RegisterCommand(const FString& ModReference, TSubclassOf<AChatCommandInstance> Command);

    /**
    * Parses command line and executes given command from the face of the given player
    * Characters enclosed in "" are considered a single argument
    * If command is not found, returns bad_arguments
    *
    * @param CommandLine Command line to execute, without prefix slash
    * @param Sender Sender which caused command execution. Should implement ICommandSenderInterface
    */
    UFUNCTION(BlueprintCallable, Category = "Utilities|ChatCommand")
    EExecutionStatus RunChatCommand(const FString& CommandLine, UCommandSender* Sender);

    /**
    * Parses given player name, returning all players matching
    *
    * It supports multiple target selectors:
    * @s(elf) - targets player who executed this command
    * @a(ll) - targets all players on the server
    * otherwise - treated as player nickname
    *
    * @param Caller caller of the original command
    * @param Name Name to parse against selectors
    * @param WorldContext World Context
    * @return Array of players matching given name
    */
    UFUNCTION(BlueprintPure, Category = "Utilities|ChatCommand", meta = (WorldContext = "WorldContext"))
    static TArray<class AFGPlayerController*> ParsePlayerName(UCommandSender* Caller, const FString& Name, UObject* WorldContext);
protected:
    /** Initializes builtin commands for the command subsystem */
    virtual void Init() override;
};
