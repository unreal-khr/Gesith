// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SquadTypes.h"
#include "CombatSoldierAIController.generated.h"

class ACombatCharacter;

/**
 *  A simple AI Controller that drives a CombatCharacter in team battles.
 *  Runs on the server only and uses the same attack and guard inputs as a player, so it tests the networked combat code.
 *  Walks straight at its target, so it doesn't need a nav mesh.
 *  A soldier with a squad leader follows the leader's orders. Without one it always charges.
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

	/** How close an enemy has to get to the leader before following soldiers fight it */
	UPROPERTY(EditAnywhere, Category="Squad", meta = (ClampMin = 0, ClampMax = 5000, Units = "cm"))
	float FollowEngageRange = 800.0f;

	/** How close an enemy has to get to its formation slot before a holding soldier fights it */
	UPROPERTY(EditAnywhere, Category="Squad", meta = (ClampMin = 0, ClampMax = 5000, Units = "cm"))
	float HoldEngageRange = 500.0f;

	/** Distance between soldiers in the formation */
	UPROPERTY(EditAnywhere, Category="Squad", meta = (ClampMin = 50, ClampMax = 1000, Units = "cm"))
	float FormationSpacing = 150.0f;

	/** Soldiers per formation row */
	UPROPERTY(EditAnywhere, Category="Squad", meta = (ClampMin = 1, ClampMax = 50))
	int32 FormationWidth = 5;

	/** Distance between the leader and the first row while following */
	UPROPERTY(EditAnywhere, Category="Squad", meta = (ClampMin = 0, ClampMax = 2000, Units = "cm"))
	float FollowDistance = 300.0f;

	/** Soldiers closer than this to their slot stop walking */
	UPROPERTY(EditAnywhere, Category="Squad", meta = (ClampMin = 0, ClampMax = 500, Units = "cm"))
	float SlotTolerance = 60.0f;

	/** Lord this soldier follows. Null for soldiers without a squad */
	TWeakObjectPtr<ACombatCharacter> SquadLeader;

	/** Current order */
	ESquadOrder SquadOrder = ESquadOrder::Charge;

	/** Place in the formation */
	int32 SquadIndex = 0;

	/** Soldiers in the formation when the order was given */
	int32 SquadCount = 1;

	/** Front center of the formation while holding */
	FVector HoldOrigin = FVector::ZeroVector;

	/** Direction the formation faces while holding */
	FVector HoldForward = FVector::ForwardVector;

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

	/** Puts this soldier under a lord's command. Server only */
	void JoinSquad(ACombatCharacter* Leader);

	/** Gives this soldier an order and its place in the formation. Server only */
	void SetSquadOrder(ESquadOrder NewOrder, int32 NewIndex, int32 NewCount, const FVector& Origin, const FVector& Forward);

	/** Returns the lord this soldier follows, or nullptr */
	ACombatCharacter* GetSquadLeader() const { return SquadLeader.Get(); }

	/** Gives an order to every living soldier following the leader and lines them up. Returns how many got it. Server only */
	static int32 OrderSquad(ACombatCharacter* Leader, ESquadOrder Order);

protected:

	/** Destroys this controller along with its pawn */
	virtual void OnPossess(APawn* InPawn) override;

	/** Called when the possessed pawn is destroyed */
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedActor);

	/** Returns the closest living character on another team that is within MaxRange of Center */
	ACombatCharacter* FindNearestEnemy(const ACombatCharacter* Soldier, const FVector& Center, float MaxRange) const;

	/** Walks toward the target, then attacks and guards */
	void FightTarget(ACombatCharacter* Soldier, float DeltaTime, float Now);

	/** Returns where this soldier stands in a formation whose front center is Origin */
	FVector GetFormationSlot(const FVector& Origin, const FVector& Forward) const;

	/** Walks to the slot and faces the formation's direction once there */
	void MoveToSlot(ACombatCharacter* Soldier, const FVector& Slot, const FVector& Forward, float DeltaTime);
};
