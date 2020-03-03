// This file has been automatically generated by the Unreal Header Implementation tool

#include "FGItemDescriptor.h"

#if WITH_EDITOR
void UFGItemDescriptor::DeleteAllIcons(){ }
void UFGItemDescriptor::GenerateAllIcons(){ }
FVector UFGItemDescriptor::GetCenterOfCollision(){ return FVector(); }
#endif 
#if WITH_EDITOR
void UFGItemDescriptor::BeginGenerateIcons(){ }
void UFGItemDescriptor::EndGenerateIcons(){ }
void UFGItemDescriptor::ClearStage(){ }
void UFGItemDescriptor::SetupStage(){ }
void UFGItemDescriptor::GenerateIcon(bool reopenAssetEditor, TArray<UPackage*>& out_modifiedAddedPackages){ }
void UFGItemDescriptor::GetItemName(FString& out_itemName) const{ }
void UFGItemDescriptor::GetCurrentFolderPath(FString& out_folderPath) const{ }
void UFGItemDescriptor::GenerateIconName(FString& out_package, FString& out_assetName) const{ }
void UFGItemDescriptor::GenerateMaterialInstanceName(FString& out_package, FString& out_assetName) const{ }
USceneComponent* UFGItemDescriptor::CreatePreviewComponent( USceneComponent* attachParent,  UActorComponent* componentTemplate, const FName& componentName){ return nullptr; }
USceneComponent* UFGItemDescriptor::CalculateComponentBounds( USceneComponent* attachParent,  UActorComponent* componentTemplate, const FName& componentName){ return nullptr; }
#endif 
#if WITH_EDITORONLY_DATA
UFGItemDescriptor::FGenerateIconContext::FGenerateIconContext(){ }
bool UFGItemDescriptor::FGenerateIconContext::IsValid() const{ return bool(); }
UFGItemDescriptor::FGenerateIconContext UFGItemDescriptor::GenerateIconContext = UFGItemDescriptor::FGenerateIconContext();
#endif 
UFGItemDescriptor::UFGItemDescriptor(){ }
void UFGItemDescriptor::Serialize(FArchive& ar){ Super::Serialize(ar); }
void UFGItemDescriptor::PostLoad(){ Super::PostLoad(); }
EResourceForm UFGItemDescriptor::GetForm(TSubclassOf< UFGItemDescriptor > inClass){ return inClass.GetDefaultObject()->mForm;}
float UFGItemDescriptor::GetEnergyValue(TSubclassOf< UFGItemDescriptor > inClass){ return inClass.GetDefaultObject()->mEnergyValue;}
float UFGItemDescriptor::GetRadioactiveDecay(TSubclassOf< UFGItemDescriptor > inClass){ return inClass.GetDefaultObject()->mRadioactiveDecay;}
FText UFGItemDescriptor::GetItemName(TSubclassOf< UFGItemDescriptor > inClass){
	if (inClass.GetDefaultObject()->mUseDisplayNameAndDescription == true)
		return inClass.GetDefaultObject()->mDisplayName;
	else
		return FText::FromString(inClass->GetName());
}
FText UFGItemDescriptor::GetItemDescription(TSubclassOf< UFGItemDescriptor > inClass){ return inClass.GetDefaultObject()->mDescription;}
void UFGItemDescriptor::GetPreviewView(TSubclassOf< UFGItemDescriptor > inClass, FItemView& out_previewView){ }
void UFGItemDescriptor::GetIconView(TSubclassOf< UFGItemDescriptor > inClass, FItemView& out_itemView){ }
FSlateBrush UFGItemDescriptor::GetItemIcon(TSubclassOf< UFGItemDescriptor > inClass){ return inClass.GetDefaultObject()->mInventoryIcon;}
UTexture2D* UFGItemDescriptor::GetSmallIcon(TSubclassOf< UFGItemDescriptor > inClass){ return inClass.GetDefaultObject()->mSmallIcon;}
UTexture2D* UFGItemDescriptor::GetBigIcon(TSubclassOf< UFGItemDescriptor > inClass){ return inClass.GetDefaultObject()->mPersistentBigIcon;}
UStaticMesh* UFGItemDescriptor::GetItemMesh(TSubclassOf< UFGItemDescriptor > inClass){ return inClass.GetDefaultObject()->mConveyorMesh;}
int32 UFGItemDescriptor::GetStackSize(TSubclassOf< UFGItemDescriptor > inClass){
	if (inClass.GetDefaultObject()->mStackSize == EStackSize::SS_FLUID)
		return 1000;
	else if (inClass.GetDefaultObject()->mStackSize == EStackSize::SS_ONE)
		return 1;
	else if (inClass.GetDefaultObject()->mStackSize == EStackSize::SS_SMALL)
		return 20;
	else if (inClass.GetDefaultObject()->mStackSize == EStackSize::SS_MEDIUM)
		return 50;
	else if (inClass.GetDefaultObject()->mStackSize == EStackSize::SS_BIG)
		return 200;
	else if (inClass.GetDefaultObject()->mStackSize == EStackSize::SS_HUGE)
		return 500;
	else
		return 0;
}
float UFGItemDescriptor::GetStackSizeConverted(TSubclassOf< UFGItemDescriptor > inClass){ return float(); }
bool UFGItemDescriptor::CanBeDiscarded(TSubclassOf< UFGItemDescriptor > inClass){ return inClass.GetDefaultObject()->mCanBeDiscarded;}
bool UFGItemDescriptor::RememberPickUp(TSubclassOf< UFGItemDescriptor > inClass) { return inClass.GetDefaultObject()->mRememberPickUp; }
TSubclassOf< UFGItemCategory > UFGItemDescriptor::GetItemCategory(TSubclassOf< UFGItemDescriptor > inClass) { return inClass.GetDefaultObject()->mItemCategory; }
float UFGItemDescriptor::GetFluidDensity(TSubclassOf< UFGItemDescriptor > inClass) { return inClass.GetDefaultObject()->mFluidDensity; }
float UFGItemDescriptor::GetFluidViscosity(TSubclassOf< UFGItemDescriptor > inClass) { return inClass.GetDefaultObject()->mFluidViscosity; }
float UFGItemDescriptor::GetFluidFriction(TSubclassOf< UFGItemDescriptor > inClass) { return inClass.GetDefaultObject()->mFluidFriction; }
FColor UFGItemDescriptor::GetFluidColor(TSubclassOf< UFGItemDescriptor > inClass) { return inClass.GetDefaultObject()->mFluidColor; }
FLinearColor UFGItemDescriptor::GetFluidColorLinear(TSubclassOf< UFGItemDescriptor > inClass) { return inClass.GetDefaultObject()->mFluidColor.ReinterpretAsLinear(); }
FText UFGItemDescriptor::GetItemNameInternal() const{ return FText(); }
FText UFGItemDescriptor::GetItemDescriptionInternal() const{ return FText(); }
