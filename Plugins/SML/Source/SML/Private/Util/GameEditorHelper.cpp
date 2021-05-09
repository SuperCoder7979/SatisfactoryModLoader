#include "Util/GameEditorHelper.h"
#include "Slate.h"
#include "Engine/GameEngine.h"
#if WITH_EDITOR
#include "Interfaces/IMainFrameModule.h"
#endif

TSharedPtr<SWindow> FGameEditorHelper::GetMainWindow() {
    // First, check for the game engine, which basically means either game
    // or editor starting up in a game mode using -game command line switch
    UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
    if (GameEngine != NULL) {
        return GameEngine->GameViewportWindow.Pin();
    }
#if WITH_EDITOR
    // Try to use Mainframe module now, if we are built with editor
    if(FModuleManager::Get().IsModuleLoaded("MainFrame")) {
        IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
        return MainFrame.GetParentWindow();
    }
#endif
    checkf(0, TEXT("GetMainWindow called before engine is initialized"));
    return NULL;
}
