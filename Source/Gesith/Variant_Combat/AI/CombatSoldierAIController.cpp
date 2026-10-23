// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatSoldierAIController.h"
#include "CombatCharacter.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GesithCastle.h"

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

void ACombatSoldierAIController::SetRosterSoldier(AGesithCastle* Castle, const FSoldierRecord& Record)
{
	HomeCastle = Castle;
	RosterId = Record.Id;

	if (ACombatCharacter* Soldier = Cast<ACombatCharacter>(GetPawn()))
	{
		Soldier->ApplySoldierStats(Record.Health, Record.Strength);
	}

	// start from the class defaults so applying stats again after training doesn't stack
	const ACombatSoldierAIController* Defaults = GetClass()->GetDefaultObject<ACombatSoldierAIController>();

	// faster soldiers swing more often, up to twice as often at the stat cap
	const float SpeedFactor = FMath::Clamp(1.0f - (Record.AttackSpeed - 10.0f) / 180.0f, 0.5f, 1.0f);
	MinAttackInterval = Defaults->MinAttackInterval * SpeedFactor;
	MaxAttackInterval = Defaults->MaxAttackInterval * SpeedFactor;

	// skilled soldiers read attacks better
	GuardChance = FMath::Clamp(0.3f + Record.WeaponSkill * 0.005f, 0.0f, 0.8f);
}

AGesithCastle* ACombatSoldierAIController::GetHomeCastle() const
{
	return HomeCastle.Get();
}

void ACombatSoldierAIController::ReturnToPost()
{
	AGesithCastle* Castle = HomeCastle.Get();

	if (!bInSquad || !Castle)
	{
		return;
	}

	bInSquad = false;
	SquadLeader = nullptr;
	SquadOrder = ESquadOrder::Charge;
	Target = nullptr;

	Castle->SoldierReturned(RosterId, bFought);

	bFought = false;
}

void ACombatSoldierAIController::ReportDeath()
{
	AGesithCastle* Castle = HomeCastle.Get();

	if (bReportedDeath || !Castle)
	{
		return;
	}

	bReportedDeath = true;

	const FSoldierRecord* Record = Castle->Roster.FindByPredicate([this](const FSoldierRecord& Soldier) { return Soldier.Id == RosterId; });

#if !UE_BUILD_SHIPPING
	if (GEngine && Record)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange, FString::Printf(TEXT("%s was killed"), *Record->Name));
	}
#endif

	Castle->SoldierFell(RosterId);
}

void ACombatSoldierAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ACombatCharacter* Soldier = Cast<ACombatCharacter>(GetPawn());

	if (!Soldier)
	{
		return;
	}

	if (Soldier->IsDead())
	{
		ReportDeath();

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

	ACombatCharacter* Leader = SquadLeader.Get();

	// castle soldiers whose lord is gone walk home on their own
	if (bInSquad && !Leader)
	{
		ReturnToPost();
	}

	AGesithCastle* Castle = HomeCastle.Get();
	const bool bAtCastle = Castle && !bInSquad;

	// soldiers whose leader is gone just charge
	ESquadOrder Order = SquadOrder;

	if (Order == ESquadOrder::Follow && (!Leader || Leader->IsDead()))
	{
		Order = ESquadOrder::Charge;
	}

	// work out where the soldier belongs and how far from there it will fight
	FVector Slot = Soldier->GetActorLocation();
	FVector SlotForward = Soldier->GetActorForwardVector();
	FVector EngageCenter = Soldier->GetActorLocation();
	float EngageRange = TNumericLimits<float>::Max();

	if (Order == ESquadOrder::Follow)
	{
		// turn the formation only while the leader walks, so looking around doesn't send soldiers running
		const FVector LeaderVelocity = Leader->GetVelocity();

		if (LeaderVelocity.SizeSquared2D() > FMath::Square(FollowTurnMinSpeed))
		{
			FollowForward = FMath::VInterpNormalRotationTo(FollowForward, LeaderVelocity.GetSafeNormal2D(), DeltaTime, FollowTurnRate);
		}

		SlotForward = FollowForward;
		Slot = GetFormationSlot(Leader->GetActorLocation() - SlotForward * FollowDistance, SlotForward);
		EngageCenter = Leader->GetActorLocation();
		EngageRange = FollowEngageRange;
	}
	else if (bAtCastle)
	{
		// castle soldiers defend their post
		bool bTraining = false;
		Castle->GetSoldierPost(RosterId, Slot, SlotForward, bTraining);
		EngageCenter = Slot;
		EngageRange = HoldEngageRange;
	}
	else if (Order == ESquadOrder::Hold)
	{
		SlotForward = HoldForward;
		Slot = GetFormationSlot(HoldOrigin, HoldForward);
		EngageCenter = Slot;
		EngageRange = HoldEngageRange;
	}

	// look for the nearest enemy every so often, or right away if the target died
	if (!Target.IsValid() || Target->IsDead() || Now >= NextRetargetTime)
	{
		Target = FindNearestEnemy(Soldier, EngageCenter, EngageRange);
		NextRetargetTime = Now + RetargetInterval;
		bTargetWasAttacking = false;
	}

	if (Target.IsValid())
	{
		bFought = true;

		FightTarget(Soldier, DeltaTime, Now);
	}
	else if (bAtCastle)
	{
		const bool bAtPost = MoveToSlot(Soldier, Slot, SlotForward, DeltaTime);

		// soldiers at the training ground practice their swings
		const FSoldierRecord* Record = Castle->Roster.FindByPredicate([this](const FSoldierRecord& Other) { return Other.Id == RosterId; });

		if (bAtPost && Record && Record->bTraining && Now >= NextDrillTime)
		{
			Soldier->DoComboAttackStart();

			NextDrillTime = Now + FMath::FRandRange(MinDrillInterval, MaxDrillInterval);
		}
	}
	else if (Order != ESquadOrder::Charge)
	{
		MoveToSlot(Soldier, Slot, SlotForward, DeltaTime);
	}
}

