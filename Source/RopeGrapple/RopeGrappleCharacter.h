// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "GrappleGun.h"
#include "RopeGrappleCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class USceneComponent;
class UCameraComponent;
class UAnimMontage;
class USoundBase;
class USphereComponent;

UCLASS(config=Game)
class ARopeGrappleCharacter : public ACharacter
{
	GENERATED_BODY()
	
public:
	ARopeGrappleCharacter();
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = Gameplay)
		void Respawn();
	UFUNCTION(BlueprintCallable, Category = Gameplay)
		void AnchorMovement(FVector anchorPoint);
	UFUNCTION(BlueprintCallable, Category = Gameplay)
		void ReleaseAnchor() { anchored = false; };

	void EndHanging();
	void BeginHanging();
	void RotateGun(FRotator rot);

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		class UInputAction* lookAction;
	/** Fire Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		class UInputAction* fireAction;
	/** Release Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		class UInputAction* releaseAction;
	/** Pull Rope In Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		class UInputAction* pullInAction;
	/** Let Rope Out Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		class UInputAction* letOutAction;

	USkeletalMeshComponent* GetMesh1P() const { return mesh1P; }
	UCameraComponent* GetFirstPersonCameraComponent() const { return firstPersonCameraComponent; }

	FVector CalculatePenetrationCorrection();


protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void ToggleSprint();
	void SetFOV();

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
		USkeletalMeshComponent* mesh1P;
	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
		UCameraComponent* firstPersonCameraComponent;
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		class UInputMappingContext* defaultMappingContext;
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		class UInputMappingContext* fireMappingContext;
	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		class UInputAction* jumpAction;
	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		class UInputAction* moveAction;
	/** Sprint Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		class UInputAction* sprintAction;
	/** Sprint End Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		class UInputAction* sprintEndAction;
	/** Sprint Speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		float sprintSpeed = 900.0f;
	/** Sprint FOV */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		float sprintFOV = 140.0f;
	/** FOV Change Time */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
		float FOVMaxChangePerTick = 1.0f;
	/** Grapple Gun **/
	UPROPERTY(EditAnywhere, Category = "Grapple", meta = (AllowPrivateAccess = "true"))
		UGrappleGun* grappleGun1;
	UPROPERTY(EditAnywhere, Category = "Grapple", meta = (AllowPrivateAccess = "true"))
		USphereComponent* grappleGunPivot;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple", meta = (AllowPrivateAccess = "true"))
		bool hanging = false;

	float defaultWalkSpeed;
	float defaultFOV;
	float lastFOV;
	bool sprinting = false;

	float allowedError = 0.01f;
	FVector respawnLocation;

	bool anchored = false;
	FVector anchorPoint;
};

