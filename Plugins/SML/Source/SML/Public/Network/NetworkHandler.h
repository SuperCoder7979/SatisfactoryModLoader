#pragma once
#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "UObject/Object.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NetworkHandler.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogModNetworkHandler, Log, All);

DECLARE_DELEGATE_TwoParams(FMessageReceived, class UNetConnection* /*Connection*/, FString /*Data*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FWelcomePlayer, UWorld* /*ServerWorld*/, class UNetConnection* /*Connection*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FClientInitialJoin, class UNetConnection* /*Connection*/);

struct FMessageType {
    FString ModReference;
    int32 MessageId;
};

struct FMessageEntry {
    bool bClientHandled;
    bool bServerHandled;
    FMessageReceived MessageReceived;
};

/**
 * Mod Network Handler
 *
 * Mod network handler manages server-client communication during connection setup,
 * before any actor channels are created, player controller initialized or map replicated
 * It can be used to perform early validation and syncing on both server and client side
 *
 * For example, SML uses it to check mods installed on client side in advance
 * before proceeding to player join, and deny join request if mods versions mismatch, or mods are missing
 * Most of the time you want to use normal replication for gameplay-related stuff
 */
UCLASS()
class SML_API UModNetworkHandler : public UEngineSubsystem {
    GENERATED_BODY()
private:
    UPROPERTY()
    TMap<TWeakObjectPtr<class UNetConnection>, class UObjectMetadata*> Metadata;
    TMap<FString, TMap<int32, FMessageEntry>> MessageHandlers;
    FWelcomePlayer WelcomePlayerDelegate;
    FClientInitialJoin ClientLoginDelegate;
private:
    void ReceiveMessage(class UNetConnection* Connection, const FString& ModId, int32 MessageId, const FString& Content) const;
public:
    /**
     * Retrieves metadata object for given connection
     * Metadata object can be used to store information related to given connection before
     * player controller is initialized, and then move it to created player controller
     */
    class UObjectMetadata* GetMetadataForConnection(UNetConnection* Connection);

    /**
     * Delegate called on server when he received client join request and welcomed new player
     * You can send additional information to client here, or check information received by client
     * before to perform any required validation
     */
    FORCEINLINE FWelcomePlayer& OnWelcomePlayer() { return WelcomePlayerDelegate; }

    /**
     * Delegate called when client has sent initial join request to remote side
     * Here you can send additional information to be acknowledged by the server via SendMessage
     */
    FORCEINLINE FClientInitialJoin& OnClientInitialJoin() { return ClientLoginDelegate; }

    /**
     * Register new mod message type and return message entry which can be used
     * to set message processing preferences and siding
     */
    FMessageEntry& RegisterMessageType(const FMessageType& MessageType);

    static void CloseWithFailureMessage(class UNetConnection* Connection, const FString& Message);

    /**
     * Send registered mod message to this connection to be processed on the remote side
     */
    static void SendMessage(class UNetConnection* Connection, FMessageType MessageType, FString Data);
private:
    friend class FSatisfactoryModLoader;

    /** Registers hooks associated with network handler */
    static void InitializePatches();
};
