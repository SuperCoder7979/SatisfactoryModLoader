#pragma once
#include "CoreMinimal.h"

/**
 * Helper for providing platform-agnostic native "choose file/folder/etc" windows
 * Unreal Engine has a built-in module providing this functionality, but it's
 * functionality is limited to Developer builds because for some reason this
 * functionality is strongly coupled with developer-specific functions like UBT/UAT running etc
 * So yeah, this helper provides access to small part of this API even in shipping
 */
class SML_API FFileDialogHelper {
public:
    static bool OpenDirectoryDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, FString& OutFolderName);
};
