#include "Util/BlueprintLoggingLibrary.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY(LogBlueprintLogging);

void UBlueprintLoggingLibrary::LogImpl(ELogVerbosity::Type Verbosity, UPackage* SourcePackage, const FString& Message) {
    FMsg::Logf(UE_LOG_SOURCE_FILE(__FILE__), __LINE__, LogBlueprintLogging.GetCategoryName(), Verbosity, TEXT("[%s]: %s"), *SourcePackage->GetPathName(), *Message);
}
