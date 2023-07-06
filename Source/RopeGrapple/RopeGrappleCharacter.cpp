// Copyright Epic Games, Inc. All Rights Reserved.

#include "RopeGrappleCharacter.h"
#include "RopeGrappleProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"


ARopeGrappleCharacter::ARopeGrappleCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
		
	// Create a CameraComponent	
	firstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	firstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	firstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f)); // Position the camera
	firstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	mesh1P->SetOnlyOwnerSee(true);
	mesh1P->SetupAttachment(firstPersonCameraComponent);
	mesh1P->bCastDynamicShadow = false;
	mesh1P->CastShadow = false;
	mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	// Create a Grapple Gun	
	grappleGunPivot = CreateDefaultSubobject<USphereComponent>(TEXT("GrappleGunPivot"));
	grappleGunPivot->SetupAttachment(mesh1P, FName(TEXT("GripPoint")));

	grappleGun1 = CreateDefaultSubobject<UGrappleGun>(TEXT("GrappleGun"));
	grappleGun1->SetupAttachment(grappleGunPivot);
	grappleGun1->AssignOwningPlayer(this);
}

void ARopeGrappleCharacter::Tick(float DeltaTime)
{
	SetFOV();
}

void ARopeGrappleCharacter::Respawn()
{
	SetActorLocation(respawnLocation, false, nullptr, ETeleportType::ResetPhysics);
}

void ARopeGrappleCharacter::AnchorMovement(FVector _anchorPoint)
{
	anchored = true;
	anchorPoint = _anchorPoint;
}

void ARopeGrappleCharacter::BeginHanging()
{
	hanging = true;
}

void ARopeGrappleCharacter::EndHanging()
{
	grappleGunPivot->SetRelativeRotation(FRotator(0, 90, 0));
	hanging = false;
}

void ARopeGrappleCharacter::RotateGun(FRotator rot)
{
	grappleGunPivot->SetWorldRotation(rot);
}

void ARopeGrappleCharacter::BeginPlay()
{
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* playerController = Cast<APlayerController>(Controller)) {
		if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer())) {
			subsystem->AddMappingContext(defaultMappingContext, 1);
			subsystem->AddMappingContext(fireMappingContext, 0);
		}
	}

	defaultWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
	defaultFOV = firstPersonCameraComponent->FieldOfView;
	respawnLocation = GetActorLocation();
}

void ARopeGrappleCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* enhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		//Jumping
		enhancedInputComponent->BindAction(jumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
		enhancedInputComponent->BindAction(jumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		//Moving
		enhancedInputComponent->BindAction(moveAction, ETriggerEvent::Triggered, this, &ARopeGrappleCharacter::Move);

		//Sprinting
		enhancedInputComponent->BindAction(sprintAction, ETriggerEvent::Triggered, this, &ARopeGrappleCharacter::ToggleSprint);
		enhancedInputComponent->BindAction(sprintEndAction, ETriggerEvent::Triggered, this, &ARopeGrappleCharacter::ToggleSprint);

		//Looking
		enhancedInputComponent->BindAction(lookAction, ETriggerEvent::Triggered, this, &ARopeGrappleCharacter::Look);

		//Grapple
		enhancedInputComponent->BindAction(fireAction, ETriggerEvent::Triggered, grappleGun1, &UGrappleGun::Fire);
		enhancedInputComponent->BindAction(releaseAction, ETriggerEvent::Triggered, grappleGun1, &UGrappleGun::Release);
		enhancedInputComponent->BindAction(pullInAction, ETriggerEvent::Triggered, grappleGun1, &UGrappleGun::PullRopeIn);
		enhancedInputComponent->BindAction(letOutAction, ETriggerEvent::Triggered, grappleGun1, &UGrappleGun::LetRopeOut);
	}
}

void ARopeGrappleCharacter::Move(const FInputActionValue& value)
{
	FVector2D movementVector = value.Get<FVector2D>();
	if (Controller != nullptr)
	{
		if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Custom) {
			grappleGun1->AddForceToPlayer(firstPersonCameraComponent->GetRightVector() * movementVector.X + firstPersonCameraComponent->GetForwardVector() * movementVector.Y);
		}
		else if (anchored) { //project movement into allowed radius
			FVector destination = grappleGun1->GetRopeOrigin() + GetActorForwardVector() * movementVector.Y + GetActorRightVector() * movementVector.X;
			FVector distance = anchorPoint - destination;
			if (distance.Length() >= grappleGun1->GetRopeLength()) {
				FVector correctedDistance = distance;
				correctedDistance.Normalize();
				correctedDistance *= grappleGun1->GetRopeLength();

				FVector difference = correctedDistance - grappleGun1->GetRopeOrigin();
				FVector projected = difference.ProjectOnToNormal(GetActorForwardVector());
				movementVector.Y = (GetActorForwardVector().Dot(distance) > 0) ? FMath::Abs(projected.X) : -FMath::Abs(projected.X);
				movementVector.X = 0;
			}
			AddMovementInput(GetActorForwardVector(), movementVector.Y);
			AddMovementInput(GetActorRightVector(), movementVector.X);
		}
		else {
			AddMovementInput(GetActorForwardVector(), movementVector.Y);
			AddMovementInput(GetActorRightVector(), movementVector.X);
		}
	}
}

void ARopeGrappleCharacter::Look(const FInputActionValue& value)
{
	FVector2D lookAxisVector = value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(lookAxisVector.X);
		AddControllerPitchInput(lookAxisVector.Y);
	}	
}

void ARopeGrappleCharacter::ToggleSprint()
{
	if (sprinting) GetCharacterMovement()->MaxWalkSpeed = defaultWalkSpeed;
	else GetCharacterMovement()->MaxWalkSpeed = sprintSpeed;

	sprinting = !sprinting;
}

void ARopeGrappleCharacter::SetFOV()
{
	lastFOV = firstPersonCameraComponent->FieldOfView;
	float speed = FVector(GetVelocity().X, GetVelocity().Y, 0).Length();

	if (sprinting && FMath::Abs(lastFOV - sprintFOV) <= allowedError) return;
	if (!sprinting && FMath::Abs(lastFOV - defaultFOV) <= allowedError) return;

	if (speed >= defaultWalkSpeed) {
		float FOVpercent = (speed - defaultWalkSpeed) / (sprintSpeed - defaultWalkSpeed);
		float newFOV = defaultFOV + (sprintFOV - defaultFOV) * FOVpercent;
		firstPersonCameraComponent->FieldOfView = (newFOV - lastFOV > FOVMaxChangePerTick) ? lastFOV + FOVMaxChangePerTick : newFOV;
	}
}
