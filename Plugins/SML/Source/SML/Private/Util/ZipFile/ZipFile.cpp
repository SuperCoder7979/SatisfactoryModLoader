#include "Util/ZipFile/ZipFile.h"

#include "Misc/FileHelper.h"
#include "miniz.h"
#include "HAL/PlatformFilemanager.h"

#define ZipArchive static_cast<mz_zip_archive*>(ZipArchiveHandle)

size_t ReadZipArchiveFunc(void* Opaque, mz_uint64 FileOffset, void* ReadBuffer, size_t Amount) {
    IFileHandle* FileHandle = static_cast<IFileHandle*>(Opaque);
    const int64 BytesToRead = static_cast<int64>(Amount);
    const int64 RequiredOffset = static_cast<int64>(FileOffset);
    const int64 CurrentOffset = FileHandle->Tell();
    if (CurrentOffset != RequiredOffset) FileHandle->Seek(RequiredOffset);
    // Try to read with suggested amount first
    if (FileHandle->Read(static_cast<uint8*>(ReadBuffer), BytesToRead)) {
        return Amount;
    }
    // Not enough bytes to actually read, try to read Size - Offset
    const int64 MaxBytesToRead = FMath::Min(FileHandle->Size() - FileHandle->Tell(), BytesToRead);
    if (FileHandle->Read(static_cast<uint8*>(ReadBuffer), MaxBytesToRead)) {
        return static_cast<size_t>(MaxBytesToRead);
    }
    // cannot read anything, return 0
    return 0;
}

size_t ExtractZipArchiveFunc(void* Opaque, mz_uint64 FileOffset, const void* WriteBuffer, size_t WriteAmount) {
    IFileHandle* FileHandle = static_cast<IFileHandle*>(Opaque);
    const int64 BytesToWrite = static_cast<int64>(WriteAmount);
    const int64 RequiredOffset = static_cast<int64>(FileOffset);
    const int64 CurrentOffset = FileHandle->Tell();
    if (CurrentOffset != RequiredOffset) FileHandle->Seek(RequiredOffset);
    // Just write required amount
    const bool Result = FileHandle->Write(static_cast<const uint8*>(WriteBuffer), BytesToWrite);
    return Result ? WriteAmount : 0;
}

FZipFile::FZipFile(TUniquePtr<IFileHandle> Handle) : FileHandle(std::move(Handle)), InitSuccess(false) {
    const SIZE_T ZipStructSize = sizeof(mz_zip_archive);
    this->ZipArchiveHandle = FMemory::Malloc(ZipStructSize);
    FMemory::Memzero(ZipArchiveHandle, ZipStructSize);

    ZipArchive->m_pIO_opaque = FileHandle.Get();
    ZipArchive->m_pRead = &ReadZipArchiveFunc;
}

FZipFile::~FZipFile() {
    if (InitSuccess) {
        mz_zip_reader_end(ZipArchive);
    }
    FMemory::Free(this->ZipArchiveHandle);
    this->ZipArchiveHandle = NULL;
}

bool FZipFile::InitArchive() {
    InitSuccess = static_cast<bool>(mz_zip_reader_init(ZipArchive, FileHandle->Size(), 0));
    return InitSuccess;
}

uint32 FZipFile::ComputeFileIndex(const ANSICHAR* FileName) const {
    uint32 ResultFileIndex;
    const bool Result = static_cast<bool>(mz_zip_reader_locate_file_v2(ZipArchive, FileName, NULL, 0, &ResultFileIndex));
    return Result ? ResultFileIndex : ZIP_NO_FILE_INDEX;
}

uint32 FZipFile::LocateFileIndex(const FString& FilePath) {
    uint32* ExistingIndex = FileNameToIndex.Find(FilePath);
    if (ExistingIndex) return *ExistingIndex;
    const uint32 NewIndex = ComputeFileIndex(TCHAR_TO_ANSI(*FilePath));
    FileNameToIndex.Add(FilePath, NewIndex);
    return NewIndex;
}

bool FZipFile::FileExists(const FString& FilePath) {
    return LocateFileIndex(FilePath) != ZIP_NO_FILE_INDEX;
}

FZipFileStat FZipFile::StatFile(const FString& FilePath) {
    mz_zip_archive_file_stat FileStat{};
    const uint32 FileIndex = LocateFileIndex(FilePath);
    if (FileIndex != ZIP_NO_FILE_INDEX) mz_zip_reader_file_stat(ZipArchive, FileIndex, &FileStat);
    return FZipFileStat{FileStat.m_uncomp_size, FileStat.m_comp_size, FileStat.m_time, FileStat.m_crc32};
}

bool FZipFile::ExtractFile(const FString& FilePath, IFileHandle* OutFileHandle) {
    const uint32 FileIndex = LocateFileIndex(FilePath);
    if (FileIndex == ZIP_NO_FILE_INDEX) return false;
    const bool Result = static_cast<bool>(mz_zip_reader_extract_to_callback(ZipArchive, FileIndex, &ExtractZipArchiveFunc, OutFileHandle, 0));
    return Result && OutFileHandle->Flush();
}

bool FZipFile::ReadFileToBuffer(const FString& FilePath, void* Buffer, SIZE_T BufferSize) {
    const uint32 FileIndex = LocateFileIndex(FilePath);
    if (FileIndex == ZIP_NO_FILE_INDEX) return false;
    return static_cast<bool>(mz_zip_reader_extract_to_mem(ZipArchive, FileIndex, Buffer, BufferSize, 0));
}

bool FZipFile::ReadFileToString(const FString& FilePath, FString& OutString) {
    const FZipFileStat FileStat = StatFile(FilePath);
    if (FileStat.UncompressedFileSize == 0) return false; // file doesn't exist
    // Create buffer
    const SIZE_T BufferSize = FileStat.UncompressedFileSize;
    void* ExtractBuffer = FMemory::Malloc(BufferSize);
    FMemory::Memzero(ExtractBuffer, BufferSize);
    // Read file into buffer, then convert it to string
    const bool Success = ReadFileToBuffer(FilePath, ExtractBuffer, BufferSize);
    if (Success) {
        FFileHelper::BufferToString(OutString, static_cast<uint8*>(ExtractBuffer), BufferSize);
    }
    FMemory::Free(ExtractBuffer);
    return Success;
}

FString FZipFile::GetLastZipError() const {
    const mz_zip_error LastErrorNumber = mz_zip_get_last_error(ZipArchive);
    const char* ErrorString = mz_zip_get_error_string(LastErrorNumber);
    return FString(ErrorString);
}

TSharedPtr<FZipFile> FZipFile::CreateZipArchiveReader(const FString& FilePath, FString& OutErrorMessage) {
    TUniquePtr<IFileHandle> FileHandle = TUniquePtr<IFileHandle>(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*FilePath));
    if (FileHandle == nullptr) {
        OutErrorMessage = FString::Printf(TEXT("Cannot open source file at %s"), *FilePath);
        return nullptr;
    }
    TSharedRef<FZipFile> ZipHandle = MakeShareable(new FZipFile(std::move(FileHandle)));
    if (!ZipHandle->InitArchive()) {
        const FString LastError = ZipHandle->GetLastZipError();
        OutErrorMessage = FString::Printf(TEXT("Corrupted zip file (%s)"), *LastError);
        return nullptr;
    }
    return ZipHandle;
}
