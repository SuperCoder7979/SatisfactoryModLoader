#include "SMLModule.h"
#include "SatisfactoryModLoader.h"

void FSMLModule::StartupModule() {
    // Basic subsystems like logging are initialized on OnInit
    FSatisfactoryModLoader::PreInitializeModLoading();

    // UObject subsystem and Engine are initialized on PostEngineInit and we need to delay their initialization to that moment
    FCoreDelegates::OnPostEngineInit.AddStatic(FSatisfactoryModLoader::InitializeModLoading);
}

void FSMLModule::ShutdownModule() {
}

IMPLEMENT_GAME_MODULE(FSMLModule, SML);
