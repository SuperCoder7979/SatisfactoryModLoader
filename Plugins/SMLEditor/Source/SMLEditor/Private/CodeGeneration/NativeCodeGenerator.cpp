#include "CodeGeneration/NativeCodeGenerator.h"
#include "DesktopPlatformModule.h"
#include "CodeGeneration/UserDefinedStructCodeGenerator.h"
#include "Configuration/ModConfiguration.h"
#include "Engine/UserDefinedStruct.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "UObject/Object.h"
#include "Util/BlueprintAssetHelperLibrary.h"
#define LOCTEXT_NAMESPACE "SML"

bool FNativeCodeGenerator::GenerateConfigStructForConfigurationAsset(UBlueprint* Blueprint) {
    UConfigGenerationContext* GenerationContext;
    if (!FUserDefinedStructCodeGenerator::CreateConfigStructContextForConfigurationAsset(Blueprint, GenerationContext)) {
        return false;
    }

    const FString BlueprintName = Blueprint->GetName();
    const FString BaseStructName = FString::Printf(TEXT("%sStruct"), *BlueprintName);

    FString OutFailureMessage;
    if (!CanGenerateNativeConfiguration(GenerationContext, OutFailureMessage)) {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::Format(
                LOCTEXT(
                    "CannotGenerateNativeConfig",
                    "Cannot generate native header file for configuration blueprint '{0}': {1}."
                ),
                FText::FromString(BlueprintName),
                FText::FromString(OutFailureMessage)
            )
        );
        return false;
    }

    const FString OwnerPluginName = UBlueprintAssetHelperLibrary::FindPluginNameByObjectPath(Blueprint->GetPathName(), false);
    IPluginManager& PluginManager = IPluginManager::Get();
    const TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(OwnerPluginName);

    const FString PluginRootDirectory = FPaths::ConvertRelativePathToFull(Plugin.IsValid() ? Plugin->GetBaseDir() : FPaths::ProjectDir());
    const FString SourceDirectory = FPaths::Combine(PluginRootDirectory, TEXT("Source"));
    const FString SuggestedFilename = FString::Printf(TEXT("%sStruct.h"), *BlueprintName);

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CreateDirectoryTree(*SourceDirectory);

    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
    TArray<FString> SaveFilenames;

    const FText DialogTitle = FText::Format(LOCTEXT("SaveNativeConfigHeader", "Save Configuration '{0}' Struct Header"), FText::FromString(BlueprintName));
    DesktopPlatform->SaveFileDialog(
        ParentWindowHandle,
        DialogTitle.ToString(),
        *SourceDirectory,
        *SuggestedFilename,
        TEXT("C++ Header Files (*.h)|*.h"),
        EFileDialogFlags::None,
        SaveFilenames
    );

    if (SaveFilenames.Num() == 0) {
        FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoFilesSelected", "You need to select a file name you want to use to save the struct header"));
        return false;
    }

    const FString ResultHeaderFilename = SaveFilenames[0];
    FStringOutputDevice StringOutputDevice;
    StringOutputDevice.SetAutoEmitLineTerminator(true);

    const FString CleanHeaderFilename = FPaths::GetBaseFilename(ResultHeaderFilename);
    GenerateConfigurationCodeHeader(CleanHeaderFilename, Blueprint, GenerationContext, StringOutputDevice);

    if (!FFileHelper::SaveStringToFile(StringOutputDevice, *ResultHeaderFilename)) {
        FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("FailedToOpenFile", "Failed to save header file at path '{0}'"), FText::FromString(ResultHeaderFilename)));
        return false;
    }

    FUserDefinedStructCodeGenerator::ShowSuccessNotification(
        FText::Format(
            LOCTEXT(
                "BlueprintConfigStructRegenerated",
                "Successfully generated Native Struct for Configuration '{0}' and saved them at '{1}'"
            ),
            FText::FromString(Blueprint->GetName()),
            FText::FromString(ResultHeaderFilename)
        )
    );
    return true;
}

