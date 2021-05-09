#pragma once
#include "Components/ScaleBox.h"
#include "Blueprint/UserWidget.h"
#include "Animation/AnimInstance.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialExpression.h"
#include "Particles/ParticleSystemComponent.h"
#include "MovieSceneSequence.h"
#include "FGCircuitSubsystem.h"
#include "FGGameMode.h"
#include "FGGameState.h"

// Classes below don't really have custom serialization, just
// fixes for loading legacy data and pre-initializing contents
//
#define APPEND_DEFAULT_SERIALIZABLE_NATIVE_CLASSES(Appender) \
    Appender(AActor::StaticClass()); \
    Appender(UActorComponent::StaticClass()); \
    Appender(UMovieSceneSequence::StaticClass()); \
    Appender(UUserWidget::StaticClass()); \
    Appender(UPrimitiveComponent::StaticClass()); \
    Appender(USkinnedMeshComponent::StaticClass()); \
    Appender(USkeletalMeshComponent::StaticClass()); \
    Appender(UCharacterMovementComponent::StaticClass()); \
    Appender(UPawnMovementComponent::StaticClass()); \
    Appender(UMovementComponent::StaticClass()); \
    Appender(UParticleSystemComponent::StaticClass()); \
    Appender(USimpleConstructionScript::StaticClass()); \
    Appender(UInheritableComponentHandler::StaticClass()); \
    Appender(USCS_Node::StaticClass()); \
    Appender(UPostProcessComponent::StaticClass()); \
    Appender(UPointLightComponent::StaticClass()); \
    Appender(ULocalLightComponent::StaticClass()); \
    Appender(ULightComponent::StaticClass()); \
    Appender(UUserWidget::StaticClass()); \
    Appender(UScaleBox::StaticClass()); \
    Appender(UMaterialExpression::StaticClass()); \
    Appender(UCurveLinearColor::StaticClass()); \
    Appender(USkeletalMeshSocket::StaticClass()); \
    Appender(UAnimInstance::StaticClass()); \
    Appender(UStaticMeshComponent::StaticClass()); \
    Appender(UMovieScene::StaticClass()); \
    Appender(AFGCircuitSubsystem::StaticClass()); \
    Appender(AFGGameMode::StaticClass()); \
    Appender(AFGGameState::StaticClass());
