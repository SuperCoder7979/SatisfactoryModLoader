#include "Patching/Patch/OfflinePlayerHandler.h"
#include "FGLocalPlayer.h"
#include "Patching/NativeHookManager.h"
#include "Net/OnlineEngineInterface.h"
#include "SatisfactoryModLoader.h"

void FOfflinePlayerHandler::RegisterHandlerPatches() {
    ULocalPlayer* LocalPlayerInstance = GetMutableDefault<ULocalPlayer>();
    SUBSCRIBE_METHOD_VIRTUAL(
        ULocalPlayer::GetNickname,
        LocalPlayerInstance,
        [](auto& Call, const ULocalPlayer* Player) {
        const FString ReturnedParentValue = Call(Player);
        if (ReturnedParentValue.IsEmpty()) {
        const FString UsernameOverride = FOfflinePlayerHandler::GetOfflineUsernameOverride();
        if (!UsernameOverride.IsEmpty()) {
        Call.Override(UsernameOverride);
        }
        }
        }
    );

    SUBSCRIBE_METHOD(
        ULocalPlayer::GetUniqueNetIdFromCachedControllerId,
        [](auto& Call, const ULocalPlayer* Player) {
        const FUniqueNetIdRepl ReturnedParentValue = Call(Player);
        if (!ReturnedParentValue.IsValid()) {
        const FString Username = FOfflinePlayerHandler::GetOfflineUsernameOverride();
        if (!Username.IsEmpty()) {
        Call.Override(FOfflinePlayerHandler::GenerateUniqueNetIdFromUsername(Username));
        }
        }
        }
    );
}

FString FOfflinePlayerHandler::GetOfflineUsernameOverride() {
    static FString UsernameOverride = TEXT("");
    static bool bReadFromCommandLine = false;
    if (!bReadFromCommandLine) {
        FParse::Value(FCommandLine::Get(), TEXT("-Username="), UsernameOverride);
        UE_LOG(LogSatisfactoryModLoader, Display, TEXT("Offline username override: %s"), *UsernameOverride);
        bReadFromCommandLine = true;
    }
    return UsernameOverride;
}

FUniqueNetIdRepl FOfflinePlayerHandler::GenerateUniqueNetIdFromUsername(const FString& Username) {
    return FUniqueNetIdRepl(UOnlineEngineInterface::Get()->CreateUniquePlayerId(Username));
}
