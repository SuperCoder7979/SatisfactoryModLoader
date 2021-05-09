#include "Toolkit/AssetDumping/AssetDumpProcessor.h"
#include "Async/ParallelFor.h"
#include "SatisfactoryModLoader.h"
#include "Toolkit/AssetDumping/AssetTypeSerializer.h"
#include "Toolkit/AssetDumping/SerializationContext.h"

using FInlinePackageArray = TArray<UPackage*, TInlineAllocator<16>>;

FAssetDumpSettings::FAssetDumpSettings() :
    RootDumpDirectory(FPaths::ProjectDir() + TEXT("AssetDump/")),
    MaxLoadRequestsInFly(4),
    MaxPackagesInProcessQueue(16),
    MaxPackagesToProcessInOneTick(FPlatformMisc::NumberOfCores() / 2),
    bForceSingleThread(false),
    bOverwriteExistingAssets(true),
    bExitOnFinish(false) {
}

TSharedPtr<FAssetDumpProcessor> FAssetDumpProcessor::ActiveDumpProcessor = NULL;

FAssetDumpProcessor::FAssetDumpProcessor(const FAssetDumpSettings& Settings, const TArray<FAssetData>& InAssets) {
    this->Settings = Settings;
    this->PackagesToLoad = InAssets;
    InitializeAssetDump();
}

FAssetDumpProcessor::FAssetDumpProcessor(const FAssetDumpSettings& Settings, const TMap<FName, FAssetData>& InAssets) {
    this->Settings = Settings;
    InAssets.GenerateValueArray(this->PackagesToLoad);
    InitializeAssetDump();
}

FAssetDumpProcessor::~FAssetDumpProcessor() {
    // Unroot all currently unprocessed UPackages and make sure we have no in-fly package load requests,
    // which will crash trying to call our method upon finishing after we've been destructed
    check(PackageLoadRequestsInFlyCounter.GetValue() == 0);

    for (UPackage* Package : this->LoadedPackages) {
        Package->RemoveFromRoot();
    }

    this->LoadedPackages.Empty();
    this->AssetDataByPackageName.Empty();
    this->PackagesToLoad.Empty();
}

TSharedRef<FAssetDumpProcessor> FAssetDumpProcessor::StartAssetDump(const FAssetDumpSettings& Settings, const TArray<FAssetData>& InAssets) {
    checkf(!ActiveDumpProcessor.IsValid(), TEXT("StartAssetDump is called while another asset dump is in progress"));

    TSharedRef<FAssetDumpProcessor> NewProcessor = MakeShareable(new FAssetDumpProcessor(Settings, InAssets));
    ActiveDumpProcessor = NewProcessor;
    return NewProcessor;
}

TSharedRef<FAssetDumpProcessor> FAssetDumpProcessor::StartAssetDump(const FAssetDumpSettings& Settings, const TMap<FName, FAssetData>& InAssets) {
    checkf(!ActiveDumpProcessor.IsValid(), TEXT("StartAssetDump is called while another asset dump is in progress"));

    TSharedRef<FAssetDumpProcessor> NewProcessor = MakeShareable(new FAssetDumpProcessor(Settings, InAssets));
    ActiveDumpProcessor = NewProcessor;
    return NewProcessor;
}

void FAssetDumpProcessor::Tick(float DeltaTime) {
    // Load packages as long as we have space in queue + packages to process
    while (PackageLoadRequestsInFlyCounter.GetValue() < Settings.MaxLoadRequestsInFly &&
        PackagesWaitingForProcessing.GetValue() < Settings.MaxPackagesInProcessQueue &&
        CurrentPackageToLoadIndex < PackagesToLoad.Num()) {

        // Associate package data with the package name (so we can find it later in async load request handler and increment counter)
        FAssetData* AssetDataToLoadNext = &PackagesToLoad[CurrentPackageToLoadIndex++];
        this->AssetDataByPackageName.Add(AssetDataToLoadNext->PackageName, AssetDataToLoadNext);
        PackageLoadRequestsInFlyCounter.Increment();

        // Start actual async loading of the asset, use our function as handler
        LoadPackageAsync(AssetDataToLoadNext->PackageName.ToString(), FLoadPackageAsyncDelegate::CreateRaw(this, &FAssetDumpProcessor::OnPackageLoaded));
    }

    // Process pending dump requests in parallel for loop
    FInlinePackageArray PackagesToProcessThisTick;
    PackagesToProcessThisTick.Reserve(Settings.MaxPackagesToProcessInOneTick);

    // Lock packages array and copy elements from it
    this->LoadedPackagesCriticalSection.Lock();

    const int32 ElementsToCopy = FMath::Min(LoadedPackages.Num(), Settings.MaxPackagesToProcessInOneTick);
    PackagesToProcessThisTick.Append(LoadedPackages.GetData(), ElementsToCopy);
    LoadedPackages.RemoveAt(0, ElementsToCopy, false);
    PackagesWaitingForProcessing.Subtract(ElementsToCopy);

    this->LoadedPackagesCriticalSection.Unlock();

    FInlinePackageArray PackagesToProcessParallel;
    FInlinePackageArray PackagesToProcessInMainThread;

    for (UPackage* Package : PackagesToProcessThisTick) {
        const FAssetData* AssetData = AssetDataByPackageName.FindChecked(Package->GetFName());
        UAssetTypeSerializer* Serializer = UAssetTypeSerializer::FindSerializerForAssetClass(AssetData->AssetClass);

        if (Serializer->SupportsParallelDumping()) {
            PackagesToProcessParallel.Add(Package);
        } else {
            PackagesToProcessInMainThread.Add(Package);
        }
    }

    if (PackagesToProcessParallel.Num()) {
        EParallelForFlags ParallelFlags = EParallelForFlags::Unbalanced;
        if (Settings.bForceSingleThread) {
            ParallelFlags |= EParallelForFlags::ForceSingleThread;
        }
        ParallelFor(
            PackagesToProcessParallel.Num(),
            [this, &PackagesToProcessParallel](const int32 PackageIndex) {
                PerformAssetDumpForPackage(PackagesToProcessParallel[PackageIndex]);
            },
            ParallelFlags
        );
    }

    if (PackagesToProcessInMainThread.Num()) {
        for (int32 i = 0; i < PackagesToProcessInMainThread.Num(); i++) {
            PerformAssetDumpForPackage(PackagesToProcessInMainThread[i]);
        }
    }

    if (CurrentPackageToLoadIndex >= PackagesToLoad.Num() &&
        PackageLoadRequestsInFlyCounter.GetValue() == 0 &&
        PackagesWaitingForProcessing.GetValue() == 0) {
        UE_LOG(LogSatisfactoryModLoader, Display, TEXT("Asset dumping finished successfully"));
        this->bHasFinishedDumping = true;

        // If we were requested to exit on finish, do it now
        if (Settings.bExitOnFinish) {
            UE_LOG(LogSatisfactoryModLoader, Display, TEXT("Exiting because bExitOnFinish was set to true in asset dumper settings..."));
            FPlatformMisc::RequestExit(false);
        }

        // If we represent an active dump processor, make sure to reset ourselves once dumping is done
        if (ActiveDumpProcessor.Get() == this) {
            ActiveDumpProcessor.Reset();
        }
    }
}