bool FNativeCodeGenerator::CanGenerateNativeConfiguration(UConfigGenerationContext* Context, FString& OutFailureMessage) {
    for (UConfigGeneratedStruct* GeneratedStruct : Context->GetAllGeneratedStructs()) {

        const FString StructName = GeneratedStruct->GetStructName();
        if (!IsValidCppIdentifierString(StructName)) {
            OutFailureMessage = FString::Printf(TEXT("Generated Struct Name '%s' does not represent a valid C++ identifier. Make sure it contains only alphanumerical characters"), *StructName);
            return false;
        }

        for (const TPair<FString, FConfigVariableDescriptor>& Pair : GeneratedStruct->GetVariables()) {
            const FConfigVariableDescriptor Descriptor = Pair.Value;
            const EConfigVariableType VariableType = Descriptor.GetVariableType();

            if (!IsValidCppIdentifierString(Pair.Key)) {
                OutFailureMessage = FString::Printf(
                    TEXT(
                        "Property '%s' inside of the generated struct named '%s' does not have a valid C++ identifier as it's name. "
                        "Make sure it contains only alphanumerical characters"
                    ),
                    *Pair.Key,
                    *StructName
                );
                return false;
            }

            if (VariableType == EConfigVariableType::ECVT_CustomStruct) {
                UScriptStruct* ScriptStruct = Descriptor.GetCustomStructType();
                if (Cast<UUserDefinedStruct>(ScriptStruct) != NULL) {
                    OutFailureMessage = FString::Printf(
                        TEXT(
                            "User Defined Structure '%s' is referenced by the Config Generated Struct '%s'. "
                            "User defined Structures cannot be referenced from C++ code, use native structs instead."
                        ),
                        *ScriptStruct->GetName(),
                        *StructName
                    );
                    return false;
                }
            }

            if (VariableType == EConfigVariableType::ECVT_Object) {
                UClass* BaseObjectClass = Descriptor.GetBaseObjectClass();
                if (BaseObjectClass != NULL && !BaseObjectClass->IsNative()) {
                    OutFailureMessage = FString::Printf(
                        TEXT(
                            "Blueprint Class '%s' is referenced by the Config Generated Struct '%s'. "
                            "Blueprint classes cannot be referenced from C++ code, use native classes instead."
                        ),
                        *BaseObjectClass->GetName(),
                        *StructName
                    );
                    return false;
                }
            }

            if (VariableType == EConfigVariableType::ECVT_Class) {
                UClass* BaseClassType = Descriptor.GetBaseClassType();
                if (BaseClassType != NULL && !BaseClassType->IsNative()) {
                    OutFailureMessage = FString::Printf(
                        TEXT(
                            "Blueprint Class '%s' is referenced by the Config Generated Struct '%s'. "
                            "Blueprint classes cannot be referenced from C++ code, use native classes instead."
                        ),
                        *BaseClassType->GetName(),
                        *StructName
                    );
                    return false;
                }
            }
        }
    }

    OutFailureMessage = TEXT("");
    return true;
}

void FNativeCodeGenerator::GenerateConfigurationCodeHeader(const FString& HeaderFileName, UBlueprint* SourceBlueprint, UConfigGenerationContext* Context, FOutputDevice& OutputDevice) {
    // Populate referenced classes data required for generating includes
    FReferencedClassesData ReferencedClasses;
    PopulateReferencedClasses(Context, ReferencedClasses);

    // Generate includes and pre-declarations first
    GenerateIncludesAndPredeclarations(HeaderFileName, ReferencedClasses, OutputDevice);

    // Sort structs in the level of their nesting
    TArray<UConfigGeneratedStruct*> GeneratedStructs = Context->GetAllGeneratedStructs();
    GeneratedStructs.StableSort(
        [](const UConfigGeneratedStruct& First, const UConfigGeneratedStruct& Second) {
            const int32 NestedCountFirst = GetNestedLevelOfStruct(&First);
            const int32 NestedCountSecond = GetNestedLevelOfStruct(&Second);
            return NestedCountFirst > NestedCountSecond;
        }
    );

    // Generate each struct now
    for (UConfigGeneratedStruct* GeneratedStruct : GeneratedStructs) {
        GenerateConfigStruct(GeneratedStruct, SourceBlueprint, OutputDevice);

        // Insert blank line after each struct for better formatting
        OutputDevice.Log(TEXT(""));
    }
}

