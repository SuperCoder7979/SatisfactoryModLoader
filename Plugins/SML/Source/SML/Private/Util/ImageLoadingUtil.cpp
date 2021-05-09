#include "Util/ImageLoadingUtil.h"

#include "Misc/FileHelper.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

UTexture2D* FImageLoadingUtil::LoadImageFromByteArray(const TArray<uint8>& InByteArray, FString& OutErrorMessage) {
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    const EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(InByteArray.GetData(), InByteArray.Num());

    // Malformed image file - unknown image format
    if (ImageFormat == EImageFormat::Invalid) {
        OutErrorMessage = TEXT("Unknown or invalid image format");
        return NULL;
    }

    const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
    // Malformed image file - unexpected data format
    if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(InByteArray.GetData(), InByteArray.Num())) {
        OutErrorMessage = TEXT("Malformed image data (invalid compressed data)");
        return NULL;
    }

    // Data equal to EPixelFormat::PF_B8G8R8A8 used below - BGRA, 8 bits depth
    // Malformed image file - decompression failed
    TArray<uint8> UncompressedBGRA;
    if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBGRA)) {
        OutErrorMessage = TEXT("Malformed image data (decompression failed)");
        return NULL;
    }

    // Create transient texture with size known from image wrapper
    UTexture2D* TextureObject = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), EPixelFormat::PF_B8G8R8A8);
    if (!TextureObject) {
        OutErrorMessage = TEXT("Texture2D object allocation failure");
        return NULL;
    }

    // Add texture to root set so it is not garbage collected
    TextureObject->AddToRoot();
    // Lock initial mip map, copy texture data, and then unlock it
    FTexture2DMipMap& PrimaryMipMap = TextureObject->PlatformData->Mips[0];
    void* TextureDataPtr = PrimaryMipMap.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(TextureDataPtr, UncompressedBGRA.GetData(), UncompressedBGRA.Num());
    PrimaryMipMap.BulkData.Unlock();
    // Update resources to see our changes
    TextureObject->UpdateResource();
    return TextureObject;
}

UTexture2D* FImageLoadingUtil::LoadImageFromFile(const FString& FilePath, FString& OutErrorMessage) {
    TArray<uint8> RawFileContents;
    // Failed to load image from the file
    if (!FFileHelper::LoadFileToArray(RawFileContents, *FilePath)) {
        OutErrorMessage = TEXT("Failed to open image file");
        return NULL;
    }
    return LoadImageFromByteArray(RawFileContents, OutErrorMessage);
}
