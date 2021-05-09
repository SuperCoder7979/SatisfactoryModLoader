#pragma once

#include "Core.h"
#include "Engine.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EditorBlueprintFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class SMLEDITOR_API UEditorBlueprintFunctionLibrary : public UBlueprintFunctionLibrary {
    GENERATED_BODY()
public:

    /** 
    * Create a Blueprint from provided Class
    * Path is relative!  For example :  "Game/ExampleMod"
    **/
    UFUNCTION(BlueprintCallable, Category = "Editor| File IO")
    static UObject* CreateBlueprintOfClass(UClass* Class, FString Name, FString Path);

    /*
        !!! CAREFULL ! REALLY SLOW !!!
        Path is relative!
        For Example :  "/Game/FactoryGame/Resource" is expected
        No provided UClass Filter will return all found Objects
    */
    UFUNCTION(BlueprintCallable, Category = "Editor| Class")
    static TArray<UObject*> GetBlueprintsFromPath(FString Path, UClass* Filter);

    /** 
    * A Created Blueprint is UBlueprint derived and not "your" Generated Class
    * This Function resolves UBlueprint Object to its generated Class
    * Alternative: search for the Class By Name of the Blueprint with _C Appended.
    **/
    UFUNCTION(BlueprintCallable, Category = "Editor| Class")
    static UClass* GetClassGeneratedByBlueprint(UObject* Blueprint);
};
