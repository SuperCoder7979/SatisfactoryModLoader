#pragma once
#include "Patching/Patch/OptionsKeybindPatch.h"
#include "Patching/BlueprintHookHelper.h"
#include "Patching/BlueprintHookManager.h"
#include "Reflection/ReflectionHelper.h"
#include "SatisfactoryModLoader.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/UserDefinedStruct.h"
#include "ModLoading/ModLoadingLibrary.h"

void FOptionsKeybindPatch::CategorizeKeyBindingsByModReference(FScriptArrayHelper& InKeyBindings, UScriptStruct* KeyBindStructClass, TMap<FString, TArray<int32>>& OutCategorizedNames) {
    FNameProperty* ActionNameProperty = FReflectionHelper::FindPropertyByShortNameChecked<FNameProperty>(KeyBindStructClass, TEXT("ActionName"));
    check(ActionNameProperty);

    for (int32 i = 0; i < InKeyBindings.Num(); i++) {
        // Retrieve action name from script struct
        void* KeyBindData = InKeyBindings.GetRawPtr(i);
        const FString ActionName = ActionNameProperty->GetPropertyValue_InContainer(KeyBindData).ToString();
        int32 FirstDotIndex;
        ActionName.FindChar(TEXT('.'), FirstDotIndex);
        FString ResultModReference;

        // If dot is not found in key binding action name, it should be native FactoryGame key binding
        if (FirstDotIndex == INDEX_NONE) {
            ResultModReference = TEXT("FactoryGame");
        } else {
            ResultModReference = ActionName.Mid(0, FirstDotIndex);
        }
        // Add resulting key binding into array
        TArray<int32>& ResultArray = OutCategorizedNames.FindOrAdd(ResultModReference);
        ResultArray.Add(i);
    }
}

void FOptionsKeybindPatch::SortModReferencesByDisplayName(TArray<FString>& InModReferences, TMap<FString, FString>& OutDisplayNames) {
    for (const FString& ModReference : InModReferences) {
        FModInfo ModInfo;
        UModLoadingLibrary* ModLoadingLibrary = GEngine->GetEngineSubsystem<UModLoadingLibrary>();
        check(ModLoadingLibrary->GetLoadedModInfo(ModReference, ModInfo));

        OutDisplayNames.Add(ModReference, ModInfo.FriendlyName);
    }
    InModReferences.StableSort(
        [&](const FString& A, const FString& B) {
            const FString& ModNameA = OutDisplayNames.FindChecked(A);
            const FString& ModNameB = OutDisplayNames.FindChecked(B);
            return ModNameA < ModNameB;
        }
    );
}

void FOptionsKeybindPatch::PopulateKeyBindButtonList(UUserWidget* ContextWidget, FScriptArrayHelper& KeyBindingsArray, const TArray<FString>& SortedModReferences, const TMap<FString, FString> DisplayNames, const TMap<FString, TArray<int32>>& CategorizedKeyBinds) {
    UClass* KeyBindButtonWidgetClass = LoadObject<UClass>(NULL, TEXT("/Game/FactoryGame/Interface/UI/Menu/Widget_KeybindButton.Widget_KeybindButton_C"));
    check(KeyBindButtonWidgetClass);

    // Now we have sorted out key bindings array. Add them in given order to the vertical box.
    UVerticalBox* ButtonBox = FReflectionHelper::GetObjectPropertyValue<UVerticalBox>(ContextWidget, TEXT("mButtonBox"));

    for (const FString& ModReference : SortedModReferences) {
        const TArray<int32>& KeyBindingIndices = CategorizedKeyBinds.FindChecked(ModReference);
        APlayerController* OwningController = ContextWidget->GetOwningPlayer();
        // Add display name on top of key bindings
        UTextBlock* TitleTextBlock = NewObject<UTextBlock>(OwningController);
        TitleTextBlock->Font.Size = 16;
        const FString ModDisplayName = DisplayNames.FindChecked(ModReference);
        TitleTextBlock->SetText(FText::FromString(ModDisplayName));
        ButtonBox->AddChildToVerticalBox(TitleTextBlock);

        // Construct widget, set relevant properties and add it into the box directly
        for (int32 KeyBindIndex : KeyBindingIndices) {
            void* KeyBindDataPtr = KeyBindingsArray.GetRawPtr(KeyBindIndex);
            UUserWidget* ButtonWidget = UWidgetBlueprintLibrary::Create(ContextWidget, KeyBindButtonWidgetClass, OwningController);
            FReflectionHelper::SetStructPropertyValue(ButtonWidget, TEXT("mKeyBindData"), KeyBindDataPtr);
            FReflectionHelper::SetPropertyValue<FObjectProperty>(ButtonWidget, TEXT("mKeyBindParent"), ContextWidget);
            ButtonBox->AddChildToVerticalBox(ButtonWidget);
        }
    }
}

