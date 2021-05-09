#include "Toolkit/AssetTypes/TextureDecompressor.h"
#include "Math/PackedVector.h"
#include "RenderUtils.h"
#include "detex.h"

void ConvertFloatRGBAToBGRA8(const void* SourcePixelData, void* DestPixelData, int32 NumPixels) {
    const FFloat16Color* SourceData = static_cast<const FFloat16Color*>(SourcePixelData);
    FColor* DestData = static_cast<FColor*>(DestPixelData);

    for (int i = 0; i < NumPixels; i++) {
        const FFloat16Color* CurrentColorFloat = SourceData++;
        FColor* CurrentColor = DestData++;
        FLinearColor LinearColor(
            CurrentColorFloat->R.GetFloat(),
            CurrentColorFloat->G.GetFloat(),
            CurrentColorFloat->B.GetFloat(),
            CurrentColorFloat->A.GetFloat()
        );
        *CurrentColor = LinearColor.ToFColor(true);
    }
}

void ConvertGrayscale8ToBGRA8(const void* SourcePixelData, void* DestPixelData, int32 NumPixels) {
    const uint8* SourceData = static_cast<const uint8*>(SourcePixelData);
    FColor* DestData = static_cast<FColor*>(DestPixelData);

    for (int i = 0; i < NumPixels; i++) {
        const uint8* CurrentColorGray = SourceData++;
        FColor* CurrentColor = DestData++;
        CurrentColor->R = *CurrentColorGray;
        CurrentColor->G = *CurrentColorGray;
        CurrentColor->B = *CurrentColorGray;
        CurrentColor->A = 255;
    }
}

// TODO this path has never been tested, i'm not sure whenever we actually need to apply sRGB color space conversion here
void ConvertFloatR11G11B10ToBGRA8(const void* SourcePixelData, void* DestPixelData, int32 NumPixels) {
    const FFloat3Packed* SourceData = static_cast<const FFloat3Packed*>(SourcePixelData);
    FColor* DestData = static_cast<FColor*>(DestPixelData);

    for (int i = 0; i < NumPixels; i++) {
        const FFloat3Packed* CurrentColorFloat = SourceData++;
        const FColor SourceColor = CurrentColorFloat->ToLinearColor().ToFColor(true);
        FColor* CurrentColor = DestData++;
        *CurrentColor = SourceColor;
    }
}

bool FTextureDecompressor::DecompressTextureData(EPixelFormat PixelFormat, const uint8* CompressedData, int32 TextureWidth, int32 TextureHeight, TArray<uint8>& OutDecompressedData, FString* OutErrorMessage) {

    uint32 SourceTextureFormat = 0;
    bool bDecompressionNeeded = true;

    // See D3DDevice for relation between EPixelFormat and Detex/D3D internal formats
    switch (PixelFormat) {
    case EPixelFormat::PF_DXT1: SourceTextureFormat = DETEX_TEXTURE_FORMAT_BC1;
        break;
    case EPixelFormat::PF_DXT3: SourceTextureFormat = DETEX_TEXTURE_FORMAT_BC2;
        break;
    case EPixelFormat::PF_DXT5: SourceTextureFormat = DETEX_TEXTURE_FORMAT_BC3;
        break;
    case EPixelFormat::PF_BC4: SourceTextureFormat = DETEX_TEXTURE_FORMAT_RGTC1;
        break;
    case EPixelFormat::PF_BC5: SourceTextureFormat = DETEX_TEXTURE_FORMAT_RGTC2;
        break;
    case EPixelFormat::PF_BC6H: SourceTextureFormat = DETEX_TEXTURE_FORMAT_BPTC_FLOAT;
        break;
    case EPixelFormat::PF_BC7: SourceTextureFormat = DETEX_TEXTURE_FORMAT_BPTC;
        break;
    default: bDecompressionNeeded = false;
        break;
    }

    // C doesn't support const, so we need to cast const-ness away
    uint8* SourceData = const_cast<uint8*>(CompressedData);
    const int32 NumPixels = TextureWidth * TextureHeight;

    // Reserve enough space in decompressed data array (we use BGRA8, so 4 channels and 8 bits for channel)
    const int32 DataOffset = OutDecompressedData.AddUninitialized(NumPixels * 4);
    uint8* DestData = &OutDecompressedData[DataOffset];
    const uint32 TargetPixelFormat = DETEX_PIXEL_FORMAT_BGRA8;
    bool bSuccess;

    if (bDecompressionNeeded) {
        // Construct compressed detex texture
        detexTexture DetexTexture;
        DetexTexture.data = SourceData;
        DetexTexture.format = SourceTextureFormat;
        DetexTexture.height = TextureHeight;
        DetexTexture.width = TextureWidth;

        // Use GPixelFormats to retrieve width in blocks
        const FPixelFormatInfo& PixelFormatInfo = GPixelFormats[PixelFormat];
        DetexTexture.width_in_blocks = TextureWidth / PixelFormatInfo.BlockSizeX;
        DetexTexture.height_in_blocks = TextureHeight / PixelFormatInfo.BlockSizeY;

        // Perform texture decompression now
        bSuccess = detexDecompressTextureLinear(&DetexTexture, DestData, TargetPixelFormat);

    } else {
        // No need to decompress, but we might need to convert pixels into right format
        if (PixelFormat == EPixelFormat::PF_B8G8R8A8) {
            // No conversion is needed - copy data directly
            FPlatformMemory::Memcpy(DestData, SourceData, NumPixels * 4);

        } else if (PixelFormat == EPixelFormat::PF_G8) {
            // Convert grayscale 8-bit image to gray BGRA8 image
            ConvertGrayscale8ToBGRA8(SourceData, DestData, NumPixels);

        } else if (PixelFormat == EPixelFormat::PF_FloatRGBA) {
            // Convert 16-bit FloatRGBA image to BGRA8 image
            ConvertFloatRGBAToBGRA8(SourceData, DestData, NumPixels);

        } else if (PixelFormat == EPixelFormat::PF_FloatRGB || PixelFormat == EPixelFormat::PF_FloatR11G11B10) {
            // Convert that weird float low-precision format that nobody is using to BGRA8 image
            ConvertFloatR11G11B10ToBGRA8(SourceData, DestData, NumPixels);

        } else {
            // Well, this format is not supported apparently
            if (OutErrorMessage) {
                *OutErrorMessage = TEXT("Unsupported pixel format");
            }
            return false;
        }

        // Usually conversion is successful when we reach this statement
        // Else statement will handle failure itself
        return true;
    }

    // Populate error message from detex if user provided pointer to set
    if (!bSuccess && OutErrorMessage) {
        const FString DetexErrorMessage = detexGetErrorMessage();
        *OutErrorMessage = FString::Printf(TEXT("detex returned error: %s"), *DetexErrorMessage);
    }
    return bSuccess;
}
