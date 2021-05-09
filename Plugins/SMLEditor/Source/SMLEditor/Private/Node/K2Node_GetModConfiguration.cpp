#include "Node/K2Node_GetModConfiguration.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_TemporaryVariable.h"
#include "KismetCompiler.h"
#include "KismetCompilerMisc.h"
#include "PackageTools.h"
#include "Configuration/ConfigManager.h"
#include "Engine/UserDefinedStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Subsystems/SubsystemBlueprintLibrary.h"
#include "Util/BlueprintAssetHelperLibrary.h"
#define LOCTEXT_NAMESPACE "SML"

static FName GetModConfiguration_OutputPinName(TEXT("Config"));

void UK2Node_GetModConfiguration::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const {
    // Only try to register when we are actually interested in our node type
    // Because asset registry lookup is rather expensive, we want to skip doing it for no reason
    UClass* ActionKey = GetClass();
    if (ActionRegistrar.IsOpenForRegistration(ActionKey)) {

        // Query asset registry for a list of valid native configuration classes
        TArray<UClass*> ValidConfigurationClasses;
        RetrieveAllConfigurationClasses(ValidConfigurationClasses);

        auto CustomizeCallback = [](UEdGraphNode* Node, bool bIsTemplateNode, UClass* ConfigurationClass) {
            UK2Node_GetModConfiguration* TypedNode = CastChecked<UK2Node_GetModConfiguration>(Node);
            TypedNode->ModConfigurationClass = ConfigurationClass;
        };

        for (UClass* ValidConfigurationClass : ValidConfigurationClasses) {
            UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(ActionKey);
            check(Spawner);
            Spawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeCallback, ValidConfigurationClass);
            ActionRegistrar.AddBlueprintAction(ActionKey, Spawner);
        }
    }
}

FText UK2Node_GetModConfiguration::GetMenuCategory() const {
    return FEditorCategoryUtils::BuildCategoryString(FCommonEditorCategory::Utilities, LOCTEXT("ConfigSubCategory", "Configuration"));
}

FLinearColor UK2Node_GetModConfiguration::GetNodeTitleColor() const {
    return FLinearColor{0.8f, 0.1f, 0.8f};
}

UScriptStruct* UK2Node_GetModConfiguration::GetConfigurationStruct() const {
    return ResolveConfigurationStruct(ModConfigurationClass);
}

void UK2Node_GetModConfiguration::AllocateDefaultPins() {
    // Allocate Exec input/outputs pins for this node
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

    // Allocate struct output node when we have valid configuration struct resolved
    UScriptStruct* ConfigStruct = GetConfigurationStruct();
    if (ConfigStruct != NULL) {
        CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, ConfigStruct, GetModConfiguration_OutputPinName);
    }
}

FNodeHandlingFunctor* UK2Node_GetModConfiguration::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const {
    return new FNodeHandlingFunctor(CompilerContext);
}

