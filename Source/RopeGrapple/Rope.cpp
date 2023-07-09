// Fill out your copyright notice in the Description page of Project Settings.


#include "Rope.h"
#include "GrappleGun.h"

ARope::ARope()
{
	PrimaryActorTick.bCanEverTick = true;
	lineRenderer = CreateDefaultSubobject<ULineBatchComponent>(TEXT("Line Renderer"));
}

void ARope::BeginPlay()
{
	Super::BeginPlay();	
	SetTickGroup(ETickingGroup::TG_DuringPhysics);
}

void ARope::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (int i = 0; i < ropePoints.Num(); ++i) {
		ropePoints[i]->UpdatePoint(DeltaTime);
	}

	float currentRopeLength = ropeLength + transitionaryOutDistance - (realDistanceBetweenPoints - transitionaryInDistance);
	if (anchorIsMovable) {
		SimulateAnchoredObject(DeltaTime);
		RestrainAnchoredObject();
	}
	else {
		grappleSource->SimulateOwningCharacter(DeltaTime);
		grappleSource->RestrainOwningCharacter(ropePoints[0], ropePoints[ropePoints.Num() - 1], currentRopeLength);
	}

	RestrainPoints(constraintIterations / 3);
	ProjectPoints();
	RestrainPoints(2 * constraintIterations / 3);
	GenerateLine();
}

void ARope::GeneratePoints(FVector startLocation, FVector endLocation)
{
	if (!GetWorld()) return;
	ropePoints.Empty();

	ropeLength = FVector::Dist(startLocation, endLocation);
	int segments = FMath::CeilToInt(ropeLength / desiredDistanceBetweenPoints);
	realDistanceBetweenPoints = ropeLength / segments;
	transitionaryInDistance = realDistanceBetweenPoints;

	FVector location = startLocation;
	FVector displacement = endLocation - startLocation;
	displacement.Normalize();
	displacement *= realDistanceBetweenPoints;

	//there should be one more point than segments
	for (int i = 0; i <= segments; ++i) {
		ropePoints.Emplace(NewObject<URopePoint>());
		ropePoints[i]->RegisterComponent();
		ropePoints[i]->position = location;
		ropePoints[i]->previousPosition = location;

		location += displacement;
	}

	//insert an artificial point at the same position as the anchor to be used as the point transitioning out (adding points)
	transitionaryOutIndex = segments;
	ropePoints.EmplaceAt(transitionaryOutIndex, NewObject<URopePoint>());
	ropePoints[transitionaryOutIndex]->RegisterComponent();
	ropePoints[transitionaryOutIndex]->position = ropePoints[transitionaryOutIndex + 1]->position;
	ropePoints[transitionaryOutIndex]->previousPosition = ropePoints[transitionaryOutIndex + 1]->position;

	//the point right before the transitioning out point will be used as the point transitioning in (removing points)
	transitionaryInIndex = transitionaryOutIndex - 1;

	//set anchor information
	if(!anchorIsMovable) ropePoints[ropePoints.Num() - 1]->isAnchor = true;
	anchorObjectImpactOffset = anchorObject->GetActorLocation() - ropePoints[ropePoints.Num() - 1]->position;

	ropeLength *= initialGiveMultiplier;
}

void ARope::RestrainPoints(int iters)
{
	for (int iterations = 0; iterations < iters; ++iterations) {
		//the point we're holding is always located at the tip of the grapple gun
		ropePoints[0]->position = grappleSource->GetRopeOrigin();

		//all the "middle" points are held normally
		for (int i = 1; i < transitionaryInIndex - 1; ++i) {
			Constrain(i, i - 1, realDistanceBetweenPoints);
			Constrain(i + 1, i, realDistanceBetweenPoints);
		}

		//distance between the two transitionary points is the transition IN dist, dist between out transition and anchor is transition OUT dist
		if(transitionaryInIndex > 0) Constrain(transitionaryInIndex, transitionaryInIndex - 1, realDistanceBetweenPoints);
		Constrain(transitionaryOutIndex, transitionaryInIndex, transitionaryInDistance);
		if(transitionaryOutIndex < ropePoints.Num() - 1) Constrain(transitionaryOutIndex + 1, transitionaryOutIndex, transitionaryOutDistance);
	}	
}