void ACombatSoldierAIController::FightTarget(ACombatCharacter* Soldier, float DeltaTime, float Now)
{
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

bool ACombatSoldierAIController::MoveToSlot(ACombatCharacter* Soldier, const FVector& Slot, const FVector& Forward, float DeltaTime)
{
	const FVector ToSlot = Slot - Soldier->GetActorLocation();
	const float Distance = ToSlot.Size2D();

	if (Distance > SlotTolerance)
	{
		SetControlRotation(FRotator(0.0f, ToSlot.Rotation().Yaw, 0.0f));

		// slow down near the slot so soldiers don't overshoot and wobble
		Soldier->AddMovementInput(ToSlot.GetSafeNormal2D(), FMath::Clamp(Distance / 300.0f, 0.3f, 1.0f));

		return false;
	}

	// stand in line facing the same way as the formation
	const FRotator FormationRotation(0.0f, Forward.Rotation().Yaw, 0.0f);

	SetControlRotation(FormationRotation);
	Soldier->SetActorRotation(FMath::RInterpTo(Soldier->GetActorRotation(), FormationRotation, DeltaTime, 5.0f));

	return true;
}

void ACombatSoldierAIController::JoinSquad(ACombatCharacter* Leader)
{
	SquadLeader = Leader;

	// castle soldiers stay on the roster while they're away
	bInSquad = HomeCastle.IsValid();
	bFought = false;
}

void ACombatSoldierAIController::SetSquadOrder(ESquadOrder NewOrder, int32 NewIndex, int32 NewCount, const FVector& Origin, const FVector& Forward)
{
	SquadOrder = NewOrder;
	SquadIndex = NewIndex;
	SquadCount = FMath::Max(NewCount, 1);
	HoldOrigin = Origin;
	HoldForward = Forward.GetSafeNormal2D();
	FollowForward = HoldForward;

	// pick a target that fits the new order right away
	Target = nullptr;
	NextRetargetTime = 0.0f;
}

FVector ACombatSoldierAIController::GetFormationSlot(const FVector& Origin, const FVector& Forward) const
{
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);

	const int32 Row = SquadIndex / FormationWidth;
	const int32 Column = SquadIndex % FormationWidth;

	// the last row may be shorter, so center each row on its own
	const int32 InRow = FMath::Min(FormationWidth, SquadCount - Row * FormationWidth);
	const float SideOffset = (Column - (InRow - 1) * 0.5f) * FormationSpacing;

	return Origin - Forward * (Row * FormationSpacing) + Right * SideOffset;
}

int32 ACombatSoldierAIController::OrderSquad(ACombatCharacter* Leader, ESquadOrder Order)
{
	if (!Leader || !Leader->GetWorld())
	{
		return 0;
	}

	TArray<ACombatSoldierAIController*> Squad;

	for (TActorIterator<ACombatSoldierAIController> It(Leader->GetWorld()); It; ++It)
	{
		const ACombatCharacter* Soldier = Cast<ACombatCharacter>(It->GetPawn());

		if (It->GetSquadLeader() == Leader && Soldier && !Soldier->IsDead())
		{
			Squad.Add(*It);
		}
	}

	// holding soldiers line up in front of where the leader stands
	const FVector Forward = Leader->GetActorForwardVector().GetSafeNormal2D();
	const FVector Origin = Leader->GetActorLocation() + Forward * 200.0f;

	for (int32 Index = 0; Index < Squad.Num(); ++Index)
	{
		Squad[Index]->SetSquadOrder(Order, Index, Squad.Num(), Origin, Forward);
	}

	return Squad.Num();
}

ACombatCharacter* ACombatSoldierAIController::FindNearestEnemy(const ACombatCharacter* Soldier, const FVector& Center, float MaxRange) const
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

		if (MaxRange < TNumericLimits<float>::Max() && FVector::DistSquared2D(Center, Other->GetActorLocation()) > FMath::Square(MaxRange))
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
