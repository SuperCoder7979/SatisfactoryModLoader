#pragma once
#include "CoreMinimal.h"
#include "Configuration/CodeGeneration/ConfigGeneratedStruct.h"
#include "EdGraph/EdGraphPin.h"

class SMLEDITOR_API FUserDefinedStructCodeGenerator {
public:
    static bool GenerateConfigStructForConfigurationAsset(class UBlueprint* Blueprint);

    static bool CreateConfigStructContextForConfigurationAsset(UBlueprint* Blueprint, UConfigGenerationContext*& OutGenerationContext);

    static void ShowSuccessNotification(const FText& NotificationText);

    /**
     * Generates user defined configuration struct inside of the given package name
     * From the provided context, and returns boolean indicating whenever it was successful or failure
     */
    static bool GenerateConfigurationStruct(const FString& PackageName, UConfigGenerationContext* Context);

    // Deletes all struct members and creates one dummy boolean member instead
    static void ClearUserDefinedStructContents(class UUserDefinedStruct* Struct);

    static bool SpawnStructVariableWithFixedName(UUserDefinedStruct* Struct, const FEdGraphPinType& VariableType, const FString& Name);

    // Populates generated struct with variable declarations
    static void PopulateGeneratedStruct
    (
        UConfigGeneratedStruct* GeneratedStruct,
        class UUserDefinedStruct* UserStruct,
        const TMap<UConfigGeneratedStruct*, UUserDefinedStruct*>& GeneratedStructs
    );

    // Creates blueprint variable type for descriptor
    static FEdGraphPinType CreatePinTypeForVariable(const FConfigVariableDescriptor& Descriptor, const TMap<UConfigGeneratedStruct*, UUserDefinedStruct*>& GeneratedStructs);
};
