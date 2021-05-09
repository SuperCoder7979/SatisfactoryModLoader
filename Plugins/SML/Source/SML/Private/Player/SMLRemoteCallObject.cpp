#include "Player/SMLRemoteCallObject.h"
#include "Command/ChatCommandLibrary.h"
#include "FGChatManager.h"
#include "FGPlayerController.h"
#include "Patching/NativeHookManager.h"
#include "Net/UnrealNetwork.h"
#include "Registry/RemoteCallObjectRegistry.h"

USMLRemoteCallObject::USMLRemoteCallObject() {
    CommandSender = CreateDefaultSubobject<UPlayerCommandSender>(TEXT("PlayerCommandSender"));
}

bool USMLRemoteCallObject::IsClientModInstalled(const FString& ModId) const {
    return ClientInstalledMods.Contains(ModId);
}

FVersion USMLRemoteCallObject::GetClientModVersion(const FString& ModId) const {
    const FVersion* Result = ClientInstalledMods.Find(ModId);
    return Result ? *Result : FVersion{};
}

void USMLRemoteCallObject::HandleChatCommand_Implementation(const FString& CommandLine) {
    AChatCommandSubsystem* ChatSystem = AChatCommandSubsystem::Get(this);
    check(ChatSystem);
    ChatSystem->RunChatCommand(CommandLine, CommandSender);
}

void USMLRemoteCallObject::SendChatMessage_Implementation(const FString& Message, const FLinearColor& Color) {
    AFGChatManager* ChatManager = AFGChatManager::Get(GetWorld());
    FChatMessageStruct MessageStruct;
    MessageStruct.MessageString = Message;
    MessageStruct.MessageType = EFGChatMessageType::CMT_SystemMessage;
    MessageStruct.ServerTimeStamp = GetWorld()->TimeSeconds;
    MessageStruct.CachedColor = Color;
    ChatManager->AddChatMessageToReceived(MessageStruct);
}

bool USMLRemoteCallObject::HandleChatCommand_Validate(const FString& CommandLine) {
    return true;
}

void USMLRemoteCallObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
    DOREPLIFETIME(USMLRemoteCallObject, DummyReplicatedField);
}

void USMLRemoteCallObject::RegisterChatCommandPatch() {
    SUBSCRIBE_METHOD(
        AFGPlayerController::EnterChatMessage,
        [](auto& Scope, AFGPlayerController* PlayerController, const FString& Message) {
        if (Message.StartsWith(TEXT("/"))) {
        const FString CommandLine = Message.TrimStartAndEnd().RightChop(1);
        USMLRemoteCallObject* RemoteCallObject = Cast<USMLRemoteCallObject>(PlayerController->GetRemoteCallObjectOfClass(USMLRemoteCallObject::StaticClass()));
        if (RemoteCallObject != NULL) {
        RemoteCallObject->HandleChatCommand(CommandLine);
        }
        Scope.Cancel();
        }
        }
    );
}
