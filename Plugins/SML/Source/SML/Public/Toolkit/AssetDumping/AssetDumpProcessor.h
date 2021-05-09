#pragma once
#include "CoreMinimal.h"
#include "Tickable.h"

/** Holds asset dumping related settings */
struct SML_API FAssetDumpSettings {
    FString RootDumpDirectory;
    int32 MaxLoadRequestsInFly;
    int32 MaxPackagesInProcessQueue;
    int32 MaxPackagesToProcessInOneTick;
    bool bForceSingleThread;
    bool bOverwriteExistingAssets;
    bool bExitOnFinish;

    /** Default settings for asset dumping */
    FAssetDumpSettings();
};

/**
 * This class is responsible for processing asset dumping request
 * Only one instance of this class can be active at a time
 * Global active instance of the asset dump processor can be retrieved through GetActiveInstance() method
 */
class SML_API FAssetDumpProcessor : public FTickableGameObject {
private:
    static TSharedPtr<FAssetDumpProcessor> ActiveDumpProcessor;

    TArray<FAssetData> PackagesToLoad;
    TMap<FName, FAssetData*> AssetDataByPackageName;
    int32 CurrentPackageToLoadIndex;

    FThreadSafeCounter PackageLoadRequestsInFlyCounter;

    FCriticalSection LoadedPackagesCriticalSection;
    TArray<UPackage*> LoadedPackages;
    FThreadSafeCounter PackagesWaitingForProcessing;

    int32 PackagesTotal;
    FThreadSafeCounter PackagesSkipped;
    FThreadSafeCounter PackagesProcessed;
    FAssetDumpSettings Settings;
    bool bHasFinishedDumping;

    explicit FAssetDumpProcessor(const FAssetDumpSettings& Settings, const TArray<FAssetData>& InAssets);
    explicit FAssetDumpProcessor(const FAssetDumpSettings& Settings, const TMap<FName, FAssetData>& InAssets);
public:
    ~FAssetDumpProcessor();

    /** Returns currently active instance of the dump processor. Try not to hold any long-living references to it, as it will prevent it's garbage collection */
    FORCEINLINE static TSharedPtr<FAssetDumpProcessor> GetActiveDumpProcessor() { return ActiveDumpProcessor; }

    /** Begins asset dumping with specified settings for provided assets. Crashes when dumping is already in progress */
    static TSharedRef<FAssetDumpProcessor> StartAssetDump(const FAssetDumpSettings& Settings, const TArray<FAssetData>& InAssets);
    static TSharedRef<FAssetDumpProcessor> StartAssetDump(const FAssetDumpSettings& Settings, const TMap<FName, FAssetData>& InAssets);

    /** Returns current progress of the asset dumping */
    FORCEINLINE float GetProgressPct() const { return (PackagesSkipped.GetValue() + PackagesProcessed.GetValue()) / (PackagesTotal * 1.0f); }

    FORCEINLINE int32 GetTotalPackages() const { return PackagesTotal; }
    FORCEINLINE int32 GetPackagesSkipped() const { return PackagesSkipped.GetValue(); }
    FORCEINLINE int32 GetPackagesProcessed() const { return PackagesProcessed.GetValue(); }
    FORCEINLINE bool IsFinishedDumping() const { return bHasFinishedDumping; }

    // Begin FTickableGameObject
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickableWhenPaused() const override;
    // End FTickableGameObject
protected:
    void InitializeAssetDump();
    void OnPackageLoaded(const FName& PackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result);
    void PerformAssetDumpForPackage(UPackage* Package);
};
