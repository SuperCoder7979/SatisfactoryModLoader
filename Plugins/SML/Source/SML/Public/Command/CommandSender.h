#pragma once
#include "CoreMinimal.h"
#include "CommandSender.generated.h"

/**
* Generic object representing abstract command source
*/
UCLASS(Abstract, BlueprintType)
class SML_API UCommandSender : public UObject {
    GENERATED_BODY()
public:
    /**
     * Returns human readable name of command sender, like player name or [Console]
     */
    UFUNCTION(BlueprintPure)
    virtual FString GetSenderName() const;

    /**
    * Returns true if this command sender represents player, false otherwise
    */
    UFUNCTION(BlueprintPure)
    virtual bool IsPlayerSender() const;

    /**
     * If this command sender is player, returns player controller instance
     */
    UFUNCTION(BlueprintPure)
    virtual class AFGPlayerController* GetPlayer() const;

    /**
     * Sends chat message to this command sender
     */
    UFUNCTION(BlueprintCallable)
    virtual void SendChatMessage(const FString& Message, const FLinearColor PrefixColor = FLinearColor::Green);
};
