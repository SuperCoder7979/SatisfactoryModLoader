#include "Command/SMLCommands/InfoCommandInstance.h"
#include "Command/CommandSender.h"
#include "SatisfactoryModLoader.h"
#include "Engine/Engine.h"
#include "ModLoading/ModLoadingLibrary.h"

AInfoCommandInstance::AInfoCommandInstance() {
    CommandName = TEXT("info");
    Usage = TEXT("/info - Information about the modded environment");
    Aliases.Add(TEXT("version"));
}

EExecutionStatus AInfoCommandInstance::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) {
    const FVersion Version = FSatisfactoryModLoader::GetModLoaderVersion();
    Sender->SendChatMessage(FString::Printf(TEXT("Running SML v.%s"), *Version.ToString()));

    UModLoadingLibrary* ModLoadingLibrary = GEngine->GetEngineSubsystem<UModLoadingLibrary>();
    const TArray<FModInfo> LoadedMods = ModLoadingLibrary->GetLoadedMods();
    const FString ModListString = FString::JoinBy(LoadedMods, TEXT(", "), [](const FModInfo& ModInfo) { return ModInfo.FriendlyName; });
    Sender->SendChatMessage(FString::Printf(TEXT("Loaded Mods: %s"), *ModListString));

    return EExecutionStatus::COMPLETED;
}
