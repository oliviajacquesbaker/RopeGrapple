// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RopePoint.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ROPEGRAPPLE_API URopePoint : public UActorComponent
{
	GENERATED_BODY()

public:	
	URopePoint();
	void UpdatePoint(float deltaTime);
	void ApplyForce(FVector force, float deltaTime);

	UPROPERTY(EditAnywhere, Category = "Grapple Options")
		float mass = 100.0;
	UPROPERTY(EditAnywhere, Category = "Grapple Options")
		float radius = 5.0;
	UPROPERTY(EditAnywhere, Category = "Grapple Options")
		bool isAnchor = false;
	UPROPERTY(EditAnywhere, Category = "Grapple Options")
		FVector gravitationalAcceleration = FVector(0, 0, -10000.0f);
	UPROPERTY(EditAnywhere, Category = "Grapple Options")
		FVector position;
	UPROPERTY(EditAnywhere, Category = "Grapple Options")
		FVector previousPosition;

	int collisionsResolved = 0;
	bool cornerFlag = false;

protected:	
	FVector velocity;
};
