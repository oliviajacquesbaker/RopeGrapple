// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Rope.h"
#include "GrappleGun.generated.h"

class ARopeGrappleCharacter;

UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ROPEGRAPPLE_API UGrappleGun : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	void AssignOwningPlayer(ARopeGrappleCharacter* targetCharacter);
	void Fire();
	void Release();
	void PullRopeIn();
	void LetRopeOut();

	void RestrainOwningCharacter(URopePoint* endPoint, URopePoint* anchorPoint, float ropeLength);
	void SimulateOwningCharacter(float deltaTime);
	void AddForceToPlayer(FVector direction);

	FVector GetRopeOrigin();
	bool IsHanging() { return hanging; };
	float GetRopeLength() { return (rope) ? rope->GetLength() : 0.0f; };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		USoundBase* fireSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		USoundClass* fireSoundClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		USoundMix* fireSoundMix;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		UAnimMontage* FireAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		FVector MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		float minTraceRadius = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		float maxTraceRadius = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		float maxRopeLength = 1000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		float minRopeLength = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		int traceBreakUps = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		TEnumAsByte<ECollisionChannel> grappleCollisionChannel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		FName grappleAnchorTag = "GrappleAnchor";
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		FName grapplePullableTag = "GrapplePullable";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		float ropeLengthChangeSpeed = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		float zInfluenceRequiredForVertReelIn = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		float verticalReelInForceMultiplier = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		float playerSwingInfluence = 2700;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		float momentumScale = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		float volumeDropOffScale = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		float minimumVolume = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple Options")
		float maximumVolume = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple Options")
		UStaticMesh* mesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grapple Options")
		class UMaterialInterface* defaultMaterial;


protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void GenerateRope(FHitResult hitAnchor);
	void SearchForRope();
	FHitResult GetAnchorPoint(FVector startLocation, FVector direction);
	FVector GetNonCollidingLocation(FVector idealLocation, FVector blockingNormal, FVector startingLocation);
	void CheckForLanding();
	float GetVolumeByDistance(float distance);

	ARopeGrappleCharacter* owningPlayer;
	ARope* rope;
	float traceRadiusIncrease;
	FVector pendingForce;

	FVector gunTipPosition;
	FVector previousGunTipPosition;
	FVector owningPlayerGravity;
	bool hanging = false;
};