void FNativeCodeGenerator::GenerateConfigStruct(UConfigGeneratedStruct* Struct, UBlueprint* SourceBlueprint, FOutputDevice& OutputDevice) {
    const FString BlueprintName = SourceBlueprint->GetName();
    const FString BaseStructName = FString::Printf(TEXT("%sStruct"), *BlueprintName);
    const bool bIsRootStruct = Struct->GetStructName() == BaseStructName;

    // Append command listing source asset to the root struct header
    if (bIsRootStruct) {
        const FString BlueprintAssetPath = SourceBlueprint->GetOutermost()->GetName();
        OutputDevice.Logf(TEXT("/* Struct generated from Mod Configuration Asset '%s' */"), *BlueprintAssetPath);
    }

    // Open struct body, mark it as USTRUCT accessible in blueprints
    OutputDevice.Log(TEXT("USTRUCT(BlueprintType)"));
    OutputDevice.Logf(TEXT("struct F%s {"), *Struct->GetStructName());
    OutputDevice.Log(TEXT("    GENERATED_BODY()"));
    OutputDevice.Log(TEXT("public:"));

    bool bIsFirstField = true;

    // Generate each variable now
    for (const TPair<FString, FConfigVariableDescriptor>& Pair : Struct->GetVariables()) {
        // Append empty line between properties unless it's a first property
        if (!bIsFirstField) {
            OutputDevice.Log(TEXT(""));
        } else {
            bIsFirstField = false;
        }

        // Determine C++ type for passed descriptor
        const FString CppTypeForDescriptor = GenerateCppTypeForVariable(Pair.Value);
        // Mark variable as BlueprintReadWrite for it to be accessible via BP limited reflection
        OutputDevice.Logf(TEXT("    UPROPERTY(BlueprintReadWrite)"));
        OutputDevice.Logf(TEXT("    %s %s;"), *CppTypeForDescriptor, *Pair.Key);
    }

    // Append method for retrieving configuration value if we're working with root struct
    if (bIsRootStruct && SourceBlueprint->GeneratedClass) {
        UModConfiguration* ModConfiguration = CastChecked<UModConfiguration>(SourceBlueprint->GeneratedClass.GetDefaultObject());
        const FConfigId& ConfigId = ModConfiguration->ConfigId;

        OutputDevice.Log(TEXT(""));
        OutputDevice.Logf(TEXT("    /* Retrieves active configuration value and returns object of this struct containing it */"));
        OutputDevice.Logf(TEXT("    static F%s GetActiveConfig() {"), *Struct->GetStructName());
        OutputDevice.Logf(TEXT("        F%s ConfigStruct{};"), *Struct->GetStructName());
        OutputDevice.Logf(TEXT("        FConfigId ConfigId{\"%s\", \"%s\"};"), *ConfigId.ModReference.ReplaceQuotesWithEscapedQuotes(), *ConfigId.ConfigCategory.ReplaceQuotesWithEscapedQuotes());
        OutputDevice.Logf(TEXT("        UConfigManager* ConfigManager = GEngine->GetEngineSubsystem<UConfigManager>();"));
        OutputDevice.Logf(TEXT("        ConfigManager->FillConfigurationStruct(ConfigId, FDynamicStructInfo{F%s::StaticStruct(), &ConfigStruct});"), *Struct->GetStructName());
        OutputDevice.Logf(TEXT("        return ConfigStruct;"));
        OutputDevice.Logf(TEXT("    }"));
    }

    // Terminate struct body with closing bracket
    OutputDevice.Log(TEXT("};"));
}

