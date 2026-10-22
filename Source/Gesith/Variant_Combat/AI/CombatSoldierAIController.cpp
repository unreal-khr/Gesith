// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatSoldierAIController.h"
#include "CombatCharacter.h"
#include "EngineUtils.h"
#include "Engine/World.h"

ACombatSoldierAIController::ACombatSoldierAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACombatSoldierAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// clean up the controller when the soldier is removed after dying
	InPawn->OnDestroyed.AddDynamic(this, &ACombatSoldierAIController::OnPawnDestroyed);

	// don't all swing on the same frame
	NextAttackTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.0f, MaxAttackInterval);
}

void ACombatSoldierAIController::OnPawnDestroyed(AActor* DestroyedActor)
{
	Destroy();
}

void ACombatSoldierAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ACombatCharacter* Soldier = Cast<ACombatCharacter>(GetPawn());

	if (!Soldier || Soldier->IsDead())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// release a block once it has been held long enough
	if (Soldier->IsBlocking() && Now >= BlockReleaseTime)
	{
		Soldier->DoGuardEnd();
	}

	// staggered soldiers can't do anything
	if (Soldier->IsStaggered())
	{
		return;
	}

	// look for the nearest enemy every so often, or right away if the target died
	if (!Target.IsValid() || Target->IsDead() || Now >= NextRetargetTime)
	{
		Target = FindNearestEnemy(Soldier);
		NextRetargetTime = Now + RetargetInterval;
		bTargetWasAttacking = false;
	}

	if (!Target.IsValid())
	{
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - Soldier->GetActorLocation();
	const FRotator LookRotation(0.0f, ToTarget.Rotation().Yaw, 0.0f);

	// face the target
	SetControlRotation(LookRotation);

	if (ToTarget.Size2D() > AttackRange)
	{
		// walk straight at the target
		Soldier->AddMovementInput(ToTarget.GetSafeNormal2D(), 1.0f);

		return;
	}

	// keep turning toward the target while standing still, so attacks and guards point the right way
	Soldier->SetActorRotation(FMath::RInterpTo(Soldier->GetActorRotation(), LookRotation, DeltaTime, 10.0f));

	// did the target just start a swing?
	const bool bTargetIsAttacking = Target->IsAttacking();

	if (bTargetIsAttacking && !bTargetWasAttacking && !Soldier->IsAttacking() && FMath::FRand() < GuardChance)
	{
		Soldier->DoGuardStart();

		// a block is held for a while, a parry is a single press
		BlockReleaseTime = Now + BlockHoldTime;
		NextAttackTime = FMath::Max(NextAttackTime, BlockReleaseTime);
	}

	bTargetWasAttacking = bTargetIsAttacking;

	// swing when the timer allows it
	if (Now >= NextAttackTime && !Soldier->IsBlocking())
	{
		Soldier->DoComboAttackStart();

		NextAttackTime = Now + FMath::FRandRange(MinAttackInterval, MaxAttackInterval);
	}
}

ACombatCharacter* ACombatSoldierAIController::FindNearestEnemy(const ACombatCharacter* Soldier) const
{
	ACombatCharacter* Nearest = nullptr;
	float NearestDistSquared = TNumericLimits<float>::Max();

	for (TActorIterator<ACombatCharacter> It(GetWorld()); It; ++It)
	{
		ACombatCharacter* Other = *It;

		if (Other == Soldier || Other->IsDead() || !Soldier->IsEnemyOf(Other))
		{
			continue;
		}

		const float DistSquared = FVector::DistSquared(Soldier->GetActorLocation(), Other->GetActorLocation());

		if (DistSquared < NearestDistSquared)
		{
			NearestDistSquared = DistSquared;
			Nearest = Other;
		}
	}

	return Nearest;
}