void ARope::ProjectPoints()
{
	for (int i = 1; i < transitionaryInIndex - 1; ++i) {
		FHitResult outHit;
		UKismetSystemLibrary::SphereTraceSingle(GetWorld(), ropePoints[i]->position + (FVector::UpVector * correctionTraceLength), 
			ropePoints[i]->position, ropePoints[i]->radius, UEngineTypes::ConvertToTraceType(ECC_Visibility), false, { this }, 
			EDrawDebugTrace::None, outHit, true, FLinearColor::Red, FLinearColor::Green, 0);
		
		if (outHit.bBlockingHit && outHit.ImpactNormal.Z >= majorityInfluence) ProjectPoint(i, outHit.ImpactPoint);
		else if (outHit.bBlockingHit) {
			UKismetSystemLibrary::SphereTraceSingle(GetWorld(), ropePoints[i]->position + (outHit.ImpactNormal * correctionTraceLength), 
				ropePoints[i]->position, ropePoints[i]->radius, UEngineTypes::ConvertToTraceType(ECC_Visibility), false, { this }, 
				EDrawDebugTrace::ForDuration, outHit, true, FLinearColor::Red, FLinearColor::Green, 0);
			ProjectPoint(i, outHit.ImpactPoint, false);
		}
	}
}

void ARope::ProjectPoint(int ind, FVector impactPoint, bool groundCollision)
{
	float correctionWeight = (groundCollision) ? 0.9 : 0.7;
	FVector correctedPrevPos = ropePoints[ind]->previousPosition + (impactPoint - ropePoints[ind]->previousPosition) * correctionWeight;
	ropePoints[ind]->previousPosition = FVector(correctedPrevPos.X, correctedPrevPos.Y, ropePoints[ind]->previousPosition.Z);
	ropePoints[ind]->position = impactPoint;
	if (!groundCollision) ropePoints[ind]->position.Z = ropePoints[ind]->previousPosition.Z;

	ropePoints[ind]->collisionsResolved += 2;
}

void ARope::Constrain(int indA, int indB, float constraintDist)
{
	float distance, percent;
	FVector difference;

	difference = ropePoints[indA]->position - ropePoints[indB]->position;
	distance = difference.Length() - constraintDist * stiffness;
	percent = (constraintDist > 0) ? (distance / constraintDist) : 1;
	percent = FMath::Clamp(percent, 0, 1);

	difference *= percent;
	if (ropePoints[indB]->isAnchor) ropePoints[indA]->position = ropePoints[indA]->position - difference;
	else if (ropePoints[indA]->isAnchor) ropePoints[indB]->position = ropePoints[indB]->position + difference;
	else {
		difference /= 2;
		ropePoints[indB]->position = ropePoints[indB]->position + difference;
		ropePoints[indA]->position = ropePoints[indA]->position - difference;
	}
}

void ARope::SimulateAnchoredObject(float DeltaTime)
{
	//allow actual object to simulate its own physics - simply track it for later calculation
	previousAnchorObjectPosition = anchorObjectPosition;
	anchorObjectPosition = anchorObject->GetActorLocation();
}

