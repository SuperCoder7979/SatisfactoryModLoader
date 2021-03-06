// This file has been automatically generated by the Unreal Header Implementation tool

#include "Equipment/FGWeaponInstantFire.h"
#include "DamageTypes/FGDamageType.h"

AFGWeaponInstantFire::AFGWeaponInstantFire() : Super() {
	this->mInstantHitDamage = 10;
	this->mWeaponTraceLength = 10000;
	this->mMagSize = 5;
	this->mDamageTypeClass = UFGDamageType::StaticClass();
	this->mReloadTime = 1.5;
	this->mFireRate = 0.5;
	this->mEquipmentSlot = EEquipmentSlot::ES_ARMS;
	this->PrimaryActorTick.TickGroup = TG_PrePhysics; this->PrimaryActorTick.EndTickGroup = TG_PrePhysics; this->PrimaryActorTick.bTickEvenWhenPaused = false; this->PrimaryActorTick.bCanEverTick = true; this->PrimaryActorTick.bStartWithTickEnabled = false; this->PrimaryActorTick.bAllowTickOnDedicatedServer = true; this->PrimaryActorTick.TickInterval = 0;
	this->bOnlyRelevantToOwner = true;
	this->bNetUseOwnerRelevancy = true;
	this->SetReplicates(true);
}
void AFGWeaponInstantFire::FireAmmunition_Implementation(){ }
void AFGWeaponInstantFire::InstantFire(){ }
void AFGWeaponInstantFire::CalcWeaponFire(FVector startTrace, FVector endTrace, FHitResult& out_hitResult){ }
void AFGWeaponInstantFire::ProcessInstantHit(const FHitResult& hitResult){ }
void AFGWeaponInstantFire::Server_NotifyHit_Implementation(const FHitResult& hitResult){ }
bool AFGWeaponInstantFire::Server_NotifyHit_Validate(const FHitResult& hitResult){ return bool(); }
void AFGWeaponInstantFire::Server_NotifyMiss_Implementation(){ }
bool AFGWeaponInstantFire::Server_NotifyMiss_Validate(){ return bool(); }
void AFGWeaponInstantFire::ProcessInstantHit_Confirmed(const FHitResult& hitResult){ }
bool AFGWeaponInstantFire::ShouldDealDamage(AActor* testActor) const{ return bool(); }
void AFGWeaponInstantFire::DealDamage(const FHitResult& hitResult){ }
void AFGWeaponInstantFire::PlayImpactEffect(const FHitResult& hitResult){ }
