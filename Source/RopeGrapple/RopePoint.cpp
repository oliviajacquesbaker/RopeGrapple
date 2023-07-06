// Fill out your copyright notice in the Description page of Project Settings.


#include "RopePoint.h"

URopePoint::URopePoint()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URopePoint::UpdatePoint(float deltaTime)
{
	if (!isAnchor) ApplyForce(gravitationalAcceleration * mass, deltaTime);
}

void URopePoint::DrawPoint()
{
	DrawDebugSphere(GetWorld(), position, radius, 16, FColor::Blue, false, 0);
}

void URopePoint::ApplyForce(FVector force, float deltaTime)
{
	velocity = position - previousPosition;
	previousPosition = position;
	position += velocity + (force / mass) * (deltaTime * deltaTime);
}