void ARope::RestrainAnchoredObject()
{
	//we only force movement if the rope is taut / actively pulling the object
	FVector distance = anchorObjectPosition - ropePoints[0]->position;
	if (distance.Length() >= GetLength() * initialGiveMultiplier) {
		//calculate a position projected into the allowed radius
		FVector correctedDistance = distance;
		correctedDistance.Normalize();
		correctedDistance *= GetLength() * initialGiveMultiplier;

		//negotiate the physics calculated position with our corrections
		bool playerAboveObject = ropePoints[0]->position.Z - ropePoints[ropePoints.Num() - 1]->position.Z >= GetLength() * majorityInfluence;
		float correctionWeight = (playerAboveObject) ? 0.07 : 0.05;
		float zPos = (!playerAboveObject) ? anchorObjectPosition.Z + (correctedDistance.Z - anchorObjectPosition.Z) * correctionWeight :
			ropePoints[0]->position.Z + correctedDistance.Z + (anchorObjectPosition.Z - ropePoints[0]->position.Z) * correctionWeight;
		anchorObjectPosition = FVector(ropePoints[0]->position.X + correctedDistance.X, ropePoints[0]->position.Y + correctedDistance.Y, zPos);

		FHitResult blockingHit;
		anchorObject->SetActorLocation(anchorObjectPosition, true, &blockingHit);

		//if moving to the corrected location was blocked...
		if (blockingHit.GetActor()) {
			FVector origin, boxExtents;
			anchorObject->GetActorBounds(true, origin, boxExtents, false);
			FVector start = anchorObjectPosition + FVector::UpVector * boxExtents.GetAbsMax();
			FVector end = anchorObjectPosition - FVector::UpVector * boxExtents.GetAbsMax();

			FHitResult hitActor;
			UKismetSystemLibrary::BoxTraceSingle(GetWorld(), start, end, boxExtents, anchorObject->GetActorRotation(), UEngineTypes::ConvertToTraceType(ECollisionChannel::ECC_Visibility), false, { anchorObject }, EDrawDebugTrace::ForDuration, hitActor, true, FLinearColor::Red, FLinearColor::Green, 0.2f);

			//only project out of the collision if the normal is reasonable (not a vertical wall)
			bool notRandomlyUp = (hitActor.Location.Z - previousAnchorObjectPosition.Z < 50) || playerAboveObject;
			if (hitActor.bBlockingHit && notRandomlyUp && FMath::Abs(hitActor.ImpactNormal.X) < minorityInfluence && FMath::Abs(hitActor.ImpactNormal.Y) < minorityInfluence) {
				anchorObject->SetActorLocation(hitActor.Location, false);

				FRotator normalDiff = UKismetMathLibrary::FindLookAtRotation(hitActor.ImpactNormal, anchorObject->GetActorUpVector());
				float angleDiff = FMath::Abs(normalDiff.Yaw + normalDiff.Pitch + normalDiff.Roll);
				if (angleDiff > 5 && angleDiff < 100) {
					FRotator NewRotator = UKismetMathLibrary::MakeRotationFromAxes(anchorObject->GetActorForwardVector(), anchorObject->GetActorRightVector(), hitActor.ImpactNormal);
					anchorObject->SetActorRotation(NewRotator);
				}
			}
			else if (playerAboveObject) { //if we're pulling it up, we can project up a perpendicular surface
				start = anchorObjectPosition + blockingHit.Normal * boxExtents.GetAbsMax();
				end = anchorObjectPosition - blockingHit.Normal * boxExtents.GetAbsMax();
				UKismetSystemLibrary::BoxTraceSingle(GetWorld(), start, end, boxExtents, anchorObject->GetActorRotation(), UEngineTypes::ConvertToTraceType(ECollisionChannel::ECC_Visibility), false, { anchorObject }, EDrawDebugTrace::None, hitActor, true, FLinearColor::Red, FLinearColor::Green, 20.0f);
				if (hitActor.bBlockingHit) {
					anchorObject->SetActorLocation(hitActor.Location, false);
				}
			}
		}
	}	

	anchorObjectPosition = anchorObject->GetActorLocation();
	ropePoints[ropePoints.Num() - 1]->position = anchorObject->GetActorLocation();
}

void ARope::GenerateLine()
{
	TArray<FBatchedLine> ropeSegments;
	FColor color; float adjust;
	for (int i = 0; i < ropePoints.Num(); ++i) {
		color = (i == transitionaryOutIndex) ? FColor::Red : (i == transitionaryInIndex) ? FColor::Yellow : FColor::Blue;
		adjust = (i == transitionaryOutIndex) ? 0.75f : 1.0f;
		//DrawDebugSphere(GetWorld(), ropePoints[i]->position, ropePoints[i]->radius * adjust, 16, color, false, 0);
		if (i > 0) ropeSegments.Emplace(ropePoints[i - 1]->position, ropePoints[i]->position, FLinearColor::Black, 0.02, 5, 0);
	}

	lineRenderer->DrawLines(ropeSegments);
}

void ARope::Shorten(float rateOfChange)
{
	if (ropePoints.Num() <= 3) return; //a rope is minimum 3 points - start, end, and the artificial transition out point.

	if (transitionaryOutDistance > 0) transitionaryOutDistance -= rateOfChange;
	else transitionaryInDistance -= rateOfChange;

	if (transitionaryInDistance <= 0) {
		ropePoints.RemoveAt(transitionaryInIndex);
		--transitionaryInIndex;
		--transitionaryOutIndex;

		transitionaryInDistance = realDistanceBetweenPoints;
		ropeLength -= realDistanceBetweenPoints;
	}
}

void ARope::Extend(float rateOfChange)
{
	if (transitionaryInDistance < realDistanceBetweenPoints) transitionaryInDistance += rateOfChange;
	else transitionaryOutDistance += rateOfChange;

	if (transitionaryOutDistance >= realDistanceBetweenPoints) {
		++transitionaryOutIndex;
		++transitionaryInIndex;

		ropePoints.EmplaceAt(transitionaryOutIndex, NewObject<URopePoint>());
		ropePoints[transitionaryOutIndex]->RegisterComponent();
		ropePoints[transitionaryOutIndex]->position = ropePoints[transitionaryOutIndex + 1]->position;
		ropePoints[transitionaryOutIndex]->previousPosition = ropePoints[transitionaryOutIndex + 1]->position;

		transitionaryOutDistance = 0.0f;
		ropeLength += realDistanceBetweenPoints;
	}
}
