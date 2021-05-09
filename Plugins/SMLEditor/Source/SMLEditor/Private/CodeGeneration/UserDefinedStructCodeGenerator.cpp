#include "CodeGeneration/UserDefinedStructCodeGenerator.h"
#include "AssetToolsModule.h"
#include "EdGraphSchema_K2.h"
#include "FileHelpers.h"
#include "PackageTools.h"
#include "Configuration/ModConfiguration.h"
#include "Configuration/CodeGeneration/ConfigGenerationContext.h"
#include "Engine/UserDefinedStruct.h"
#include "Factories/StructureFactory.h"
#include "Framework/Notifications/NotificationManager.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Widgets/Notifications/SNotificationList.h"
#define LOCTEXT_NAMESPACE "SML"

void FUserDefinedStructCodeGenerator::ShowSuccessNotification(const FText& NotificationText) {
    FNotificationInfo Info(NotificationText);
    Info.ExpireDuration = 3.0f;
    Info.bUseLargeFont = false;
    TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
    if (Notification.IsValid()) {
        Notification->SetCompletionState(SNotificationItem::CS_Success);
    }
}

bool FUserDefinedStructCodeGenerator::GenerateConfigStructForConfigurationAsset(UBlueprint* Blueprint) {
    UConfigGenerationContext* GenerationContext;
    if (!CreateConfigStructContextForConfigurationAsset(Blueprint, GenerationContext)) {
        return false;
    }

    const FString ModConfigPackageName = Blueprint->GetOutermost()->GetName();
    const FString ModConfigFolderName = FPackageName::GetLongPackagePath(ModConfigPackageName);
    const bool bSuccess = GenerateConfigurationStruct(ModConfigFolderName, GenerationContext);
    if (bSuccess) {
        ShowSuccessNotification(
            FText::Format(
                LOCTEXT(
                    "BlueprintConfigStructRegenerated",
                    "Successfully regenerated Structs for Configuration '{0}' and saved them under '{1}'"
                ),
                FText::FromString(Blueprint->GetName()),
                FText::FromString(ModConfigFolderName)
            )
        );
    }
    return bSuccess;
}

bool FUserDefinedStructCodeGenerator::CreateConfigStructContextForConfigurationAsset(UBlueprint* Blueprint, UConfigGenerationContext*& OutGenerationContext) {
    const FString BlueprintName = Blueprint->GetName();

    UClass* BlueprintGeneratedClass = Blueprint->GeneratedClass;
    if (BlueprintGeneratedClass == NULL || !BlueprintGeneratedClass->IsChildOf<UModConfiguration>()) {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::Format(
                LOCTEXT(
                    "MissingGeneratedClass",
                    "Blueprint '{0}' is not compiled, or does not inherit from Mod Configuration. "
                    "Make sure blueprint is compiled successfully and has Mod Configuration as a parent class"
                ),
                FText::FromString(BlueprintName)
            )
        );
        return false;
    }

    UModConfiguration* ModConfiguration = CastChecked<UModConfiguration>(BlueprintGeneratedClass->GetDefaultObject());
    if (ModConfiguration->RootSection == NULL) {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::Format(
                LOCTEXT(
                    "MissingRootSection",
                    "Blueprint '{0}' does not contain a valid root section. "
                    "Make sure 'Root Section' property has a valid object value"
                ),
                FText::FromString(BlueprintName)
            )
        );
        return false;
    }

    OutGenerationContext = NewObject<UConfigGenerationContext>();

    const FString BaseStructName = FString::Printf(TEXT("%sStruct"), *BlueprintName);
    ModConfiguration->RootSection->CreatePropertyDescriptor(OutGenerationContext, BaseStructName);
    return true;
}

