// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "CombatSoldierAIController.generated.h"

class ACombatCharacter;

/**
 *  A simple AI Controller that drives a CombatCharacter in team battles.
 *  Runs on the server only and uses the same attack and guard inputs as a player, so it tests the networked combat code.
 *  Walks straight at the nearest enemy, so it doesn't need a nav mesh.
 */
UCLASS()
class ACombatSoldierAIController : public AAIController
{
	GENERATED_BODY()

protected:

	/** Distance at which the soldier stops walking and starts swinging */
	UPROPERTY(EditAnywhere, Category="Soldier", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float AttackRange = 150.0f;

	/** Shortest time between attacks */
	UPROPERTY(EditAnywhere, Category="Soldier", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float MinAttackInterval = 0.8f;

	/** Longest time between attacks */
	UPROPERTY(EditAnywhere, Category="Soldier", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float MaxAttackInterval = 1.6f;

	/** Chance to guard when the target starts an attack */
	UPROPERTY(EditAnywhere, Category="Soldier", meta = (ClampMin = 0, ClampMax = 1))
	float GuardChance = 0.4f;

	/** How long a block is held */
	UPROPERTY(EditAnywhere, Category="Soldier", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float BlockHoldTime = 0.8f;

	/** How often the soldier looks for the nearest enemy */
	UPROPERTY(EditAnywhere, Category="Soldier", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float RetargetInterval = 0.5f;

	/** Current enemy */
	TWeakObjectPtr<ACombatCharacter> Target;

	/** Time at which the next attack is allowed */
	float NextAttackTime = 0.0f;

	/** Time at which the next target search runs */
	float NextRetargetTime = 0.0f;

	/** Time at which the current block is released */
	float BlockReleaseTime = 0.0f;

	/** True if the target was attacking on the last tick, so a new swing can be noticed */
	bool bTargetWasAttacking = false;

public:

	/** Constructor */
	ACombatSoldierAIController();

	/** Updates movement, attacks and guarding */
	virtual void Tick(float DeltaTime) override;

protected:

	/** Destroys this controller along with its pawn */
	virtual void OnPossess(APawn* InPawn) override;

	/** Called when the possessed pawn is destroyed */
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedActor);

	/** Returns the closest living character on another team */
	ACombatCharacter* FindNearestEnemy(const ACombatCharacter* Soldier) const;
};
