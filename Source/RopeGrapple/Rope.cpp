// Fill out your copyright notice in the Description page of Project Settings.


#include "Rope.h"
#include "GrappleGun.h"

ARope::ARope()
{
	PrimaryActorTick.bCanEverTick = true;
	splineComponent = CreateDefaultSubobject<USplineComponent>("Spline");
	splineComponent->bDrawDebug = true;
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

	if (anchorIsMovable) {
		SimulateAnchoredObject(DeltaTime);
		RestrainAnchoredObject();
	}
	else {
		float currentRopeLength = ropeLength + transitionaryOutDistance - (realDistanceBetweenPoints - transitionaryInDistance);
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

	//initialize visual spline
	splineComponent->ClearSplinePoints();
	for (int i = 0; i < ropePoints.Num(); ++i) {
		splineComponent->AddPoint({ (float)i, ropePoints[i]->position });
		ropeMeshes.Add(CreateSplineMesh());
	}

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
	FVector previousNormal = FVector::ZeroVector;
	for (int i = 1; i < transitionaryInIndex - 1; ++i) {
		FHitResult outHit;
		UKismetSystemLibrary::SphereTraceSingle(GetWorld(), ropePoints[i]->position + (FVector::UpVector * correctionTraceLength), 
			ropePoints[i]->position, ropePoints[i]->radius, UEngineTypes::ConvertToTraceType(ECC_Visibility), false, { this }, 
			EDrawDebugTrace::None, outHit, true, FLinearColor::Red, FLinearColor::Green, 0);
		
		if (outHit.bBlockingHit && outHit.ImpactNormal.Z >= majorityInfluence) {
			ProjectPoint(i, outHit.ImpactPoint);
			previousNormal = FVector::ZeroVector;
		}
		else if (outHit.bBlockingHit) {
			UKismetSystemLibrary::SphereTraceSingle(GetWorld(), ropePoints[i]->position + (outHit.ImpactNormal * correctionTraceLength), 
				ropePoints[i]->position, ropePoints[i]->radius, UEngineTypes::ConvertToTraceType(ECC_Visibility), false, { this }, 
				EDrawDebugTrace::None, outHit, true, FLinearColor::Red, FLinearColor::Green, 0);

			ProjectPoint(i, outHit.ImpactPoint, false);
			float angle = FMath::RadiansToDegrees(acosf(FVector::DotProduct(outHit.ImpactNormal, previousNormal)));
			if (previousNormal != FVector::ZeroVector && angle > 60) {
				HandleCorner(i - 1, i, previousNormal, outHit.ImpactNormal);
			}
			previousNormal = outHit.ImpactNormal;
		}
		else previousNormal = FVector::ZeroVector;
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

void ARope::HandleCorner(int indA, int indB, FVector aImpactNormal, FVector bImpactNormal)
{
	FVector current = ropePoints[indB]->position;
	FVector adjust = FVector(ropePoints[indA]->position.X - ropePoints[indB]->position.X, ropePoints[indA]->position.Y - ropePoints[indB]->position.Y, 0).GetSafeNormal();

	FVector goal = current + adjust * realDistanceBetweenPoints * 5;
	adjust *= 10.0f;

	FHitResult outHit;
	FVector lastHit;
	for (int i = 0; i < 40; ++i) {
		UKismetSystemLibrary::SphereTraceSingle(GetWorld(), current + (bImpactNormal * correctionTraceLength),
			current, 10, UEngineTypes::ConvertToTraceType(ECC_Visibility), false, { this },
			EDrawDebugTrace::None, outHit, true, FLinearColor::Red, FLinearColor::Green, 5);

		if (outHit.bBlockingHit) lastHit = outHit.ImpactPoint;
		else {
			DrawDebugSphere(GetWorld(), lastHit, 5, 8, FColor(181, 0, 200), false, 5, 2, 1);
			int modifiedInd = (FVector::Distance(lastHit, ropePoints[indA]->position) < FVector::Distance(lastHit, ropePoints[indB]->position)) ? indA : indB;
			ropePoints[modifiedInd]->position = lastHit;
			ropePoints[modifiedInd]->cornerFlag = true;
			break;
		}

		current += adjust;
	}	
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
	if (ropePoints[indB]->isAnchor || ropePoints[indB]->cornerFlag) ropePoints[indA]->position = ropePoints[indA]->position - difference;
	else if (ropePoints[indA]->isAnchor || ropePoints[indA]->cornerFlag) ropePoints[indB]->position = ropePoints[indB]->position + difference;
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
	if (GreaterThanRopeLength(distance)) {
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
			UKismetSystemLibrary::BoxTraceSingle(GetWorld(), start, end, boxExtents, anchorObject->GetActorRotation(), UEngineTypes::ConvertToTraceType(ECollisionChannel::ECC_Visibility), false, { anchorObject }, EDrawDebugTrace::None, hitActor, true, FLinearColor::Red, FLinearColor::Green, 0.2f);

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

USplineMeshComponent* ARope::CreateSplineMesh()
{
	USplineMeshComponent* splineMesh = NewObject<USplineMeshComponent>(this, USplineMeshComponent::StaticClass());
	splineMesh->SetStaticMesh(mesh);
	splineMesh->SetForwardAxis(ESplineMeshAxis::Z, true);
	splineMesh->RegisterComponentWithWorld(GetWorld());
	splineMesh->SetMobility(EComponentMobility::Movable);
	splineMesh->AttachToComponent(splineComponent, FAttachmentTransformRules::KeepRelativeTransform);
	return splineMesh;
}

void ARope::GenerateLine()
{
	FColor color; float adjust;
	for (int i = 0; i < ropePoints.Num(); ++i) {
		color = (i == transitionaryOutIndex) ? FColor::Red : (i == transitionaryInIndex) ? FColor::Yellow : FColor::Blue;
		if ((ropePoints[i]->cornerFlag)) color = FColor::Purple;
		adjust = (i == transitionaryOutIndex) ? 0.75f : 1.0f;
		DrawDebugSphere(GetWorld(), ropePoints[i]->position, ropePoints[i]->radius * adjust, 16, color, false, 0);
	}

	for (int i = 0; i < ropePoints.Num(); ++i) {
		splineComponent->SetLocationAtSplinePoint(i, ropePoints[i]->position, ESplineCoordinateSpace::World);
	}

	for (int i = 0; i < ropePoints.Num() - 1; ++i) {
		// define the positions of the points and tangents
		FVector StartPoint = splineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Type::Local);
		FVector StartTangent = splineComponent->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::Type::Local);
		FVector EndPoint = splineComponent->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::Type::Local);
		FVector EndTangent = splineComponent->GetTangentAtSplinePoint(i + 1, ESplineCoordinateSpace::Type::Local);
		if (i == 0) StartPoint = grappleSource->GetRopeOrigin();
		ropeMeshes[i]->SetStartAndEnd(StartPoint, StartTangent, EndPoint, EndTangent, true);
	}
}

void ARope::Shorten(float rateOfChange)
{
	if (ropePoints.Num() <= 3) return; //a rope is minimum 3 points - start, end, and the artificial transition out point.

	if (transitionaryOutDistance > 0) transitionaryOutDistance -= rateOfChange;
	else transitionaryInDistance -= rateOfChange;

	if (transitionaryInDistance <= 0) {
		ropePoints.RemoveAt(transitionaryInIndex);
		ropeMeshes[transitionaryInIndex]->DestroyComponent();
		ropeMeshes.RemoveAt(transitionaryInIndex);
		splineComponent->RemoveSplinePoint(transitionaryInIndex);

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

		splineComponent->AddSplinePointAtIndex(ropePoints[transitionaryOutIndex + 1]->position, transitionaryOutIndex, ESplineCoordinateSpace::World);
		ropeMeshes.EmplaceAt(transitionaryOutIndex, CreateSplineMesh());	

		transitionaryOutDistance = 0.0f;
		ropeLength += realDistanceBetweenPoints;
	}
}
