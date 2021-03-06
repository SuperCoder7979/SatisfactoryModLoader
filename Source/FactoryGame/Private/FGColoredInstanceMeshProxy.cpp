// This file has been automatically generated by the Unreal Header Implementation tool

#include "FGColoredInstanceMeshProxy.h"

UFGColoredInstanceMeshProxy::UFGColoredInstanceMeshProxy() : Super() {
	this->mCanBecolored = true;
	this->PrimaryComponentTick.TickGroup = TG_DuringPhysics; this->PrimaryComponentTick.EndTickGroup = TG_PrePhysics; this->PrimaryComponentTick.bTickEvenWhenPaused = false; this->PrimaryComponentTick.bCanEverTick = false; this->PrimaryComponentTick.bStartWithTickEnabled = false; this->PrimaryComponentTick.bAllowTickOnDedicatedServer = false; this->PrimaryComponentTick.TickInterval = 0;
}
void UFGColoredInstanceMeshProxy::BeginPlay(){ }
void UFGColoredInstanceMeshProxy::OnRegister(){ Super::OnRegister(); }
void UFGColoredInstanceMeshProxy::OnUnregister(){ Super::OnUnregister(); }
void UFGColoredInstanceMeshProxy::SetColorSlot(uint8 colorSlotIndex){ }
void UFGColoredInstanceMeshProxy::SetInstanced(bool setToInstanced){ }
void UFGColoredInstanceMeshProxy::OnHiddenInGameChanged(){ }
bool UFGColoredInstanceMeshProxy::ShouldCreateRenderState() const { 
#if WITH_EDITOR // Ensure visibility in blueprint & editor.
    if( GetWorld()->WorldType == EWorldType::Editor || GetWorld()->WorldType == EWorldType::EditorPreview)
    {
        return true;
    }
#endif    
    // only create when its blocking instancing.
    return mBlockInstancing;
}
void UFGColoredInstanceMeshProxy::InstantiateInternal(){ }
