#pragma once
#include "Containers/UnrealString.h"

class SMLEDITOR_API FGameAssetGenerator {
public:
    /**
     * Generates Satisfactory Data Assets from FG Data Dump Json File
     * WARNING! It takes some time, happens in multiple phases and is prone to multiple errors
     * Only call if you know what you're doing
     *
     * Example Usage:
     * SML::generateSatisfactoryAssets(TEXT("D:/SatisfactoryExperimental/configs/FGDataAssets.json"));
     */
    static void GenerateSatisfactoryAssets(const FString& DataJsonFilePath);
};
