// Fill out your copyright notice in the Description page of Project Settings.

#include "GrappleGun.h"
#include "RopeGrappleCharacter.h"
#include "Components/CapsuleComponent.h"

void UGrappleGun::BeginPlay()
{
	traceRadiusIncrease = (maxTraceRadius - minTraceRadius) / (traceBreakUps - 1);
	gunTipPosition = previousGunTipPosition = GetRopeOrigin();
}

void UGrappleGun::AssignOwningPlayer(ARopeGrappleCharacter* targetCharacter)
{
	if (targetCharacter == nullptr) return;
	owningPlayer = targetCharacter;
	owningPlayerGravity = FVector(0, 0, owningPlayer->GetCharacterMovement()->GravityScale * 100);
}

void UGrappleGun::Fire()
{
	if (!owningPlayer || !(owningPlayer->GetController())) return;

	if (FireSound) UGameplayStatics::PlaySoundAtLocation(this, FireSound, owningPlayer->GetActorLocation());
	if (FireAnimation) {
		UAnimInstance* AnimInstance = owningPlayer->GetMesh1P()->GetAnimInstance();
		if (AnimInstance) AnimInstance->Montage_Play(FireAnimation, 1.f);
	}

	GenerateRope();
}

void UGrappleGun::Release()
{
	if (rope) rope->Destroy();
	if (owningPlayer) {
		owningPlayer->ReleaseAnchor();
		owningPlayer->EndHanging();

		if (rope && !rope->IsAnchorMovable()) {
			owningPlayer->GetCharacterMovement()->ClearAccumulatedForces();
			owningPlayer->GetCharacterMovement()->AddImpulse((gunTipPosition - previousGunTipPosition) * momentumScale);
			owningPlayer->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Falling);
		}		
		hanging = false;
	}

	rope = nullptr;
}

void UGrappleGun::PullRopeIn()
{
	if (rope) {
		if (rope->GetLength() <= minRopeLength) return;
		rope->Shorten(ropeLengthChangeSpeed);

		FVector direction = rope->GetAnchorPoint() - GetRopeOrigin();
		if (owningPlayer->GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Walking && rope->GreaterThanRopeLength(direction)) {
			direction.Normalize();
			if (direction.Z <= zInfluenceRequiredForVertReelIn) owningPlayer->GetCharacterMovement()->AddInputVector(direction * ropeLengthChangeSpeed, true);
			else {
				direction.Z = FMath::Clamp(direction.Z, 0, zInfluenceRequiredForVertReelIn * 1.3f);
				owningPlayer->GetCharacterMovement()->AddImpulse(direction * ropeLengthChangeSpeed * verticalReelInForceMultiplier);
			}
		}
	}
}

void UGrappleGun::LetRopeOut()
{
	if (rope) {
		if (rope->GetLength() >= maxRopeLength) return;
		rope->Extend(ropeLengthChangeSpeed);
	}
}

FVector UGrappleGun::GetRopeOrigin()
{
	return GetComponentLocation() + GetComponentRotation().RotateVector(MuzzleOffset);
}

void UGrappleGun::SimulateOwningCharacter(float deltaTime)
{
	if (owningPlayer->GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Custom) {
		FVector velocity = gunTipPosition - previousGunTipPosition;
		previousGunTipPosition = gunTipPosition;
		gunTipPosition += velocity + owningPlayerGravity * (deltaTime * deltaTime);

		if (pendingForce.SquaredLength() > 0) {
			gunTipPosition += playerSwingInfluence * FVector(pendingForce.X, pendingForce.Y, 0) * (deltaTime * deltaTime);
			pendingForce = FVector::ZeroVector;
		}
	}
	else {
		previousGunTipPosition = gunTipPosition;
		gunTipPosition = GetRopeOrigin();
	}
}

void UGrappleGun::AddForceToPlayer(FVector direction)
{
	pendingForce = direction;
}