FString FNativeCodeGenerator::GenerateCppTypeForVariable(const FConfigVariableDescriptor& Descriptor) {
    const EConfigVariableType VariableType = Descriptor.GetVariableType();
    if (VariableType == EConfigVariableType::ECVT_Int32) {
        return TEXT("int32");
    }
    if (VariableType == EConfigVariableType::ECVT_Int64) {
        return TEXT("int64");
    }
    if (VariableType == EConfigVariableType::ECVT_Float) {
        return TEXT("float");
    }
    if (VariableType == EConfigVariableType::ECVT_String) {
        return TEXT("FString");
    }
    if (VariableType == EConfigVariableType::ECVT_Bool) {
        return TEXT("bool");
    }
    if (VariableType == EConfigVariableType::ECVT_Array) {
        const FString InnerType = GenerateCppTypeForVariable(Descriptor.GetArrayElementDescriptor());
        return FString::Printf(TEXT("TArray<%s>"), *InnerType);
    }
    if (VariableType == EConfigVariableType::ECVT_Map) {
        const FString KeyType = GenerateCppTypeForVariable(Descriptor.GetMapKeyDescriptor());
        const FString ValueType = GenerateCppTypeForVariable(Descriptor.GetMapValueDescriptor());
        return FString::Printf(TEXT("TMap<%s, %s>"), *KeyType, *ValueType);
    }
    if (VariableType == EConfigVariableType::ECVT_ConfigGeneratedStruct) {
        UConfigGeneratedStruct* GeneratedStruct = Descriptor.GetConfigGeneratedStruct();
        const FString StructName = GeneratedStruct ? GeneratedStruct->GetStructName() : TEXT("Unknown");
        return FString::Printf(TEXT("F%s"), *StructName);
    }
    if (VariableType == EConfigVariableType::ECVT_CustomStruct) {
        UScriptStruct* CustomStruct = Descriptor.GetCustomStructType();
        if (CustomStruct == NULL) {
            CustomStruct = TBaseStructure<FLinearColor>::Get();
        }
        return FString::Printf(TEXT("%s%s"), CustomStruct->GetPrefixCPP(), *CustomStruct->GetName());
    }
    if (VariableType == EConfigVariableType::ECVT_Object) {
        UClass* BaseObjectClass = Descriptor.GetBaseObjectClass();
        if (BaseObjectClass == NULL) {
            BaseObjectClass = UObject::StaticClass();
        }
        const FString ClassName = FString::Printf(TEXT("%s%s"), BaseObjectClass->GetPrefixCPP(), *BaseObjectClass->GetName());
        return FString::Printf(TEXT("%s*"), *ClassName);
    }
    if (VariableType == EConfigVariableType::ECVT_Class) {
        UClass* BaseClassType = Descriptor.GetBaseClassType();
        if (BaseClassType == NULL || BaseClassType == UClass::StaticClass()) {
            // No lower limit is set or it is set to bare UClass, in which case we don't need TSubclassOf
            // And can just use raw UClass object pointer
            return TEXT("UClass*");
        }
        const FString ClassName = FString::Printf(TEXT("%s%s"), BaseClassType->GetPrefixCPP(), *BaseClassType->GetName());
        return FString::Printf(TEXT("TSubclassOf<%s>"), *ClassName);
    }
    checkf(false, TEXT("Unrechable code"));
    return TEXT("");
}

void FNativeCodeGenerator::GenerateIncludesAndPredeclarations(const FString& HeaderFileName, const FReferencedClassesData& ReferencedClasses, FOutputDevice& OutputDevice) {
    // Include pragma once and all the includes referenced
    OutputDevice.Log(TEXT("#pragma once"));
    OutputDevice.Log(TEXT("#include \"CoreMinimal.h\""));
    if (ReferencedClasses.bUsedClassPropertyWithLimitedType) {
        OutputDevice.Log(TEXT("#include \"SubclassOf.h\""));
    }
    TArray<FString> AllIncludePaths;
    for (UStruct* ReferencedClass : ReferencedClasses.ReferencedClasses) {
        FString IncludePath = GetIncludePathForClass(ReferencedClass);
        // We can get duplicates since multiple objects can be defined in one header file
        AllIncludePaths.AddUnique(IncludePath);
    }
    for (const FString& IncludePath : AllIncludePaths) {
        OutputDevice.Logf(TEXT("#include \"%s\""), *IncludePath);
    }

    // Include ConfigManager and GEngine for retrieving it
    OutputDevice.Log(TEXT("#include \"Configuration/ConfigManager.h\""));
    OutputDevice.Log(TEXT("#include \"Engine/Engine.h\""));

    // Generated include should always be the last include in the file
    OutputDevice.Logf(TEXT("#include \"%s.generated.h\""), *HeaderFileName);

    // Include an empty line between includes and pre-declarations
    OutputDevice.Log(TEXT(""));

    // Include pre-declarations
    for (const FString& ConfigStruct : ReferencedClasses.ReferencedGeneratedStructs) {
        OutputDevice.Logf(TEXT("struct F%s;"), *ConfigStruct);
    }

    // Include empty line after structs pre-declarations
    if (ReferencedClasses.ReferencedGeneratedStructs.Num() > 0) {
        OutputDevice.Log(TEXT(""));
    }
}

void FNativeCodeGenerator::PopulateReferencedClasses(UConfigGenerationContext* Context, FReferencedClassesData& ReferencedClasses) {
    for (UConfigGeneratedStruct* GeneratedStruct : Context->GetAllGeneratedStructs()) {
        for (const TPair<FString, FConfigVariableDescriptor>& Pair : GeneratedStruct->GetVariables()) {
            const FConfigVariableDescriptor& VariableDescriptor = Pair.Value;
            AddReferencedVariable(VariableDescriptor, ReferencedClasses);
        }
    }
}