bool FUserDefinedStructCodeGenerator::GenerateConfigurationStruct(const FString& PackageName, UConfigGenerationContext* Context) {
    // Load required modules and create factories for making new struct assets
    UStructureFactory* StructureFactory = NewObject<UStructureFactory>();
    FAssetToolsModule& Module = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    IAssetTools& AssetTools = Module.Get();
    TMap<UConfigGeneratedStruct*, UUserDefinedStruct*> GeneratedStructs;
    TArray<UPackage*> PackagesToSave;

    // Generate asset for each struct we have
    for (UConfigGeneratedStruct* GeneratedStruct : Context->GetAllGeneratedStructs()) {
        const FString& StructAssetName = GeneratedStruct->GetStructName();

        const FString FinalStructAssetPackagePath = UPackageTools::SanitizePackageName(PackageName + TEXT("/") + StructAssetName);
        if (!FPackageName::IsValidLongPackageName(FinalStructAssetPackagePath)) {
            FMessageDialog::Open(
                EAppMsgType::Ok,
                FText::Format(
                    LOCTEXT("InvalidPackageName", "Cannot create struct asset because of the invalid package name '{0}'"),
                    FText::FromString(FinalStructAssetPackagePath)
                )
            );
            return false;
        }

        UUserDefinedStruct* UserDefinedStruct;

        // First try to load existing package and make sure it represents an user defined struct
        if (UPackage* ExistingPackage = LoadPackage(NULL, *FinalStructAssetPackagePath, LOAD_Quiet)) {
            UserDefinedStruct = FindObject<UUserDefinedStruct>(ExistingPackage, *StructAssetName);
            if (UserDefinedStruct == NULL) {
                FMessageDialog::Open(
                    EAppMsgType::Ok,
                    FText::Format(
                        LOCTEXT(
                            "InvalidConfigAsset",
                            "Package with name '{0} already exists, but it's type is not a user defined struct. "
                            "Delete it or make sure it contains a correct user defined struct."
                        ),
                        FText::FromString(FinalStructAssetPackagePath)
                    )
                );
                return false;
            }

            // Clear struct contains so it will only contain one dummy field, just like the newly created one
            ClearUserDefinedStructContents(UserDefinedStruct);
        } else {
            // Create asset manually if we haven't found existing one already
            UserDefinedStruct = Cast<UUserDefinedStruct>(AssetTools.CreateAsset(GeneratedStruct->GetStructName(), PackageName, NULL, StructureFactory));
            if (UserDefinedStruct == NULL) {
                FMessageDialog::Open(
                    EAppMsgType::Ok,
                    FText::Format(
                        LOCTEXT("NameOccupied", "Couldn't create config struct asset at package name '{0}'"),
                        FText::FromString(FinalStructAssetPackagePath)
                    )
                );
                return false;
            }
        }

        GeneratedStructs.Add(GeneratedStruct, UserDefinedStruct);
        PackagesToSave.Add(UserDefinedStruct->GetOutermost());
    }

    // Populate each struct generated with properties
    for (const TPair<UConfigGeneratedStruct*, UUserDefinedStruct*>& Pair : GeneratedStructs) {
        PopulateGeneratedStruct(Pair.Key, Pair.Value, GeneratedStructs);
    }

    // Remove dummy variable created as a part of structure initial creation
    for (const TPair<UConfigGeneratedStruct*, UUserDefinedStruct*>& Pair : GeneratedStructs) {
        UUserDefinedStruct* DefinedStruct = Pair.Value;
        const FStructVariableDescription& Description = FStructureEditorUtils::GetVarDesc(DefinedStruct)[0];
        FStructureEditorUtils::RemoveVariable(DefinedStruct, Description.VarGuid);
    }

    // Save packages that we just created
    FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, /*bCheckDirty*/false, /*bPromptToSave*/false);
    return true;
}

void FUserDefinedStructCodeGenerator::ClearUserDefinedStructContents(UUserDefinedStruct* Struct) {
    const TArray<FStructVariableDescription> OldVariables = FStructureEditorUtils::GetVarDesc(Struct);

    // Create one dummy variable at the end of the struct
    FStructureEditorUtils::AddVariable(
        Struct,
        FEdGraphPinType(
            UEdGraphSchema_K2::PC_Boolean,
            NAME_None,
            NULL,
            EPinContainerType::None,
            false,
            FEdGraphTerminalType()
        )
    );

    // Remove all previous variables so only remaining one will be a dummy one
    for (const FStructVariableDescription& VariableDescription : OldVariables) {
        FStructureEditorUtils::RemoveVariable(Struct, VariableDescription.VarGuid);
    }
}