void UK2Node_GetModConfiguration::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) {
    Super::ExpandNode(CompilerContext, SourceGraph);

    UEdGraphPin* InputExecPin = FindPinChecked(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
    UEdGraphPin* OutputThenExecPin = FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
    UEdGraphPin* StructOutputPin = GetOutputConfigStructPin();

    if (StructOutputPin == NULL) {
        CompilerContext.MessageLog.Error(*LOCTEXT("InvalidConfigClass", "Node @@ does not reference a valid configuration struct").ToString(), this);
        return;
    }

    // Allocate node for obtaining engine subsystem of UConfigManager
    UK2Node_CallFunction* GetConfigManagerNode = SourceGraph->CreateIntermediateNode<UK2Node_CallFunction>();
    const FName GetEngineSubsystemFuncName = GET_FUNCTION_NAME_CHECKED(USubsystemBlueprintLibrary, GetEngineSubsystem);
    GetConfigManagerNode->FunctionReference.SetExternalMember(GetEngineSubsystemFuncName, USubsystemBlueprintLibrary::StaticClass());

    GetConfigManagerNode->PostPlacedNewNode();
    GetConfigManagerNode->AllocateDefaultPins();
    CompilerContext.MessageLog.NotifyIntermediateObjectCreation(GetConfigManagerNode, this);

    UEdGraphPin* CallCreateClassTypePin = GetConfigManagerNode->FindPinChecked(TEXT("Class"));
    UEdGraphPin* SubsystemOutputPin = GetConfigManagerNode->GetReturnValuePin();

    // Set engine subsystem class for the GetEngineSubsystem method call
    CallCreateClassTypePin->DefaultObject = UConfigManager::StaticClass();

    // Allocate node for making config id struct literal
    UK2Node_MakeStruct* MakeConfigId = SourceGraph->CreateIntermediateNode<UK2Node_MakeStruct>();
    MakeConfigId->StructType = FConfigId::StaticStruct();

    MakeConfigId->PostPlacedNewNode();
    MakeConfigId->AllocateDefaultPins();
    CompilerContext.MessageLog.NotifyIntermediateObjectCreation(MakeConfigId, this);

    // Set default pin connections for MakeStruct pins
    UEdGraphPin* ConfigIdOutputPin = MakeConfigId->FindPinChecked(MakeConfigId->StructType->GetFName());
    UEdGraphPin* ConfigIdModReferencePin = MakeConfigId->FindPinChecked(GET_MEMBER_NAME_CHECKED(FConfigId, ModReference));
    UEdGraphPin* ConfigIdCategoryPin = MakeConfigId->FindPinChecked(GET_MEMBER_NAME_CHECKED(FConfigId, ConfigCategory));

    UModConfiguration* ModConfigurationCDO = ModConfigurationClass.GetDefaultObject();
    if (ModConfigurationCDO == NULL) {
        CompilerContext.MessageLog.Error(*LOCTEXT("InvalidConfigClass", "Node @@ does not reference a valid configuration class").ToString(), this);
        return;
    }

    ConfigIdModReferencePin->DefaultValue = ModConfigurationCDO->ConfigId.ModReference;
    ConfigIdCategoryPin->DefaultValue = ModConfigurationCDO->ConfigId.ConfigCategory;

    // Create node for storing struct result in a temporary variable
    UK2Node_TemporaryVariable* TemporaryVariableNode = SourceGraph->CreateIntermediateNode<UK2Node_TemporaryVariable>();
    TemporaryVariableNode->VariableType = StructOutputPin->PinType;

    TemporaryVariableNode->PostPlacedNewNode();
    TemporaryVariableNode->AllocateDefaultPins();
    CompilerContext.MessageLog.NotifyIntermediateObjectCreation(TemporaryVariableNode, this);

    UEdGraphPin* TemporaryVariablePin = TemporaryVariableNode->GetVariablePin();

    // Create node for filling configuration struct with actual data
    UK2Node_CallFunction* FillConfigurationStructNode = SourceGraph->CreateIntermediateNode<UK2Node_CallFunction>();
    const FName FillConfigStructFuncName = GET_FUNCTION_NAME_CHECKED(UConfigManager, FillConfigurationStruct);
    FillConfigurationStructNode->FunctionReference.SetExternalMember(FillConfigStructFuncName, UConfigManager::StaticClass());

    FillConfigurationStructNode->PostPlacedNewNode();
    FillConfigurationStructNode->AllocateDefaultPins();
    CompilerContext.MessageLog.NotifyIntermediateObjectCreation(FillConfigurationStructNode, this);

    // Connect Self input pin with the subsystem output pin we obtained above
    UEdGraphPin* SelfInputPin = FillConfigurationStructNode->FindPinChecked(UEdGraphSchema_K2::PN_Self, EGPD_Input);
    SubsystemOutputPin->PinType = SelfInputPin->PinType;
    SubsystemOutputPin->MakeLinkTo(SelfInputPin);

    // Connect ConfigId input pin with the MakeStruct node output we obtained above
    UEdGraphPin* ConfigIdInputPin = FillConfigurationStructNode->FindPinChecked(TEXT("ConfigId"), EGPD_Input);
    ConfigIdOutputPin->MakeLinkTo(ConfigIdInputPin);

    // Connect StructInfo input pin with the temporary variable pin we obtained above
    UEdGraphPin* StructInfoInputPin = FillConfigurationStructNode->FindPinChecked(TEXT("StructInfo"), EGPD_Input);
    StructInfoInputPin->PinType = TemporaryVariablePin->PinType;
    TemporaryVariablePin->MakeLinkTo(StructInfoInputPin);
    FillConfigurationStructNode->PinConnectionListChanged(StructInfoInputPin);

    // Move output pins from the current node to the temporary variable node, and move exec pins to FillConfigurationStruct node pins
    UEdGraphPin* FillConfigStructInputExecPin = FillConfigurationStructNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
    UEdGraphPin* FillConfigStructOutputThenPin = FillConfigurationStructNode->FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);

    CompilerContext.MovePinLinksToIntermediate(*StructOutputPin, *TemporaryVariablePin);
    CompilerContext.MovePinLinksToIntermediate(*InputExecPin, *FillConfigStructInputExecPin);
    CompilerContext.MovePinLinksToIntermediate(*OutputThenExecPin, *FillConfigStructOutputThenPin);

    // Break all links to this node so it will be removed as unconnected
    BreakAllNodeLinks();
}

