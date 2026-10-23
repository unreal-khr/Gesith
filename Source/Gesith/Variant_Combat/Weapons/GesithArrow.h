// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GesithArrow.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 *  An arrow shot from a bow.
 *  Spawned and judged on the server, replicated so everyone sees it fly.
 *  The character that shot it is the instigator. Arrows fly through allies, so archers can shoot over their own lines.
 */
UCLASS()
class AGesithArrow : public AActor
{
	GENERATED_BODY()

	/** Collision sphere at the arrow tip */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> Collision;

	/** Placeholder shaft */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Shaft;

	/** Moves the arrow and lets it drop over distance */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UProjectileMovementComponent> Movement;

protected:

	/** Damage dealt on hit, set by the shooter */
	float Damage = 1.0f;

	/** Knockback applied on hit */
	UPROPERTY(EditAnywhere, Category="Arrow", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float KnockbackImpulse = 150.0f;

	/** How long a stuck arrow stays around */
	UPROPERTY(EditAnywhere, Category="Arrow", meta = (ClampMin = 0, ClampMax = 60, Units = "s"))
	float StuckLifeSpan = 5.0f;

	/** True once the arrow hit something, so it only deals damage once */
	bool bHasHit = false;

public:

	/** Constructor */
	AGesithArrow();

	/** Sets the damage and launch speed. Call right after spawning, on the server */
	void Launch(float NewDamage, const FVector& Velocity);

protected:

	/** Keeps the arrow from hitting its archer on every machine */
	virtual void BeginPlay() override;

	/** Gives clients the arrow's speed so they can fly it smoothly */
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;

	/** Walls, the ground and other world objects stop the arrow. Server only */
	UFUNCTION()
	void OnArrowHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Characters are overlapped instead, so the arrow can pass allies and the dead. Server only */
	UFUNCTION()
	void OnArrowOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Deals damage and sticks into whatever was hit */
	void StrikeTarget(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& ImpactPoint);
};