FGuid GenerateRandomUniqueIdFromString(const FString& DisplayName) {
    FGuid ResultGuid;

    const FRandomStream RandomStream{(int32)GetTypeHash(DisplayName)};
    const uint64 CityHash = CityHash64((ANSICHAR*)GetData(DisplayName), DisplayName.Len() * sizeof(TCHAR));

    ResultGuid[0] = RandomStream.GetUnsignedInt();
    ResultGuid[1] = RandomStream.GetUnsignedInt();
    ResultGuid[2] = (uint32)(CityHash & 0xFFFFFFFF);
    ResultGuid[3] = (uint32)(CityHash >> 32);

    // https://tools.ietf.org/html/rfc4122#section-4.4
    // https://en.wikipedia.org/wiki/Universally_unique_identifier
    //
    // The 4 bits of digit M indicate the UUID version, and the 1–3
    // most significant bits of digit N indicate the UUID variant.
    // xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx
    ResultGuid[1] = (ResultGuid[1] & 0xffff0fff) | 0x00004000; // version 4
    ResultGuid[2] = (ResultGuid[2] & 0x3fffffff) | 0x80000000; // variant 1

    return ResultGuid;
}

FName GenerateStructVariableName(const uint32 UniqueNameId, const FGuid Guid) {
    const FString Result = TEXT("MemberVar");
    const FString FriendlyName = FString::Printf(TEXT("%s_%u"), *Result, UniqueNameId);
    const FName NameResult = *FString::Printf(TEXT("%s_%s"), *FriendlyName, *Guid.ToString(EGuidFormats::Digits));
    check(NameResult.IsValidXName(INVALID_OBJECTNAME_CHARACTERS));
    return NameResult;
}

bool FUserDefinedStructCodeGenerator::SpawnStructVariableWithFixedName(UUserDefinedStruct* Struct, const FEdGraphPinType& VariableType, const FString& DisplayName) {
    const FScopedTransaction Transaction(LOCTEXT("AddVariable", "Add Variable"));
    FStructureEditorUtils::ModifyStructData(Struct);

    FString ErrorMessage;
    if (!FStructureEditorUtils::CanHaveAMemberVariableOfType(Struct, VariableType, &ErrorMessage)) {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::Format(
                LOCTEXT(
                    "StructCannotHaveMemberOfType",
                    "Configuration Struct '{0}' cannot have a member '{1}' of predefined type: {2}. "
                    "Make sure your configuration does not contain recursion or invalid property types."
                ),
                FText::FromString(Struct->GetName()),
                FText::FromString(DisplayName),
                FText::FromString(ErrorMessage)
            )
        );
        return false;
    }

    const FGuid Guid = GenerateRandomUniqueIdFromString(DisplayName);
    const uint32 UniqueNameId = GetTypeHash(DisplayName);
    const FName VarName = GenerateStructVariableName(UniqueNameId, Guid);
    TArray<FStructVariableDescription>& VarDesc = FStructureEditorUtils::GetVarDesc(Struct);
    check(NULL == VarDesc.FindByPredicate(FStructureEditorUtils::FFindByNameHelper<FStructVariableDescription>(VarName)));
    check(FStructureEditorUtils::IsUniqueVariableFriendlyName(Struct, DisplayName));

    FStructVariableDescription NewVar;
    NewVar.VarName = VarName;
    NewVar.FriendlyName = DisplayName;
    NewVar.SetPinType(VariableType);
    NewVar.VarGuid = Guid;
    VarDesc.Add(NewVar);

    FStructureEditorUtils::OnStructureChanged(Struct, FStructureEditorUtils::EStructureEditorChangeInfo::AddedVariable);
    return true;
}