void FOptionsKeybindPatch::HandleRefreshKeyBindingsHook(FBlueprintHookHelper& HookHelper) {
    // Retrieve information about struct and properties first
    UUserWidget* ContextWidget = CastChecked<UUserWidget>(HookHelper.GetContext());
    UUserDefinedStruct* KeyBindStructClass = LoadObject<UUserDefinedStruct>(NULL, TEXT("/Game/FactoryGame/Interface/UI/Menu/KeyBindData"));
    check(KeyBindStructClass);

    // Retrieve key bindings by calling blueprint method
    FScriptArray OutKeyBindDataArray;
    UFunction* Function = FReflectionHelper::CallScriptFunction(HookHelper.GetContext(), TEXT("GetKeybindData"), &OutKeyBindDataArray);
    FArrayProperty* FirstParameter = CastField<FArrayProperty>(FReflectionHelper::FindParameterByIndex(Function, 0));
    FScriptArrayHelper KeyBindingsArrayHelper(FirstParameter, &OutKeyBindDataArray);

    // KeyBind data is returned in the array, process each element and divide them by mod display name
    TMap<FString, TArray<int32>> OutCategorizedKeyBinds;
    CategorizeKeyBindingsByModReference(KeyBindingsArrayHelper, KeyBindStructClass, OutCategorizedKeyBinds);

    // Generate ModReference array and sort it by display name
    TMap<FString, FString> OutDisplayNameByModReference;
    TArray<FString> SortedModReferences;
    OutCategorizedKeyBinds.GetKeys(SortedModReferences);
    SortModReferencesByDisplayName(SortedModReferences, OutDisplayNameByModReference);

    // Now we have sorted out key bindings array. Add them in given order to the vertical box.
    PopulateKeyBindButtonList(ContextWidget, KeyBindingsArrayHelper, SortedModReferences, OutDisplayNameByModReference, OutCategorizedKeyBinds);

    // Empty script array and deallocate all structures
    KeyBindingsArrayHelper.EmptyValues();

    // Skip normal function code because it is unnecessary to run
    HookHelper.JumpToFunctionReturn();
}

void FOptionsKeybindPatch::RegisterPatch() {
    // TODO fix once we get the fresh dump of blueprints
    // UClass* WidgetKeyBindClass = LoadObject<UClass>(NULL, TEXT("/Game/FactoryGame/Interface/UI/Menu/PauseMenu/Widget_KeyBind.Widget_KeyBind_C"));
    // check(WidgetKeyBindClass);
    // UFunction* RefreshKeyBindingsFunction = WidgetKeyBindClass->FindFunctionByName(TEXT("RefreshKeyBindings"));
    // UBlueprintHookManager* HookManager = GEngine->GetEngineSubsystem<UBlueprintHookManager>();
    // HookManager->HookBlueprintFunction(RefreshKeyBindingsFunction, HandleRefreshKeyBindingsHook, EPredefinedHookOffset::Start);
}
