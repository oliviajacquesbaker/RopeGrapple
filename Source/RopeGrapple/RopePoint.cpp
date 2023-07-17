// Fill out your copyright notice in the Description page of Project Settings.


#include "RopePoint.h"

URopePoint::URopePoint()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URopePoint::UpdatePoint(float deltaTime)
{
	if (!isAnchor) ApplyForce(gravitationalAcceleration * mass, deltaTime);
	cornerFlag = false;
}

void URopePoint::ApplyForce(FVector force, float deltaTime)
{
	velocity = position - previousPosition;
	if (collisionsResolved > 0 && velocity.Length() > 3) {
		velocity = velocity.GetSafeNormal() * 3; 
		--collisionsResolved;
	}
	previousPosition = position;
	position += velocity + (force / mass) * (deltaTime * deltaTime);
}