void UGrappleGun::RestrainOwningCharacter(URopePoint* endPoint, URopePoint* anchorPoint, float ropeLength)
{
	if (!owningPlayer) return;

	FVector ropeOrigin = GetRopeOrigin();
	if (owningPlayer->GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Falling && (anchorPoint->position - ropeOrigin).SquaredLength() > ropeLength * ropeLength) {
		owningPlayer->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Custom);
		owningPlayer->BeginHanging();
		hanging = true;
	}
	else if (owningPlayer->GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Custom) {
		FVector dummy(ropeOrigin.X, ropeOrigin.Y, anchorPoint->position.Z);
		owningPlayer->RotateGun(UKismetMathLibrary::FindLookAtRotation(ropeOrigin, dummy + owningPlayer->GetActorForwardVector() * 50));

		//constrain using Jakobsen's
		FVector difference = endPoint->position - gunTipPosition;
		float distance = difference.Length() / 2;
		difference.Normalize();
		difference *= distance;
		gunTipPosition += difference;

		//adjust for difference between grapple gun tip and player location and attempt to move there
		FHitResult movementBlocked;
		FVector playerLocation = owningPlayer->GetActorLocation();
		FVector destination = gunTipPosition + (playerLocation - ropeOrigin);
		owningPlayer->SetActorLocation(destination, true, &movementBlocked);

		//if movement was blocked, use the anchor's direction against the blocking object to project the goal destination out of the collision
		if (movementBlocked.GetActor()) {
			FHitResult movementBlockedSecondary;
			owningPlayer->SetActorLocation(GetNonCollidingLocation(destination, rope->GetAnchorNormal()), true, &movementBlockedSecondary);

			//if THAT movement was blocked, try again with normal off blocking collider (case where anchor is on horizontal surface)
			if (movementBlockedSecondary.GetActor()) {
				owningPlayer->SetActorLocation(GetNonCollidingLocation(destination, movementBlocked.Normal), true, &movementBlockedSecondary);
			}
		}

		endPoint->position = GetRopeOrigin();
		CheckForLanding();
	}
	else {
		endPoint->position = ropeOrigin;
	}
}

FVector UGrappleGun::GetNonCollidingLocation(FVector idealLocation, FVector blockingNormal)
{
	float playerColliderRadius = owningPlayer->GetCapsuleComponent()->GetScaledCapsuleRadius();
	FVector start = idealLocation + blockingNormal * 3 * playerColliderRadius;

	FHitResult hitActor;
	UKismetSystemLibrary::SphereTraceSingle(GetWorld(), start, idealLocation, playerColliderRadius, UEngineTypes::ConvertToTraceType(ECollisionChannel::ECC_Visibility), false, { owningPlayer }, EDrawDebugTrace::None, hitActor, true, FLinearColor::Red, FLinearColor::Green, 20.0f);
	return hitActor.Location + blockingNormal;
}

void UGrappleGun::CheckForLanding()
{
	const FVector PawnLocation = owningPlayer->GetActorLocation();
	FFindFloorResult FloorResult;
	owningPlayer->GetCharacterMovement()->FindFloor(PawnLocation, FloorResult, false);
	if (FloorResult.IsWalkableFloor() && owningPlayer->GetCharacterMovement()->IsValidLandingSpot(PawnLocation, FloorResult.HitResult))
	{
		owningPlayer->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Falling);
		owningPlayer->EndHanging();
		hanging = false;
	}
}

void UGrappleGun::GenerateRope()
{
	APlayerController* owningController = Cast<APlayerController>(owningPlayer->GetController());
	UCameraComponent* characterCamera = Cast<UCameraComponent>(owningPlayer->GetComponentByClass(UCameraComponent::StaticClass()));
	if (!owningController || !characterCamera) return;

	FVector startLocation = GetRopeOrigin();
	FHitResult hitAnchor = GetAnchorPoint(characterCamera->GetComponentLocation(), characterCamera->GetForwardVector());
	AActor* anchorObject = hitAnchor.GetActor();
	if (anchorObject) {
		FVector endLocation = hitAnchor.ImpactPoint;

		rope = GetWorld()->SpawnActor<ARope>();
		if (rope) {
			if (anchorObject->Tags.Contains(grappleAnchorTag)) {
				rope->SetObjectLocks(anchorObject, this);
				if (owningPlayer) owningPlayer->AnchorMovement(endLocation);
			}
			else if (anchorObject->Tags.Contains(grapplePullableTag)) rope->SetObjectLocks(anchorObject, this, true);
			else return; //eventual option for two-way pull objects..

			rope->SetMeshAndMaterial(mesh, defaultMaterial);
			rope->GeneratePoints(startLocation, endLocation);
			rope->SetAnchorNormal(hitAnchor.ImpactNormal);
		}
	}
}

FHitResult UGrappleGun::GetAnchorPoint(FVector startLocation, FVector direction)
{
	FHitResult outHit;
	float traceRadius = minTraceRadius;
	for (int i = 0; i < traceBreakUps; ++i) {
		UKismetSystemLibrary::SphereTraceSingle(GetWorld(), startLocation + (direction * maxRopeLength * i), startLocation + (direction * maxRopeLength * (i + 1)), traceRadius, UEngineTypes::ConvertToTraceType(grappleCollisionChannel), false, {}, EDrawDebugTrace::None, outHit, true, FLinearColor::Red, FLinearColor::Green, 1.0f);
		if (outHit.bBlockingHit) return outHit;

		traceRadius += traceRadiusIncrease;
	}

	return outHit;
}
