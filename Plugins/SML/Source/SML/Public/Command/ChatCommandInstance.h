#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "ChatCommandInstance.generated.h"

/** Describes result of the command execution */
UENUM(BlueprintType)
enum class EExecutionStatus : uint8 {
    /** command executed successfully */
    COMPLETED,
    /** command not executed, or had no effect */
    UNCOMPLETED,
    /** command not executed, user permissions are too low */
    INSUFFICIENT_PERMISSIONS,
    /** command failed due to user providing invalid arguments */
    BAD_ARGUMENTS
};

UCLASS(Abstract, Blueprintable)
class SML_API AChatCommandInstance : public AInfo {
    GENERATED_BODY()
public:
    AChatCommandInstance();
private:
    friend class AChatCommandSubsystem;

    // Replicated in case of command actor wanting to replicate
    UPROPERTY(Replicated)
    FName ModReference;
public:
    /**
    * ModId of the mod registering the command
    */
    UFUNCTION(BlueprintPure)
    FORCEINLINE FName GetOwnerModReference() const { return ModReference; }

    /**
     * Whenever this command can only be used by player sender
     * Trying to run it from non-player sender will print error message
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    uint8 bOnlyUsableByPlayer: 1;

    /**
     * Minimum number of arguments to enforce before
     * actually giving execution control to the command
     * Providing less will instead print command usage
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 MinNumberOfArguments;

    /**
     * Name of the command
     * Not limited in any way, but consider using only ASCII chars there
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FString CommandName;

    /**
     * List of the additional names command can be referenced by
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<FString> Aliases;
    /**
     * Usage of the command
     * Example:
     * msg <username> <message>
     * give <username> <item> [amount] [extra_data]
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FString Usage;

    void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /**
     * Command handler function to be called when command is executed
     *
     * @param Sender Command Sender caused execution of this command
     * @param Arguments Additional arguments passed to command
     * @param Label Command Alias used for calling this command
     * @return Execute Status describing what command actually did
     */
    UFUNCTION(BlueprintNativeEvent)
    EExecutionStatus ExecuteCommand(class UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label);

    /**
    * Prints usage of this command to the given player
    *
    * @param CommandSender ICommandSenderInterface implementing object
    */
    UFUNCTION(BlueprintCallable)
    void PrintCommandUsage(class UCommandSender* CommandSender) const;
};
