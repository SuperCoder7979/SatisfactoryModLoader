#pragma once
#include "CoreMinimal.h"
#include "K2Node.h"
#include "Configuration/ModConfiguration.h"
#include "K2Node_GetModConfiguration.generated.h"

UCLASS()
class SMLEDITOR_API UK2Node_GetModConfiguration : public UK2Node {
    GENERATED_BODY()
public:
    UPROPERTY()
    TSubclassOf<UModConfiguration> ModConfigurationClass;

    /** Retrieves script struct describing this configuration layout, or NULL if it doesn't exist */
    UScriptStruct* GetConfigurationStruct() const;

    // ~ Begin UEdGraphNode Interface
    virtual void AllocateDefaultPins() override;
    virtual FText GetTooltipText() const override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    // ~ End UEdGraphNode Interface

    // ~ Begin UK2Node Interface
    virtual bool IsNodePure() const override { return false; }
    virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
    virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
    virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
    virtual FText GetMenuCategory() const override;
    virtual FLinearColor GetNodeTitleColor() const override;
    // ~ End UK2Node Interface

    /** Get the return value pin */
    UEdGraphPin* GetOutputConfigStructPin() const;

private:
    /** Tries to resolve configuration struct associated with the provided class, or returns NULL if struct is not found */
    static UScriptStruct* ResolveConfigurationStruct(UClass* ConfigClass);

    /** Determines whenever provided class is valid configuration class */
    static bool IsValidConfigurationClass(UClass* Class);

    /** Retrieves all valid configuration classes by scanning asset registry */
    static void RetrieveAllConfigurationClasses(TArray<UClass*>& OutValidConfigurationClasses);
};
