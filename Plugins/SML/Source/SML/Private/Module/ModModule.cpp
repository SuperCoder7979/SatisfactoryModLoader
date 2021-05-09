#include "Module/ModModule.h"

UModModule* UModModule::SpawnChildModule(FName ModuleName, TSoftClassPtr<UModModule> ModuleClass) {
    // Make sure we are not attempting to spawn same module twice
    checkf(!ChildModules.Contains(ModuleName), TEXT("Module already loaded: %s"), *ModuleName.ToString());

    // Create new module, initialize it with data
    UClass* ModuleClassObject = ModuleClass.LoadSynchronous();
    check(ModuleClassObject);
    UModModule* ModModule = NewObject<UModModule>(this, ModuleClassObject, ModuleName);
    ModModule->SetOwnerModReference(GetOwnerModReference());

    // Trigger events that we already received ourselves on spawned module in same order
    for (ELifecyclePhase Phase : EventsReceived) {
        ModModule->DispatchLifecycleEvent(Phase);
    }

    // Add spawned module to the child module map
    ChildModules.Add(ModuleName, ModModule);
    return ModModule;
}

UModModule* UModModule::GetChildModule(FName ModuleName, TSubclassOf<UModModule> ModuleClass) {
    UModModule** ModModule = ChildModules.Find(ModuleName);
    // Check that module actually exists and it's type matches type requested
    if (ModModule != NULL && (*ModModule)->IsA(ModuleClass)) {
        return *ModModule;
    }
    return NULL;
}

void UModModule::DispatchLifecycleEvent(ELifecyclePhase Phase) {
    // Add event to received events, so later spawned child modules will receive it
    EventsReceived.Add(Phase);

    // Process phase event on already spawned child modules
    for (const TPair<FName, UModModule*>& ModModule : ChildModules) {
        ModModule.Value->DispatchLifecycleEvent(Phase);
    }

    // Trigger kismet OnLifecycleEvent callback
    K2_OnLifecycleEvent(Phase);
}

FString UModModule::LifecyclePhaseToString(ELifecyclePhase Phase) {
    UEnum* LifecyclePhase = StaticEnum<ELifecyclePhase>();
    return LifecyclePhase->GetNameStringByValue((int64)Phase);
}
