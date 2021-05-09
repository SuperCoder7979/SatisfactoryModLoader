#pragma once
#include "CoreMinimal.h"
#include "Configuration/CodeGeneration/ConfigGenerationContext.h"

struct FReferencedClassesData {
    // Referenced classes, they need includes
    TArray<UStruct*> ReferencedClasses;
    // Referenced generated structs, they need pre-declarations
    TArray<FString> ReferencedGeneratedStructs;
    // If one of the structs used Class field with limited type, e.g TSubclassOf<T> include is needed
    bool bUsedClassPropertyWithLimitedType = false;
};

class SMLEDITOR_API FNativeCodeGenerator {
public:
    static bool GenerateConfigStructForConfigurationAsset(UBlueprint* Blueprint);

    /**
    * Checks if native configuration can be generated from a given context
    * Currently native configuration cannot be generated only if blueprint structs are used
    * inside the configuration structures.
    * Technically we could support it by generating structs with matching layout,
    * but blueprints structs really shouldn't be used if vanilla code generation is used.
    */
    static bool CanGenerateNativeConfiguration(UConfigGenerationContext* Context, FString& OutFailureMessage);

    /**
    * Generates native C++ code header and writes it into the passed output device
    * It will generate separate structure for each generated struct and populate it with fields
    * Additionally it will generate wrapper methods to fill configuration with actual data from configuration manager
    */
    static void GenerateConfigurationCodeHeader(const FString& HeaderFileName, UBlueprint* SourceBlueprint, UConfigGenerationContext* Context, FOutputDevice& OutputDevice);

    /**
     * Generates individual structure and populates it
     * with properties defined in configuration
     */
    static void GenerateConfigStruct(UConfigGeneratedStruct* Struct, UBlueprint* SourceBlueprint, FOutputDevice& OutputDevice);

    static FString GenerateCppTypeForVariable(const FConfigVariableDescriptor& Descriptor);

    /**
     * Generates initial includes and referenced structs pre-declarations
     * Relies on metadata to know exact class paths, so native code generation is only possible
     * inside the editor, because metadata does not exist in shipping
     */
    static void GenerateIncludesAndPredeclarations(const FString& HeaderFileName, const FReferencedClassesData& ReferencedClasses, FOutputDevice& OutputDevice);

    /**
     * Generates list of classes referenced by context
     * For generating corresponding includes and pre-declarations to
     * insert before actually generating code
     */
    static void PopulateReferencedClasses(UConfigGenerationContext* Context, FReferencedClassesData& ReferencedClasses);

    static FString GetIncludePathForClass(UStruct* Class);

    static int GetNestedLevelOfStruct(const UConfigGeneratedStruct* Struct);

    static void AddReferencedVariable(const FConfigVariableDescriptor& Descriptor, FReferencedClassesData& ReferencedClasses);

    static bool IsValidCppIdentifierString(const FString& Identifier);
};