FText UK2Node_GetModConfiguration::GetTooltipText() const {
    return LOCTEXT("GetModConfigNodeTooltip", "Retrieves active configuration value for given configuration type");
}

FText UK2Node_GetModConfiguration::GetNodeTitle(ENodeTitleType::Type TitleType) const {
    const FText ClassName = FBlueprintEditorUtils::GetFriendlyClassDisplayName(ModConfigurationClass);
    return FText::Format(LOCTEXT("GetModConfigNodeTitle", "Get {0}"), ClassName);
}

UEdGraphPin* UK2Node_GetModConfiguration::GetOutputConfigStructPin() const {
    return FindPin(GetModConfiguration_OutputPinName, EGPD_Output);
}

UScriptStruct* UK2Node_GetModConfiguration::ResolveConfigurationStruct(UClass* ConfigClass) {
    // Fail instantly when we are provided with NULL class
    if (ConfigClass == NULL) {
        return NULL;
    }

    // Skip class if it's not generated by the blueprint
    UBlueprint* OwningBlueprint = Cast<UBlueprint>(ConfigClass->ClassGeneratedBy);
    if (OwningBlueprint == NULL) {
        return NULL;
    }

    const FString BlueprintName = OwningBlueprint->GetName();
    const FString RootConfigStructName = FString::Printf(TEXT("%sStruct"), *BlueprintName);

    // First try to resolve native struct with the same name in any package
    if (UScriptStruct* NativeStruct = FindObject<UScriptStruct>(ANY_PACKAGE, *RootConfigStructName)) {
        return NativeStruct;
    }

    // Otherwise try to resolve blueprint struct in the same package as the blueprint
    const FString BlueprintPackageName = OwningBlueprint->GetOutermost()->GetName();
    const FString BasePackagePath = FPackageName::GetLongPackagePath(BlueprintPackageName);

    const FString BlueprintStructPackageName = UPackageTools::SanitizePackageName(BasePackagePath + TEXT("/") + RootConfigStructName);
    if (UPackage* StructPackage = LoadPackage(NULL, *BlueprintStructPackageName, LOAD_Quiet)) {
        if (UUserDefinedStruct* UserDefinedStruct = FindObject<UUserDefinedStruct>(StructPackage, *RootConfigStructName)) {
            return UserDefinedStruct;
        }
    }

    // Cannot resolve neither native nor blueprint struct, return NULL
    return NULL;
}

bool UK2Node_GetModConfiguration::IsValidConfigurationClass(UClass* Class) {
    // Skip abstract classes or classes replaced by the blueprint recompilation
    if (Class->HasAnyClassFlags(CLASS_NewerVersionExists | CLASS_Abstract)) {
        return false;
    }

    // Skip classes which don't have valid mod reference set or have no valid root section
    UModConfiguration* ConfigurationCDO = CastChecked<UModConfiguration>(Class->GetDefaultObject());
    if (ConfigurationCDO->ConfigId.ModReference.IsEmpty() || ConfigurationCDO->RootSection == NULL) {
        return false;
    }

    // Otherwise we consider it valid if we can resolve configuration struct for it
    UScriptStruct* ConfigurationStruct = ResolveConfigurationStruct(Class);
    return ConfigurationStruct != NULL;
}

void UK2Node_GetModConfiguration::RetrieveAllConfigurationClasses(TArray<UClass*>& OutValidConfigurationClasses) {
    // Retrieve all possible classes usable as NativeParentClass asset registry attribute
    TArray<UClass*> DerivedConfigClasses;
    GetDerivedClasses(UModConfiguration::StaticClass(), DerivedConfigClasses);
    DerivedConfigClasses.Add(UModConfiguration::StaticClass());

    TArray<FString> PossibleTagValues;
    for (UClass* ConfigClass : DerivedConfigClasses) {
        if (Cast<UBlueprintGeneratedClass>(ConfigClass) == NULL) {
            const FString NativeParentClassName = FString::Printf(TEXT("%s'%s'"), *ConfigClass->GetClass()->GetName(), *ConfigClass->GetPathName());
            PossibleTagValues.Add(NativeParentClassName);
        }
    }

    TArray<UClass*> FoundBlueprintAssets;
    UBlueprintAssetHelperLibrary::FindBlueprintAssetsByTag(UModConfiguration::StaticClass(), FBlueprintTags::NativeParentClassPath, PossibleTagValues, FoundBlueprintAssets);

    // Filter classes and basically exclude abstract classes or classes without FConfigId set
    for (UClass* BlueprintConfigClass : FoundBlueprintAssets) {
        if (IsValidConfigurationClass(BlueprintConfigClass)) {
            OutValidConfigurationClasses.Add(BlueprintConfigClass);
        }
    }
}
