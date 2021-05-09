#include "Util/FileDialogHelper.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/ThreadHeartBeat.h"

#pragma warning(push)
#pragma warning(disable: 4668)

#ifdef TEXT
#undef TEXT
#endif

#include "Windows/MinWindows.h"
#include "Windows/COMPointer.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <commdlg.h>
#include <shlobj.h>
#include <LM.h>
#include "Windows/HideWindowsPlatformTypes.h"
#pragma warning(pop)

#ifdef TEXT_PASTE
#define TEXT(x) TEXT_PASTE(x)
#endif

/** Disables all Slate windows while system modal dialog is open, and automatically enables them back on scope exit */
class FScopedSystemModalMode {
public:
    FScopedSystemModalMode() {
        if (FSlateApplication::IsInitialized()) {
            FSlateApplication::Get().ExternalModalStart();
        }
    }

    ~FScopedSystemModalMode() {
        if (FSlateApplication::IsInitialized()) {
            FSlateApplication::Get().ExternalModalStop();
        }
    }
};

// Implementation inspired by DesktopPlatformWindows.cpp, all credit/copyright for original implementation goes to Epic Games
bool FFileDialogHelper::OpenDirectoryDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, FString& OutFolderName) {
    FScopedSystemModalMode SystemModalScope;
    FSlowHeartBeatScope SlowHeartBeatScope;

    TComPtr<IFileOpenDialog> FileDialog;
    if (SUCCEEDED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&FileDialog)))) {
        // Set this dialog as folder picker
        DWORD DialogFlags = 0;
        FileDialog->GetOptions(&DialogFlags);
        FileDialog->SetOptions(DialogFlags | FOS_PICKFOLDERS);

        // Set up title and default path. When it doesn't exist, folder picker will be navigated to user folder by default
        FileDialog->SetTitle(*DialogTitle);
        if (!DefaultPath.IsEmpty()) {
            // SHCreateItemFromParsingName requires absolute path using '\' as a separate and not '/' like unreal normalized paths do
            FString DefaultWindowsPath = FPaths::ConvertRelativePathToFull(DefaultPath);
            DefaultWindowsPath.ReplaceInline(TEXT("/"), TEXT("\\"), ESearchCase::CaseSensitive);

            TComPtr<IShellItem> DefaultPathItem;
            if (SUCCEEDED(::SHCreateItemFromParsingName(*DefaultWindowsPath, nullptr, IID_PPV_ARGS(&DefaultPathItem)))) {
                FileDialog->SetFolder(DefaultPathItem);
            }
        }

        // Show the folder picker dialog
        if (SUCCEEDED(FileDialog->Show((HWND)ParentWindowHandle))) {
            TComPtr<IShellItem> Result;
            if (SUCCEEDED(FileDialog->GetResult(&Result))) {
                PWSTR ResultFilePathStr = nullptr;
                if (SUCCEEDED(Result->GetDisplayName(SIGDN_FILESYSPATH, &ResultFilePathStr))) {
                    OutFolderName = ResultFilePathStr;
                    FPaths::NormalizeDirectoryName(OutFolderName);

                    ::CoTaskMemFree(ResultFilePathStr);
                    return true;
                }
            }
        }
    }
    return false;
}