FString FNativeCodeGenerator::GetIncludePathForClass(UStruct* Class) {
    // We cannot use "IncludePath" metadata attribute here because it's not added for UScriptStruct,
    // and object we have as argument can be either UClass or UScriptStruct, or maybe even UEnum if we decide to
    // support them at some point. However, "ModuleRelativePath" is present on all of these objects,
    // and even on function and property objects. According to UHT source, only difference
    // between these two is that include path has Public/Private/Classes prefixes stripped. We can
    // mimic that behavior and get uniform include paths for all defined objects

    FString IncludePath = Class->GetMetaData(TEXT("ModuleRelativePath"));
    checkf(!IncludePath.IsEmpty(), TEXT("ModuleRelativePath metadata not found on object %s"), *Class->GetPathName());
    // Walk over the first potential slash
    if (IncludePath.StartsWith(TEXT("/"))) {
        IncludePath.RemoveAt(0);
    }
    // Does this module path start with a known include path location? If so, we can cut that part out of the include path
    static const TCHAR PublicFolderName[] = TEXT("Public/");
    static const TCHAR PrivateFolderName[] = TEXT("Private/");
    static const TCHAR ClassesFolderName[] = TEXT("Classes/");
    if (IncludePath.StartsWith(PublicFolderName)) {
        IncludePath.RemoveAt(0, ARRAY_COUNT(PublicFolderName) - 1);
    }
    if (IncludePath.StartsWith(PrivateFolderName)) {
        IncludePath.RemoveAt(0, ARRAY_COUNT(PrivateFolderName) - 1);
    }
    if (IncludePath.StartsWith(ClassesFolderName)) {
        IncludePath.RemoveAt(0, ARRAY_COUNT(ClassesFolderName) - 1);
    }
    return IncludePath;
}

int FNativeCodeGenerator::GetNestedLevelOfStruct(const UConfigGeneratedStruct* Struct) {
    const FString& StructName = Struct->GetStructName();
    int32 NumUnderscores = 0;
    for (int32 i = 0; i < StructName.Len(); i++) {
        if (StructName[i] == TEXT('_')) NumUnderscores++;
    }
    return NumUnderscores;
}

void FNativeCodeGenerator::AddReferencedVariable(const FConfigVariableDescriptor& Descriptor, FReferencedClassesData& ReferencedClasses) {
    const EConfigVariableType VariableType = Descriptor.GetVariableType();
    if (VariableType == EConfigVariableType::ECVT_ConfigGeneratedStruct) {
        // Config generated struct needs pre-declaration to be referenced
        UConfigGeneratedStruct* GeneratedStruct = Descriptor.GetConfigGeneratedStruct();
        if (GeneratedStruct != NULL) {
            ReferencedClasses.ReferencedGeneratedStructs.Add(GeneratedStruct->GetStructName());
        }
    } else if (VariableType == EConfigVariableType::ECVT_CustomStruct) {
        // Custom structure needs import too, unless it is BP struct, but we shouldn't encounter BP structs
        UScriptStruct* ReferencedStruct = Descriptor.GetCustomStructType();
        if (ReferencedStruct != NULL) {
            checkf(Cast<UUserDefinedStruct>(ReferencedStruct) == NULL, TEXT("User defined structs are not supported"));
            ReferencedClasses.ReferencedClasses.Add(ReferencedStruct);
        }
    } else if (VariableType == EConfigVariableType::ECVT_Object) {
        UClass* BaseObjectClass = Descriptor.GetBaseObjectClass();
        if (BaseObjectClass != NULL) {
            checkf(BaseObjectClass->IsNative(), TEXT("Non-native classes are not supported"));
            ReferencedClasses.ReferencedClasses.Add(BaseObjectClass);
        }
    } else if (VariableType == EConfigVariableType::ECVT_Class) {
        UClass* BaseClassType = Descriptor.GetBaseClassType();
        if (BaseClassType != NULL) {
            checkf(BaseClassType->IsNative(), TEXT("Non-native classes are not supported"));
            ReferencedClasses.ReferencedClasses.Add(BaseClassType);
        }
        if (BaseClassType != UClass::StaticClass()) {
            ReferencedClasses.bUsedClassPropertyWithLimitedType = true;
        }
    }
}

bool FNativeCodeGenerator::IsValidCppIdentifierString(const FString& Identifier) {
    for (int32 i = 0; i < Identifier.Len(); i++) {
        if (!IsValidCPPIdentifierChar(Identifier[i])) {
            return false;
        }
    }
    return Identifier.Len() > 0;
}
