#pragma once
#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "Player/PlayerCommandSender.h"
#include "Util/SemVersion.h"
#include "SMLRemoteCallObject.generated.h"

UCLASS(NotBlueprintable)
class SML_API USMLRemoteCallObject : public UFGRemoteCallObject {
    GENERATED_BODY()
public:
    USMLRemoteCallObject();

    /** Command Sender associated with this player */
    UPROPERTY(BlueprintReadOnly)
    UPlayerCommandSender* CommandSender;

    /** Returns true whenever provided mod is installed on client side */
    UFUNCTION(BlueprintPure)
    bool IsClientModInstalled(const FString& ModId) const;

    /** Returns version of the mod installed on client side. Returns 0.0.0 if mod is not installed */
    UFUNCTION(BlueprintPure)
    FVersion GetClientModVersion(const FString& ModId) const;

    /** Sends system chat message to the player */
    UFUNCTION(BlueprintCallable, Reliable, Client)
    void SendChatMessage(const FString& Message, const FLinearColor& Color);

    /** Called client side to process chat command on server */
    UFUNCTION(BlueprintCallable, Reliable, Server, WithValidation = HandleChatCommand_Validate)
    void HandleChatCommand(const FString& CommandLine);

    /** Validation function for HandleChatCommand */
    bool HandleChatCommand_Validate(const FString& CommandLine);

    void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
private:
    // To be able to modify client installed mods list
    friend class FSMLNetworkManager;
    friend class FSatisfactoryModLoader;

    /* List of mods installed on the client side */
    TMap<FString, FVersion> ClientInstalledMods;

    /** Needed for RCO to work */
    UPROPERTY(Replicated)
    int32 DummyReplicatedField;

    /** Registers chat command patch for routing chat commands through SML RCO */
    static void RegisterChatCommandPatch();
};
