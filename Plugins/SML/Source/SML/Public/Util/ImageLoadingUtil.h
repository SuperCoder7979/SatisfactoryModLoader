#pragma once
#include "CoreMinimal.h"
#include "Engine/Texture2D.h"

class SML_API FImageLoadingUtil {
public:
    /** Loads image from passed byte array and returns texture object */
    static UTexture2D* LoadImageFromByteArray(const TArray<uint8>& InByteArray, FString& OutErrorMessage);

    /** Loads image from file at the given path */
    static UTexture2D* LoadImageFromFile(const FString& FilePath, FString& OutErrorMessage);
};
