// Copyright Epic Games, Inc. All Rights Reserved.


#include "TP_WeaponComponent.h"
#include "RopeGrappleCharacter.h"
#include "RopeGrappleProjectile.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

void UTP_WeaponComponent::Fire()
{
	if (Character == nullptr || Character->GetController() == nullptr) return;	
	
	//if (FireSound != nullptr) UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
	
	if (FireAnimation != nullptr)
	{
		UAnimInstance* AnimInstance = Character->GetMesh1P()->GetAnimInstance();
		if (AnimInstance != nullptr) {
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
}

void UTP_WeaponComponent::AttachWeapon(ARopeGrappleCharacter* TargetCharacter)
{
	if (TargetCharacter == nullptr) return;
	Character = TargetCharacter;
}