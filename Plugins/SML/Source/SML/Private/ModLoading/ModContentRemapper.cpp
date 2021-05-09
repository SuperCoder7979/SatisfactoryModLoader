#include "ModLoading/ModContentRemapper.h"

#include "SatisfactoryModLoader.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/CoreRedirects.h"

void UModContentRemapper::RegisterPackageRedirect(const FString& OriginalPackage, const FString& NewPackage, bool bMatchSubstring) {
    ECoreRedirectFlags Flags = ECoreRedirectFlags::Type_Package;
    if (bMatchSubstring) {
        Flags |= ECoreRedirectFlags::Option_MatchSubstring;
    }

    TArray<FCoreRedirect> RedirectList;
    RedirectList.Add(FCoreRedirect(Flags, OriginalPackage, NewPackage));
    FCoreRedirects::AddRedirectList(RedirectList, TEXT("ModContentRemapper::RegisterPackageRedirect"));
}

void UModContentRemapper::RegisterClassRedirect(const FString& OldClassName, const FString& NewClassName) {
    TArray<FCoreRedirect> RedirectList;
    RedirectList.Add(FCoreRedirect(ECoreRedirectFlags::Type_Class, OldClassName, NewClassName));
    FCoreRedirects::AddRedirectList(RedirectList, TEXT("ModContentRemapper::RegisterClassRedirect"));
}

bool UModContentRemapper::ShouldCreateSubsystem(UObject* Outer) const {
    // Only initialize remapping system in cooked environment
    return FPlatformProperties::RequiresCookedData();
}

FCoreRedirect MakeRedirectForPlugin(IPlugin& Plugin) {
    const FString OldPath = FString::Printf(TEXT("/Game/%s/"), *Plugin.GetName());
    const FString NewPath = Plugin.GetMountedAssetPath();
    const ECoreRedirectFlags Flags = ECoreRedirectFlags::Type_Package | ECoreRedirectFlags::Option_MatchSubstring;
    return FCoreRedirect(Flags, OldPath, NewPath);
}

void UModContentRemapper::Initialize(FSubsystemCollectionBase& Collection) {
    TArray<FCoreRedirect> PluginRemaps;

    IPluginManager& PluginManager = IPluginManager::Get();
    PluginManager.OnNewPluginCreated().AddUObject(this, &UModContentRemapper::OnNewPluginMounted);
    PluginManager.OnNewPluginMounted().AddUObject(this, &UModContentRemapper::OnNewPluginMounted);

    TArray<TSharedRef<IPlugin>> EnabledPluginsWithContent = PluginManager.GetEnabledPluginsWithContent();
    for (const TSharedRef<IPlugin>& Plugin : EnabledPluginsWithContent) {
        if (Plugin->GetType() == EPluginType::Mod) {
            PluginRemaps.Add(MakeRedirectForPlugin(Plugin.Get()));
        }
    }
    FCoreRedirects::AddRedirectList(PluginRemaps, TEXT("UModContentRemapper::Initialize"));
    UE_LOG(LogSatisfactoryModLoader, Log, TEXT("Registered %d plugin content remaps"), PluginRemaps.Num());
}

void UModContentRemapper::OnNewPluginMounted(IPlugin& Plugin) {
    if (Plugin.IsEnabled() && Plugin.GetType() == EPluginType::Mod && Plugin.CanContainContent()) {
        const FString PluginName = Plugin.GetName();
        if (!PluginsAlreadyHandled.Contains(PluginName)) {
            const FCoreRedirect PluginRedirect = MakeRedirectForPlugin(Plugin);
            FCoreRedirects::AddRedirectList({PluginRedirect}, TEXT("UModContentRemapper::OnNewPluginMounted"));
            this->PluginsAlreadyHandled.Add(PluginName);
        }
    }
}
