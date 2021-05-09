#pragma once
#include "CoreMinimal.h"

/** Describes file stats inside of the zip file */
struct FZipFileStat {
    /** Compressed file size */
    uint64 UncompressedFileSize;

    /** Uncompressed file size */
    uint64 CompressedFileSize;

    /** Modification time of the entry in the zip file */
    int64 ModificationTime;

    /** Crc32 hash of the uncompressed file */
    uint32 FileCrc32;
};

/**
 * A Handle that manages the lifetime of the zip archive and file handle bound to it
 * Archive will be automatically closed upon destructor call, same goes for file handle
 * Primary usage is wrapping it into TSharedPtr
 */
class FZipFile {
private:
    void* ZipArchiveHandle;
    TUniquePtr<class IFileHandle> FileHandle;
    bool InitSuccess;
    TMap<FString, uint32> FileNameToIndex;
public:
    explicit FZipFile(TUniquePtr<IFileHandle> Handle);
    ~FZipFile();
    bool InitArchive();
private:
    // Special file index indicating absence of file in ZIP
    static constexpr uint32 ZIP_NO_FILE_INDEX = (MAX_uint32 - 1);
    uint32 ComputeFileIndex(const ANSICHAR* FileName) const;
    uint32 LocateFileIndex(const FString& FilePath);
public:
    /** Checks if file exists with given path */
    bool FileExists(const FString& FilePath);

    /** Retrieves information about file */
    FZipFileStat StatFile(const FString& FilePath);

    /** Extracts file into the given file handle */
    bool ExtractFile(const FString& FilePath, IFileHandle* OutFileHandle);
    /** Reads entire file into the provided buffer. It should be big enough */
    bool ReadFileToBuffer(const FString& FilePath, void* Buffer, SIZE_T BufferSize);
    /** Reads entire file into the string */
    bool ReadFileToString(const FString& FilePath, FString& OutString);

    /** Returns last error encountered while reading this zip archive */
    FString GetLastZipError() const;

    /**
    * Creates Zip Archive Reader instance from a given file name
    * Will return null pointer if initialization failed, e.g
    * file is missing, corrupted or cannot be opened
    */
    static TSharedPtr<FZipFile> CreateZipArchiveReader(const FString& FilePath, FString& OutErrorMessage);
};