void FAssetDumpProcessor::OnPackageLoaded(const FName& PackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result) {
    // Reduce package load requests in fly counter, so next request can be made
    this->PackageLoadRequestsInFlyCounter.Decrement();

    // Make sure request suceeded
    if (Result != EAsyncLoadingResult::Succeeded) {
        UE_LOG(LogSatisfactoryModLoader, Error, TEXT("Failed to load package %s for dumping. It will be skipped."), *PackageName.ToString());
        PackagesSkipped.Increment();
        return;
    }

    // Add package to the root set so it will not be garbage collected while waiting to be processed
    check(LoadedPackage);
    LoadedPackage->AddToRoot();

    // Add package to loaded ones, using critical section so we avoid concurrency issues
    this->LoadedPackagesCriticalSection.Lock();
    this->LoadedPackages.Add(LoadedPackage);
    this->LoadedPackagesCriticalSection.Unlock();

    // Increment counter for packages in queue, it will prevent main thread from loading more packages if queue is already full
    this->PackagesWaitingForProcessing.Increment();
}

void FAssetDumpProcessor::PerformAssetDumpForPackage(UPackage* Package) {
    FAssetData* AssetData = AssetDataByPackageName.FindChecked(Package->GetFName());
    const TSharedRef<FSerializationContext> Context = MakeShareable(new FSerializationContext(Settings.RootDumpDirectory, *AssetData, Package));

    // Unroot package at this point, we're going to process it this tick anyway, so it doesn't need to be kept anymore
    Package->RemoveFromRoot();

    // Check for existing asset files
    if (!Settings.bOverwriteExistingAssets) {
        const FString AssetOutputFile = Context->GetDumpFilePath(TEXT(""), TEXT("json"));

        // Skip dumping when we have a dump file already and are not allowed to overwrite assets
        if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*AssetOutputFile)) {
            UE_LOG(LogSatisfactoryModLoader, Display, TEXT("Skipping dumping asset %s, dump file is already present and overwriting is not allowed"), *Package->GetName());
            this->PackagesSkipped.Increment();
            return;
        }
    }

    // Find matching serializer, or skip package if we couldn't find one
    UAssetTypeSerializer* Serializer = UAssetTypeSerializer::FindSerializerForAssetClass(AssetData->AssetClass);
    if (Serializer == NULL) {
        UE_LOG(LogSatisfactoryModLoader, Warning, TEXT("Skipping dumping asset %s, failed to find serializer for the associated asset class '%s'"), *Package->GetName(), *AssetData->AssetClass.ToString());
        this->PackagesSkipped.Increment();
        return;
    }

    // Serialize asset, finalize serialization, save data into file and increment processed packages counter
    UE_LOG(LogSatisfactoryModLoader, Display, TEXT("Serializing asset %s (%s)"), *Package->GetName(), *AssetData->AssetClass.ToString());
    Serializer->SerializeAsset(Context);
    Context->Finalize();
    this->PackagesProcessed.Increment();
}

bool FAssetDumpProcessor::IsTickable() const {
    return bHasFinishedDumping == false;
}

TStatId FAssetDumpProcessor::GetStatId() const {
    RETURN_QUICK_DECLARE_CYCLE_STAT(FAssetDumpProcessor, STATGROUP_Game);
}

bool FAssetDumpProcessor::IsTickableWhenPaused() const {
    return true;
}

void FAssetDumpProcessor::InitializeAssetDump() {
    this->Settings = Settings;
    this->CurrentPackageToLoadIndex = 0;
    this->bHasFinishedDumping = false;
    this->PackagesTotal = PackagesToLoad.Num();
    check(PackagesTotal);
    UE_LOG(LogSatisfactoryModLoader, Display, TEXT("Starting asset dump of %d packages..."), PackagesTotal);
}
