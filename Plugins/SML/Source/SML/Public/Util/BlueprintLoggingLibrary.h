#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlueprintLoggingLibrary.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBlueprintLogging, Verbose, All);

#define IMPLEMENT_BP_LOG_FUNCTION(Verbosity) \
    { \
        P_GET_PROPERTY(FStrProperty, Str); \
        P_FINISH; \
        UPackage* OutermostPackage = Stack.Node->GetOutermost(); \
        check(OutermostPackage); \
        UBlueprintLoggingLibrary::LogImpl(ELogVerbosity::Verbosity, OutermostPackage, Str); \
    }

#define BP_FUNCTION_ONLY_STUB() \
    { checkf(0, TEXT("Function can only be called from blueprints")); }

UCLASS()
class SML_API UBlueprintLoggingLibrary : public UBlueprintFunctionLibrary {
    GENERATED_BODY()
public:
    /**
    * Logs the given string in verbose level to the SML Log file and the game log as well as into the console
    */
    UFUNCTION(BlueprintCallable, Category = "SML|Logging", CustomThunk, meta = (BlueprintThreadSafe))
    static void LogVerbose(const FString& Str) BP_FUNCTION_ONLY_STUB();

    /**
    * Logs the given string in info level to the SML Log file and the game log as well as into the console
    */
    UFUNCTION(BlueprintCallable, Category = "SML|Logging", CustomThunk, meta = (BlueprintThreadSafe))
    static void LogDisplay(const FString& Str) BP_FUNCTION_ONLY_STUB();

    /**
    * Logs the given string in warning level to the SML Log file and the game log as well as into the console
    */
    UFUNCTION(BlueprintCallable, Category = "SML|Logging", CustomThunk, meta = (BlueprintThreadSafe))
    static void LogWarning(const FString& Str) BP_FUNCTION_ONLY_STUB();

    /**
    * Logs the given string in error level to the SML Log file and the game log as well as into the console
    */
    UFUNCTION(BlueprintCallable, Category = "SML|Logging", CustomThunk, meta = (BlueprintThreadSafe))
    static void LogError(const FString& Str) BP_FUNCTION_ONLY_STUB();

    /**
    * Logs the given string in fatal level to the SML Log file and the game log as well as into the console
    */
    UFUNCTION(BlueprintCallable, Category = "SML|Logging", CustomThunk, meta = (BlueprintThreadSafe))
    static void LogFatal(const FString& Str) BP_FUNCTION_ONLY_STUB();

private:
    static void LogImpl(ELogVerbosity::Type Verbosity, UPackage* SourcePackage, const FString& Message);

    DECLARE_FUNCTION(execLogVerbose) IMPLEMENT_BP_LOG_FUNCTION(Verbose);
    DECLARE_FUNCTION(execLogDisplay) IMPLEMENT_BP_LOG_FUNCTION(Display);
    DECLARE_FUNCTION(execLogWarning) IMPLEMENT_BP_LOG_FUNCTION(Warning);
    DECLARE_FUNCTION(execLogError) IMPLEMENT_BP_LOG_FUNCTION(Error);
    DECLARE_FUNCTION(execLogFatal) IMPLEMENT_BP_LOG_FUNCTION(Fatal);
};