void FUserDefinedStructCodeGenerator::PopulateGeneratedStruct
(
    UConfigGeneratedStruct* GeneratedStruct,
    UUserDefinedStruct* UserStruct,
    const TMap<UConfigGeneratedStruct*, UUserDefinedStruct*>& GeneratedStructs
) {
    // Generate each config variable
    for (const TPair<FString, FConfigVariableDescriptor>& Pair : GeneratedStruct->GetVariables()) {
        // Creates new variable with specified type
        const FEdGraphPinType GraphPin = CreatePinTypeForVariable(Pair.Value, GeneratedStructs);
        SpawnStructVariableWithFixedName(UserStruct, GraphPin, Pair.Key);
    }
}

FEdGraphPinType FUserDefinedStructCodeGenerator::CreatePinTypeForVariable
(
    const FConfigVariableDescriptor& Descriptor,
    const TMap<UConfigGeneratedStruct*, UUserDefinedStruct*>& GeneratedStructs
) {

    const EConfigVariableType VariableType = Descriptor.GetVariableType();
    FEdGraphPinType GraphPinType{};

    if (VariableType == EConfigVariableType::ECVT_Bool) {
        GraphPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    } else if (VariableType == EConfigVariableType::ECVT_Float) {
        GraphPinType.PinCategory = UEdGraphSchema_K2::PC_Float;
    } else if (VariableType == EConfigVariableType::ECVT_Int32) {
        GraphPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    } else if (VariableType == EConfigVariableType::ECVT_Int64) {
        GraphPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
    } else if (VariableType == EConfigVariableType::ECVT_String) {
        GraphPinType.PinCategory = UEdGraphSchema_K2::PC_String;
    } else if (VariableType == EConfigVariableType::ECVT_Float) {
        GraphPinType.PinCategory = UEdGraphSchema_K2::PC_Float;
    } else if (VariableType == EConfigVariableType::ECVT_CustomStruct) {
        GraphPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        GraphPinType.PinSubCategoryObject = Descriptor.GetCustomStructType();
    } else if (VariableType == EConfigVariableType::ECVT_ConfigGeneratedStruct) {
        GraphPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        UUserDefinedStruct* MatchingStruct = GeneratedStructs.FindChecked(Descriptor.GetConfigGeneratedStruct());
        GraphPinType.PinSubCategoryObject = MatchingStruct;
    } else if (VariableType == EConfigVariableType::ECVT_Object) {
        GraphPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        UClass* BaseObjectClass = Descriptor.GetBaseObjectClass();
        GraphPinType.PinSubCategoryObject = BaseObjectClass ? BaseObjectClass : UObject::StaticClass();
    } else if (VariableType == EConfigVariableType::ECVT_Class) {
        GraphPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
        UClass* BaseClassType = Descriptor.GetBaseClassType();
        GraphPinType.PinSubCategoryObject = BaseClassType ? BaseClassType : UClass::StaticClass();
    } else if (VariableType == EConfigVariableType::ECVT_Array) {
        FEdGraphPinType InnerGraphPinType = CreatePinTypeForVariable(Descriptor.GetArrayElementDescriptor(), GeneratedStructs);
        GraphPinType = InnerGraphPinType;
        GraphPinType.ContainerType = EPinContainerType::Array;
    } else if (VariableType == EConfigVariableType::ECVT_Map) {
        FEdGraphPinType KeyPinType = CreatePinTypeForVariable(Descriptor.GetMapKeyDescriptor(), GeneratedStructs);
        FEdGraphPinType ValuePinType = CreatePinTypeForVariable(Descriptor.GetMapValueDescriptor(), GeneratedStructs);
        GraphPinType = KeyPinType;
        GraphPinType.ContainerType = EPinContainerType::Map;
        GraphPinType.PinValueType = FEdGraphTerminalType::FromPinType(ValuePinType);
    }
    return GraphPinType;
}
