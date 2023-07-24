// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RopePoint.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/LineBatchComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Rope.generated.h"

UCLASS()
class ROPEGRAPPLE_API ARope : public AActor
{
	GENERATED_BODY()
	
public:	
	ARope();
	virtual void Tick(float DeltaTime) override;
	void GeneratePoints(FVector startLocation, FVector endLocation);
	void GenerateLine();

	void Extend(float rateOfChange);
	bool Shorten(float rateOfChange);

	void SetObjectLocks(AActor* anchor, class UGrappleGun* ropeSource, bool anchorCanMove = false) { anchorObject = anchor; grappleSource = ropeSource; anchorIsMovable = anchorCanMove; anchorObjectPosition = previousAnchorObjectPosition = anchorObject->GetActorLocation();}
	float GetLength() { return ropeLength + transitionaryOutDistance - (realDistanceBetweenPoints - transitionaryInDistance); };
	float GetInitialGiveMultiplier() { return initialGiveMultiplier; };
	FVector GetHeldPoint() { return ropePoints[0]->position; };
	FVector GetAnchorPoint() { return ropePoints[ropePoints.Num() - 1]->position; };
	void SetAnchorNormal(FVector normal) { anchorNormal = normal; };
	FVector GetAnchorNormal() { return anchorNormal; };
	bool IsAnchorMovable() { return anchorIsMovable; };
	bool GreaterThanRopeLength(FVector comparisonVector) { ropeTempLength = GetLength() * initialGiveMultiplier; return comparisonVector.SquaredLength() >= ropeTempLength * ropeTempLength; };
	void SetMeshAndMaterial(UStaticMesh* mesh_, UMaterialInterface* material_) { mesh = mesh_; defaultMaterial = material_; };


protected:
	virtual void BeginPlay() override;
	void RestrainPoints(int iterations);
	void ProjectPoints();
	void ProjectPoint(int ind, FVector impactPoint, bool zCorrectionAllowed = true);
	void HandleCorner(int indA, int indB, FVector aImpactNormal, FVector bImpactNormal);
	void Constrain(int indA, int indB, float constraintDist);
	void SimulateAnchoredObject(float DeltaTime);
	void RestrainAnchoredObject();
	USplineMeshComponent* CreateSplineMesh();

	UPROPERTY(VisibleAnywhere, Category = "Grapple Options")
		int constraintIterations = 100;
	UPROPERTY(VisibleAnywhere, Category = "Grapple Options")
		float desiredDistanceBetweenPoints = 50.0f;
	UPROPERTY(VisibleAnywhere, Category = "Grapple Options")
		float stiffness = 0.93f;
	UPROPERTY(VisibleAnywhere, Category = "Grapple Options")
		float playerCausedTension = 50.0f;
	UPROPERTY(VisibleAnywhere, Category = "Grapple Options")
		TArray<URopePoint*> ropePoints;
	UPROPERTY(VisibleAnywhere, Category = "Grapple Options")
		USplineComponent* splineComponent;
	UPROPERTY(VisibleAnywhere, Category = "Grapple Options")
		TArray<USplineMeshComponent*> ropeMeshes;

	AActor* anchorObject;
	class UGrappleGun* grappleSource;
	FVector anchorNormal;
	float realDistanceBetweenPoints;
	float realStiffness;
	float ropeLength;
	float initialGiveMultiplier = 1.05f;

	int transitionaryOutIndex = -1;
	int transitionaryInIndex = -1;
	float transitionaryOutDistance = 0.1f;
	float transitionaryInDistance = 0.0f;
	float errorAcceptance = 0.01f;
	float correctionTraceLength = 100.0f;
	float majorityInfluence = 0.75f;
	float minorityInfluence = 0.4f;

	FVector anchorObjectPosition;
	FVector previousAnchorObjectPosition;
	float ropeTempLength;

	bool anchorIsMovable;
	FVector anchorObjectImpactOffset;

	UStaticMesh* mesh;
	class UMaterialInterface* defaultMaterial;
};
