#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlueprintAssetHelperLibrary.generated.h"

UCLASS()
class SML_API UBlueprintAssetHelperLibrary : public UBlueprintFunctionLibrary {
    GENERATED_BODY()
public:
    /** Finds blueprint assets having provided base class as a parent and also having (one of) provided values of the given asset tag */
    UFUNCTION(BlueprintCallable)
    static void FindBlueprintAssetsByTag(UClass* BaseClass, const FName TagName, const TArray<FString>& TagValues, TArray<UClass*>& FoundAssets);

    /** Iterates all loaded native classes and finds these extending the provided one. Will actually include even abstract classes! */
    UFUNCTION(BlueprintCallable)
    static void FindNativeClassesByType(UClass* BaseClass, TArray<UClass*>& FoundClasses);

    /** Tries to determine plugin name owning the provided object path using the fixed mount points */
    UFUNCTION(BlueprintPure)
    static FString FindPluginNameByObjectPath(const FString& ObjectPath, bool bTreatNonModPluginsAsGame = true);
};
