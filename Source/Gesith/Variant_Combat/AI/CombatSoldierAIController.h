// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SquadTypes.h"
#include "CombatSoldierAIController.generated.h"

class ACombatCharacter;
class AGesithCastle;
struct FSoldierRecord;

/**
 *  A simple AI Controller that drives a CombatCharacter in team battles.
 *  Runs on the server only and uses the same attack and guard inputs as a player, so it tests the networked combat code.
 *  Walks straight at its target, so it doesn't need a nav mesh.
 *  A soldier with a squad leader follows the leader's orders.
 *  A castle soldier without a leader stands at their post in the castle, or drills at the training ground.
 *  Any other soldier always charges.
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

	/** Farthest distance a soldier shoots from. Farther targets are walked toward */
	UPROPERTY(EditAnywhere, Category="Soldier|Ranged", meta = (ClampMin = 0, ClampMax = 10000, Units = "cm"))
	float BowRange = 2500.0f;

	/** Enemies closer than this make an archer put the bow away and draw a melee weapon */
	UPROPERTY(EditAnywhere, Category="Soldier|Ranged", meta = (ClampMin = 0, ClampMax = 2000, Units = "cm"))
	float BowMinRange = 400.0f;

	/** Aim error as a fraction of the distance, for a fresh recruit. Weapon skill shrinks it */
	UPROPERTY(EditAnywhere, Category="Soldier|Ranged", meta = (ClampMin = 0, ClampMax = 0.5))
	float BowSpread = 0.04f;

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

	/** The follow formation only turns while the leader moves faster than this, so turning on the spot doesn't move the squad */
	UPROPERTY(EditAnywhere, Category="Squad", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float FollowTurnMinSpeed = 150.0f;

	/** How fast the follow formation turns toward the leader's walking direction */
	UPROPERTY(EditAnywhere, Category="Squad", meta = (ClampMin = 0, ClampMax = 1000, Units = "deg/s"))
	float FollowTurnRate = 120.0f;

	/** Shortest time between practice swings at the training ground */
	UPROPERTY(EditAnywhere, Category="Castle", meta = (ClampMin = 0, ClampMax = 30, Units = "s"))
	float MinDrillInterval = 2.0f;

	/** Longest time between practice swings at the training ground */
	UPROPERTY(EditAnywhere, Category="Castle", meta = (ClampMin = 0, ClampMax = 30, Units = "s"))
	float MaxDrillInterval = 4.0f;

	/** How quickly the soldier turns to face where it walks */
	UPROPERTY(EditAnywhere, Category="Soldier", meta = (ClampMin = 0, ClampMax = 50))
	float TurnSpeed = 8.0f;

	/** Walking speed while going about castle chores, as a fraction of full speed */
	UPROPERTY(EditAnywhere, Category="Castle", meta = (ClampMin = 0.1, ClampMax = 1))
	float StrollSpeed = 0.4f;

	/** Shortest time spent on one chore before picking another */
	UPROPERTY(EditAnywhere, Category="Castle", meta = (ClampMin = 0, ClampMax = 300, Units = "s"))
	float MinChoreTime = 10.0f;

	/** Longest time spent on one chore before picking another */
	UPROPERTY(EditAnywhere, Category="Castle", meta = (ClampMin = 0, ClampMax = 300, Units = "s"))
	float MaxChoreTime = 25.0f;

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

	/** Direction the formation faces while following. Follows the leader's walking direction, not where they look */
	FVector FollowForward = FVector::ForwardVector;

	/** Direction the formation faces while holding */
	FVector HoldForward = FVector::ForwardVector;

	/** Castle whose roster this soldier comes from. Null for soldiers spawned for testing */
	TWeakObjectPtr<AGesithCastle> HomeCastle;

	/** Roster id of this soldier in the home castle */
	int32 RosterId = 0;

	/** True while a castle soldier is summoned and follows a lord */
	bool bInSquad = false;

	/** True once the soldier has fought an enemy since being summoned */
	bool bFought = false;

	/** True once the home castle has been told this soldier died */
	bool bReportedDeath = false;

	/** Time of the next practice swing */
	float NextDrillTime = 0.0f;

	/** Where the soldier is spending free time in the castle, and which way they face there */
	FVector ChoreLocation = FVector::ZeroVector;
	FVector ChoreForward = FVector::ForwardVector;

	/** Time at which the soldier picks a new chore. Negative to pick one right away */
	float NextChoreTime = -1.0f;

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

	/** Weapon skill from the roster, used for aim. 10 is a fresh recruit */
	float WeaponSkill = 10.0f;

public:

	/** Constructor */
	ACombatSoldierAIController();

	/** Updates movement, attacks and guarding */
	virtual void Tick(float DeltaTime) override;

	/** Puts this soldier under a lord's command. Server only */
	void JoinSquad(ACombatCharacter* Leader);

	/** Gives this soldier an order and its place in the formation. Server only */
	void SetSquadOrder(ESquadOrder NewOrder, int32 NewIndex, int32 NewCount, const FVector& Origin, const FVector& Forward);

	/** Links this soldier to a roster entry and applies the entry's current stats. Can be called again after training. Server only */
	void SetRosterSoldier(AGesithCastle* Castle, const FSoldierRecord& Record);

	/** Leaves the squad and goes back to their post in the castle. Server only */
	void ReturnToPost();

	/** Returns the castle this soldier belongs to, or nullptr */
	AGesithCastle* GetHomeCastle() const;

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

	/** Tells the home castle the soldier died, once */
	void ReportDeath();

	/** Returns the closest living character on another team that is within MaxRange of Center */
	ACombatCharacter* FindNearestEnemy(const ACombatCharacter* Soldier, const FVector& Center, float MaxRange) const;

	/** Walks toward the target, then attacks and guards */
	void FightTarget(ACombatCharacter* Soldier, float DeltaTime, float Now);

	/** Stands and shoots at the target, walking closer if it's out of range */
	void ShootTarget(ACombatCharacter* Soldier, float DeltaTime, float Now);

	/** Takes out a ranged or melee weapon if one is carried. Returns true once the right kind is in hand */
	bool UseWeapon(ACombatCharacter* Soldier, bool bRanged) const;

	/** Returns true if the soldier's squad leader has ordered bows out */
	bool IsRangedOrder() const;

	/** Returns where this soldier stands in a formation whose front center is Origin */
	FVector GetFormationSlot(const FVector& Origin, const FVector& Forward) const;

	/** Walks to the slot and faces the formation's direction once there. Returns true once at the slot */
	bool MoveToSlot(ACombatCharacter* Soldier, const FVector& Slot, const FVector& Forward, float DeltaTime, float SpeedScale = 1.0f);
};
