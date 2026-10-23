// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "CombatLifeBar.h"
#include "Engine/DamageEvents.h"
#include "TimerManager.h"
#include "Engine/LocalPlayer.h"
#include "CombatPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "CombatSoldierAIController.h"
#include "GesithCastle.h"

ACombatCharacter::ACombatCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// bind the attack montage ended delegate
	OnAttackMontageEnded.BindUObject(this, &ACombatCharacter::AttackMontageEnded);

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(35.0f, 90.0f);

	// Configure character movement
	GetCharacterMovement()->MaxWalkSpeed = 400.0f;

	// create the camera boom
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);

	CameraBoom->TargetArmLength = DefaultCameraDistance;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->bEnableCameraRotationLag = true;

	// create the orbiting camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// create the life bar widget component
	LifeBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("LifeBar"));
	LifeBar->SetupAttachment(RootComponent);

	// set the player tag
	Tags.Add(FName("Player"));

	// blue and red life bars for the two teams
	TeamColors.Add(FLinearColor(0.1f, 0.4f, 1.0f));
	TeamColors.Add(FLinearColor(1.0f, 0.15f, 0.1f));
}

void ACombatCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ACombatCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ACombatCharacter::ComboAttackPressed()
{
	// route the input
	DoComboAttackStart();
}

void ACombatCharacter::GuardPressed()
{
	// route the input
	DoGuardStart();
}

void ACombatCharacter::GuardReleased()
{
	// route the input
	DoGuardEnd();
}

void ACombatCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ACombatCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ACombatCharacter::DoComboAttackStart()
{
	// clients tell the server, which runs the same logic and decides what hits
	if (!HasAuthority())
	{
		ServerComboAttackInput();
	}

	// run the attack locally right away so the input feels responsive
	HandleComboAttackInput();
}

void ACombatCharacter::DoComboAttackEnd()
{
	// stub
}

void ACombatCharacter::DoGuardStart()
{
	if (bHasShield)
	{
		// raise the shield locally, then tell the server
		SetBlocking(true);

		if (!HasAuthority())
		{
			ServerSetBlocking(true);
		}
	}
	else
	{
		// parries are only judged on the server
		if (HasAuthority())
		{
			TryParry();
		}
		else
		{
			ServerParry();
		}
	}
}

void ACombatCharacter::DoGuardEnd()
{
	if (bHasShield)
	{
		// lower the shield locally, then tell the server
		SetBlocking(false);

		if (!HasAuthority())
		{
			ServerSetBlocking(false);
		}
	}
}

bool ACombatCharacter::IsStaggered() const
{
	return GetWorld()->GetTimeSeconds() < StaggerEndTime;
}

bool ACombatCharacter::IsEnemyOf(const ACombatCharacter* Other) const
{
	return Other && TeamId != 0 && Other->TeamId != 0 && TeamId != Other->TeamId;
}

bool ACombatCharacter::IsAllyOf(const ACombatCharacter* Other) const
{
	return Other && TeamId != 0 && TeamId == Other->TeamId;
}

void ACombatCharacter::SetTeam(int32 NewTeamId)
{
	if (HasAuthority())
	{
		TeamId = NewTeamId;

		OnRep_TeamId();
	}
}

void ACombatCharacter::OnRep_TeamId()
{
	if (LifeBarWidget)
	{
		LifeBarWidget->SetBarColor(TeamColors.IsValidIndex(TeamId - 1) ? TeamColors[TeamId - 1] : LifeBarColor);
	}
}

void ACombatCharacter::JoinTeam(int32 NewTeamId)
{
	if (HasAuthority())
	{
		ServerJoinTeam_Implementation(NewTeamId);
	}
	else
	{
		ServerJoinTeam(NewTeamId);
	}
}

void ACombatCharacter::ServerJoinTeam_Implementation(int32 NewTeamId)
{
#if !UE_BUILD_SHIPPING
	SetTeam(FMath::Clamp(NewTeamId, 0, TeamColors.Num()));

	ShowCombatDebugMessage(FString::Printf(TEXT("Joined team %d"), TeamId), FColor::Green);
#endif
}

void ACombatCharacter::SpawnBattle(int32 SoldiersPerTeam)
{
	if (HasAuthority())
	{
		ServerSpawnBattle_Implementation(SoldiersPerTeam);
	}
	else
	{
		ServerSpawnBattle(SoldiersPerTeam);
	}
}

void ACombatCharacter::ServerSpawnBattle_Implementation(int32 SoldiersPerTeam)
{
#if !UE_BUILD_SHIPPING
	// the field cap is 200 units, so 100 per team at most
	SoldiersPerTeam = FMath::Clamp(SoldiersPerTeam, 1, 100);

	// the two lines face each other, centered in front of the player
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const FVector Center = GetActorLocation() + Forward * (BattleLineGap * 0.5f + 300.0f);

	for (int32 Team = 1; Team <= 2; ++Team)
	{
		// team 1 is the near line, team 2 the far one
		const FVector LineCenter = Center + Forward * (Team == 1 ? -0.5f : 0.5f) * BattleLineGap;
		const FRotator Facing = (Team == 1 ? Forward : -Forward).Rotation();

		for (int32 Index = 0; Index < SoldiersPerTeam; ++Index)
		{
			const float Offset = (Index - (SoldiersPerTeam - 1) * 0.5f) * BattleLineSpacing;
			const FVector Location = LineCenter + Right * Offset;

			SpawnSoldier(Location, Facing, Team);
		}
	}

	ShowBattleStatus();
#endif
}

void ACombatCharacter::ClearBattle()
{
	if (HasAuthority())
	{
		ServerClearBattle_Implementation();
	}
	else
	{
		ServerClearBattle();
	}
}

void ACombatCharacter::ServerClearBattle_Implementation()
{
#if !UE_BUILD_SHIPPING
	for (TActorIterator<ACombatCharacter> It(GetWorld()); It; ++It)
	{
		const ACombatSoldierAIController* SoldierAI = Cast<ACombatSoldierAIController>(It->GetController());

		// castle soldiers belong to the roster, not the test battle
		if (SoldierAI && SoldierAI->GetHomeCastle())
		{
			continue;
		}

		if (SoldierAI || (!It->IsPlayerControlled() && It->TeamId != 0))
		{
			It->Destroy();
		}
	}
#endif
}

void ACombatCharacter::SpawnSquad(int32 SquadSize)
{
	if (HasAuthority())
	{
		ServerSpawnSquad_Implementation(SquadSize);
	}
	else
	{
		ServerSpawnSquad(SquadSize);
	}
}

void ACombatCharacter::ServerSpawnSquad_Implementation(int32 SquadSize)
{
#if !UE_BUILD_SHIPPING
	SquadSize = FMath::Clamp(SquadSize, 1, 50);

	SpawnSquadMembers(SquadSize);

	const int32 Count = ACombatSoldierAIController::OrderSquad(this, ESquadOrder::Follow);

	ShowCombatDebugMessage(FString::Printf(TEXT("Squad of %d is following you. F1 Follow, F2 Hold, F3 Charge"), Count), FColor::Green);
#endif
}

void ACombatCharacter::MusterFromCastle(AGesithCastle* Castle)
{
	if (!HasAuthority() || !Castle)
	{
		return;
	}

	// the lord fights on the castle's side
	if (TeamId != Castle->TeamId)
	{
		SetTeam(Castle->TeamId);
	}

	const TArray<FSoldierRecord> Records = Castle->MusterSoldiers();

	for (const FSoldierRecord& Record : Records)
	{
		ACombatCharacter* Soldier = Castle->GetSoldierPawn(Record.Id);
		ACombatSoldierAIController* SoldierAI = Soldier ? Cast<ACombatSoldierAIController>(Soldier->GetController()) : nullptr;

		// soldiers without a character right now stay home
		if (!SoldierAI || Soldier->IsDead())
		{
			Castle->SoldierReturned(Record.Id, false);
			continue;
		}

		// stats may have grown at the training ground since the character was made
		SoldierAI->SetRosterSoldier(Castle, Record);
		SoldierAI->JoinSquad(this);
	}

	const int32 Count = ACombatSoldierAIController::OrderSquad(this, ESquadOrder::Follow);

	ShowCombatDebugMessage(FString::Printf(TEXT("%d soldiers summoned. F1 Follow, F2 Hold, F3 Charge"), Count), FColor::Green);
}

void ACombatCharacter::DismissToCastle(AGesithCastle* Castle)
{
	if (!HasAuthority() || !Castle)
	{
		return;
	}

	int32 Count = 0;

	for (TActorIterator<ACombatSoldierAIController> It(GetWorld()); It; ++It)
	{
		if (It->GetSquadLeader() == this && It->GetHomeCastle() == Castle)
		{
			It->ReturnToPost();
			++Count;
		}
	}

	ShowCombatDebugMessage(FString::Printf(TEXT("%d soldiers dismissed and went back to their posts"), Count), FColor::Green);
}

void ACombatCharacter::ApplySoldierStats(float Health, float Strength)
{
	if (!HasAuthority())
	{
		return;
	}

	// start from the class defaults so applying stats again after training doesn't stack
	const ACombatCharacter* Defaults = GetClass()->GetDefaultObject<ACombatCharacter>();

	// a fresh recruit (10) fights like the base character, the stat cap (100) is much tougher
	MaxHP = Defaults->MaxHP * (0.5f + Health / 20.0f);
	MeleeDamage = Defaults->MeleeDamage * (0.5f + Strength / 20.0f);

	ResetHP();
}

TArray<ACombatCharacter*> ACombatCharacter::SpawnSquadMembers(int32 SquadSize)
{
	TArray<ACombatCharacter*> Soldiers;

	// a squad needs a side to fight for
	if (TeamId == 0)
	{
		SetTeam(1);
	}

	// spawn behind the leader, the follow order lines them up afterwards
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);

	for (int32 Index = 0; Index < SquadSize; ++Index)
	{
		const FVector Location = GetActorLocation() - Forward * (300.0f + (Index / 5) * BattleLineSpacing) + Right * ((Index % 5 - 2) * BattleLineSpacing);

		ACombatCharacter* Soldier = SpawnSoldier(Location, Forward.Rotation(), TeamId);
		ACombatSoldierAIController* SoldierAI = Soldier ? Cast<ACombatSoldierAIController>(Soldier->GetController()) : nullptr;

		if (SoldierAI)
		{
			SoldierAI->JoinSquad(this);
			Soldiers.Add(Soldier);
		}
		else if (Soldier)
		{
			Soldier->Destroy();
		}
	}

	return Soldiers;
}

void ACombatCharacter::SpawnEnemies(int32 EnemyCount)
{
	if (HasAuthority())
	{
		ServerSpawnEnemies_Implementation(EnemyCount);
	}
	else
	{
		ServerSpawnEnemies(EnemyCount);
	}
}

void ACombatCharacter::ServerSpawnEnemies_Implementation(int32 EnemyCount)
{
#if !UE_BUILD_SHIPPING
	EnemyCount = FMath::Clamp(EnemyCount, 1, 100);

	if (TeamId == 0)
	{
		SetTeam(1);
	}

	const int32 EnemyTeam = TeamId == 1 ? 2 : 1;

	// a line far enough away to give the squad time to form up
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const FVector LineCenter = GetActorLocation() + Forward * 2000.0f;

	for (int32 Index = 0; Index < EnemyCount; ++Index)
	{
		const float Offset = (Index - (EnemyCount - 1) * 0.5f) * BattleLineSpacing;

		SpawnSoldier(LineCenter + Right * Offset, (-Forward).Rotation(), EnemyTeam);
	}

	ShowBattleStatus();
#endif
}

ACombatCharacter* ACombatCharacter::SpawnSoldier(const FVector& Location, const FRotator& Facing, int32 Team)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// soldiers use the same Blueprint as the player, so they get the same mesh, animations and combat code
	ACombatCharacter* Soldier = GetWorld()->SpawnActor<ACombatCharacter>(GetClass(), Location, Facing, SpawnParams);

	if (!Soldier)
	{
		return nullptr;
	}

	if (ACombatSoldierAIController* SoldierAI = GetWorld()->SpawnActor<ACombatSoldierAIController>())
	{
		SoldierAI->Possess(Soldier);
	}

	Soldier->SetTeam(Team);

	return Soldier;
}

void ACombatCharacter::ShowBattleStatus() const
{
#if !UE_BUILD_SHIPPING
	if (!GEngine || !HasAuthority())
	{
		return;
	}

	int32 Alive[3] = { 0, 0, 0 };

	for (TActorIterator<ACombatCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && It->TeamId >= 1 && It->TeamId <= 2)
		{
			++Alive[It->TeamId];
		}
	}

	// a fixed key replaces the previous status line instead of stacking
	GEngine->AddOnScreenDebugMessage(0x6E5174, 10.0f, FColor::Green, FString::Printf(TEXT("Battle: team 1 has %d alive, team 2 has %d alive"), Alive[1], Alive[2]));
#endif
}

void ACombatCharacter::AutoGuard()
{
	if (HasAuthority())
	{
		ServerAutoGuard_Implementation();
	}
	else
	{
		ServerAutoGuard();
	}
}

void ACombatCharacter::ServerAutoGuard_Implementation()
{
#if !UE_BUILD_SHIPPING
	bDebugAutoGuard = !bDebugAutoGuard;

	ShowCombatDebugMessage(bDebugAutoGuard ? TEXT("Auto guard on") : TEXT("Auto guard off"), FColor::Green);
#endif
}

void ACombatCharacter::Stagger(float Duration)
{
	if (HasAuthority())
	{
		MulticastStagger(Duration);
	}
}

void ACombatCharacter::ResetHP()
{
	// reset the current HP total
	CurrentHP = MaxHP;

	// update the life bar
	UpdateLifeBar();
}

void ACombatCharacter::UpdateLifeBar()
{
	// the HP may replicate before the widget exists
	if (LifeBarWidget)
	{
		LifeBarWidget->SetLifePercentage(FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f));
	}
}

void ACombatCharacter::HandleComboAttackInput()
{
	// ignore attack inputs while dead, staggered or holding up a shield
	if (IsDead() || IsStaggered() || bIsBlocking)
	{
		return;
	}

	// are we already playing an attack animation?
	if (bIsAttacking)
	{
		// cache the input time so we can check it later
		CachedAttackInputTime = GetWorld()->GetTimeSeconds();

		return;
	}

	// perform a combo attack
	ComboAttack();
}

void ACombatCharacter::ComboAttack()
{
	// raise the attacking flag
	bIsAttacking = true;

	// reset the combo count
	ComboCount = 0;

	// play the attack montage
	PlayComboAttackMontage();

	// let the other machines see the attack
	if (HasAuthority())
	{
		MulticastPlayComboAttack();
	}
}

void ACombatCharacter::PlayComboAttackMontage()
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		const float MontageLength = AnimInstance->Montage_Play(ComboAttackMontage, AttackSpeed, EMontagePlayReturnType::MontageLength, 0.0f, true);

		// subscribe to montage completed and interrupted events
		if (MontageLength > 0.0f)
		{
			// set the end delegate for the montage
			AnimInstance->Montage_SetEndDelegate(OnAttackMontageEnded, ComboAttackMontage);
		}
	}
}

void ACombatCharacter::AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// reset the attacking flag
	bIsAttacking = false;

	// check if we have a non-stale cached input
	if (GetWorld()->GetTimeSeconds() - CachedAttackInputTime <= AttackInputCacheTimeTolerance)
	{
		// do another attack
		HandleComboAttackInput();
	}
}

void ACombatCharacter::SetBlocking(bool bNewBlocking)
{
	// can't raise the shield while dead, staggered or attacking
	if (bNewBlocking && (IsDead() || IsStaggered() || bIsAttacking))
	{
		return;
	}

	if (bIsBlocking != bNewBlocking)
	{
		ShowCombatDebugMessage(bNewBlocking ? TEXT("Shield up") : TEXT("Shield down"), FColor::White);
	}

	bIsBlocking = bNewBlocking;

	UpdateWalkSpeed();
}

void ACombatCharacter::UpdateWalkSpeed()
{
	// the walk speed isn't known until BeginPlay
	if (DefaultWalkSpeed <= 0.0f)
	{
		return;
	}

	// slow down while the shield is up
	GetCharacterMovement()->MaxWalkSpeed = bIsBlocking ? DefaultWalkSpeed * BlockWalkSpeedMultiplier : DefaultWalkSpeed;
}

void ACombatCharacter::TryParry()
{
	if (IsDead() || IsStaggered())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// is the parry still on cooldown?
	if (Now < NextParryTime)
	{
		return;
	}

	ShowCombatDebugMessage(TEXT("Parry attempt"), FColor::White);

	// open the parry window
	ParryWindowEndTime = Now + ParryWindow;
	NextParryTime = Now + ParryCooldown;
}

bool ACombatCharacter::IsFacingAttacker(const AActor* Attacker) const
{
	if (!Attacker)
	{
		return false;
	}

	// compare our facing with the flat direction to the attacker
	const FVector ToAttacker = (Attacker->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();

	return FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToAttacker) >= FMath::Cos(FMath::DegreesToRadians(GuardAngle));
}

void ACombatCharacter::OnRep_CurrentHP()
{
	UpdateLifeBar();
}

void ACombatCharacter::OnRep_IsBlocking()
{
	// the server has the final say, so the owner gets corrected here too
	UpdateWalkSpeed();
}

void ACombatCharacter::ServerComboAttackInput_Implementation()
{
	HandleComboAttackInput();
}

void ACombatCharacter::ServerSetBlocking_Implementation(bool bNewBlocking)
{
	SetBlocking(bNewBlocking);
}

void ACombatCharacter::ServerParry_Implementation()
{
	TryParry();
}

void ACombatCharacter::MulticastPlayComboAttack_Implementation()
{
	// the server and the owner already started the attack themselves
	if (HasAuthority() || IsLocallyControlled())
	{
		return;
	}

	bIsAttacking = true;
	ComboCount = 0;

	PlayComboAttackMontage();
}

void ACombatCharacter::MulticastJumpToComboSection_Implementation(int32 NewComboCount)
{
	// the server and the owner run their own combo checks
	if (HasAuthority() || IsLocallyControlled())
	{
		return;
	}

	ComboCount = NewComboCount;

	if (ComboSectionNames.IsValidIndex(ComboCount))
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_JumpToSection(ComboSectionNames[ComboCount], ComboAttackMontage);
		}
	}
}

void ACombatCharacter::ShowCombatDebugMessage(const FString& Message, const FColor& Color) const
{
#if !UE_BUILD_SHIPPING
	// only the server prints, so each event shows up once. AI soldiers would flood the screen, so only players print
	if (GEngine && HasAuthority() && IsPlayerControlled())
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, Color, FString::Printf(TEXT("%s: %s"), *GetName(), *Message));
	}
#endif
}

void ACombatCharacter::MulticastDealtDamage_Implementation(float Damage, const FVector_NetQuantize& ImpactPoint)
{
	// call the BP handler to play effects, etc.
	DealtDamage(Damage, ImpactPoint);
}

void ACombatCharacter::MulticastReceivedDamage_Implementation(float Damage, const FVector_NetQuantize& ImpactPoint, const FVector_NetQuantizeNormal& DamageDirection)
{
	// enable partial ragdoll physics, but keep the pelvis vertical
	if (!IsDead())
	{
		GetMesh()->SetPhysicsBlendWeight(0.5f);
		GetMesh()->SetBodySimulatePhysics(PelvisBoneName, false);
	}

	ShowCombatDebugMessage(FString::Printf(TEXT("Hit for %.1f, HP %.1f / %.1f"), Damage, CurrentHP, MaxHP), FColor::Red);

	// pass control to BP to play effects, etc.
	ReceivedDamage(Damage, ImpactPoint, DamageDirection);
}

void ACombatCharacter::MulticastGuardedAttack_Implementation(bool bWasParried, const FVector_NetQuantize& ImpactPoint)
{
	ShowCombatDebugMessage(bWasParried ? TEXT("Parried!") : TEXT("Blocked"), bWasParried ? FColor::Yellow : FColor::Cyan);

	// pass control to BP to play effects, etc.
	GuardedAttack(bWasParried, ImpactPoint);
}

void ACombatCharacter::MulticastStagger_Implementation(float Duration)
{
	StaggerEndTime = GetWorld()->GetTimeSeconds() + Duration;

	// consume any cached input so the interrupted attack doesn't restart
	CachedAttackInputTime = 0.0f;

	// lower the shield
	SetBlocking(false);

	// interrupt the attack
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Stop(0.1f, ComboAttackMontage);
	}

	bIsAttacking = false;

	ShowCombatDebugMessage(TEXT("Staggered"), FColor::Orange);

	// pass control to BP to play effects, etc.
	Staggered(Duration);
}

void ACombatCharacter::MulticastDeath_Implementation()
{
	ShowCombatDebugMessage(TEXT("Died"), FColor::Red);

	// disable movement while we're dead
	GetCharacterMovement()->DisableMovement();

	// enable full ragdoll physics
	GetMesh()->SetSimulatePhysics(true);

	// hide the life bar
	LifeBar->SetHiddenInGame(true);

	// pull back the camera
	GetCameraBoom()->TargetArmLength = DeathCameraDistance;
}

void ACombatCharacter::ClientApplyKnockback_Implementation(const FVector& DamageImpulse)
{
	GetCharacterMovement()->AddImpulse(DamageImpulse, true);
}

void ACombatCharacter::DoAttackTrace(FName DamageSourceBone)
{
	// only the server decides what gets hit
	if (!HasAuthority())
	{
		return;
	}

	// sweep for objects in front of the character to be hit by the attack
	TArray<FHitResult> OutHits;

	// start at the provided socket location, sweep forward
	const FVector TraceStart = GetMesh()->GetSocketLocation(DamageSourceBone);
	const FVector TraceEnd = TraceStart + (GetActorForwardVector() * MeleeTraceDistance);

	// check for pawn and world dynamic collision object types
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	// use a sphere shape for the sweep
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(MeleeTraceRadius);

	// ignore self
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->SweepMultiByObjectType(OutHits, TraceStart, TraceEnd, FQuat::Identity, ObjectParams, CollisionShape, QueryParams))
	{
		// a sweep can return several hits on the same actor, only damage each once
		TSet<AActor*> DamagedActors;

		// iterate over each object hit
		for (const FHitResult& CurrentHit : OutHits)
		{
			AActor* HitActor = CurrentHit.GetActor();

			if (!HitActor || DamagedActors.Contains(HitActor))
			{
				continue;
			}

			// no friendly fire
			if (IsAllyOf(Cast<ACombatCharacter>(HitActor)))
			{
				continue;
			}

			// check if we've hit a damageable actor
			ICombatDamageable* Damageable = Cast<ICombatDamageable>(HitActor);

			if (Damageable)
			{
				DamagedActors.Add(HitActor);

				// knock upwards and away from the impact normal
				const FVector Impulse = (CurrentHit.ImpactNormal * -MeleeKnockbackImpulse) + (FVector::UpVector * MeleeLaunchImpulse);

				// pass the damage event to the actor
				Damageable->ApplyDamage(MeleeDamage, this, CurrentHit.ImpactPoint, Impulse);

				// play effects on every machine
				MulticastDealtDamage(MeleeDamage, CurrentHit.ImpactPoint);
			}

			// stop hitting things if a parry interrupted this attack
			if (IsStaggered())
			{
				break;
			}
		}
	}
}

void ACombatCharacter::CheckCombo()
{
	// are we playing an attack animation?
	if (bIsAttacking)
	{
		// is the last attack input not stale?
		if (GetWorld()->GetTimeSeconds() - CachedAttackInputTime <= ComboInputCacheTimeTolerance)
		{
			// consume the attack input so we don't accidentally trigger it twice
			CachedAttackInputTime = 0.0f;

			// increase the combo counter
			++ComboCount;

			// do we still have a combo section to play?
			if (ComboCount < ComboSectionNames.Num())
			{
				// jump to the next combo section
				if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
				{
					AnimInstance->Montage_JumpToSection(ComboSectionNames[ComboCount], ComboAttackMontage);
				}

				// let the other machines follow the combo
				if (HasAuthority())
				{
					MulticastJumpToComboSection(ComboCount);
				}
			}
		}
	}
}

void ACombatCharacter::CheckChargedAttack()
{
	// stub
}

void ACombatCharacter::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	// only the server applies damage
	if (!HasAuthority() || IsDead())
	{
		return;
	}

	// was the attack guarded?
	if (IsFacingAttacker(DamageCauser))
	{
		// auto guard parries without a shield and blocks with one
		const bool bAutoParry = bDebugAutoGuard && !bHasShield;
		const bool bAutoBlock = bDebugAutoGuard && bHasShield;

		// a parry stops the damage and staggers the attacker
		if (bAutoParry || GetWorld()->GetTimeSeconds() < ParryWindowEndTime)
		{
			ParryWindowEndTime = 0.0f;

			if (ACombatCharacter* Attacker = Cast<ACombatCharacter>(DamageCauser))
			{
				Attacker->Stagger(ParryStaggerTime);
			}

			MulticastGuardedAttack(true, DamageLocation);

			return;
		}

		// a block reduces the damage
		if (bIsBlocking || bAutoBlock)
		{
			Damage *= BlockDamageMultiplier;

			MulticastGuardedAttack(false, DamageLocation);

			if (Damage <= 0.0f)
			{
				return;
			}
		}
	}

	// pass the damage event to the actor
	FDamageEvent DamageEvent;
	const float ActualDamage = TakeDamage(Damage, DamageEvent, nullptr, DamageCauser);

	// only process knockback and effects if we received nonzero damage
	if (ActualDamage > 0.0f)
	{
		// apply the knockback impulse
		GetCharacterMovement()->AddImpulse(DamageImpulse, true);

		// remote players move themselves, so they need the impulse too
		if (!IsLocallyControlled())
		{
			ClientApplyKnockback(DamageImpulse);
		}

		// is the character ragdolling?
		if (GetMesh()->IsSimulatingPhysics())
		{
			// apply an impulse to the ragdoll
			GetMesh()->AddImpulseAtLocation(DamageImpulse * GetMesh()->GetMass(), DamageLocation);
		}

		// play the reaction everywhere
		MulticastReceivedDamage(ActualDamage, DamageLocation, DamageImpulse.GetSafeNormal());
	}

}

void ACombatCharacter::HandleDeath()
{
	// ragdoll everywhere
	MulticastDeath();

	// keep count during team battles
	if (TeamId != 0)
	{
		ShowBattleStatus();
	}

	// schedule respawning
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &ACombatCharacter::RespawnCharacter, RespawnTime, false);
}

void ACombatCharacter::ApplyHealing(float Healing, AActor* Healer)
{
	// stub
}

void ACombatCharacter::RespawnCharacter()
{
	// destroy the character and let it be respawned by the Player Controller
	Destroy();
}

float ACombatCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// only process damage on the server, and only if the character is still alive
	if (!HasAuthority() || IsDead())
	{
		return 0.0f;
	}

	// reduce the current HP
	CurrentHP -= Damage;

	// update the life bar on the server, clients update through OnRep
	UpdateLifeBar();

	// have we run out of HP?
	if (IsDead())
	{
		// die
		HandleDeath();
	}

	// return the received damage amount
	return Damage;
}

void ACombatCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// is the character still alive?
	if (!IsDead())
	{
		// disable ragdoll physics
		GetMesh()->SetPhysicsBlendWeight(0.0f);
	}
}

void ACombatCharacter::BeginPlay()
{
	Super::BeginPlay();

	// get the life bar from the widget component
	LifeBarWidget = Cast<UCombatLifeBar>(LifeBar->GetUserWidgetObject());
	check(LifeBarWidget);

	// initialize the camera
	GetCameraBoom()->TargetArmLength = DefaultCameraDistance;

	// save the relative transform for the mesh so we can reset the ragdoll later
	MeshStartingTransform = GetMesh()->GetRelativeTransform();

	// set the life bar color, using the team color if the team already replicated
	OnRep_TeamId();

	// save the walk speed so blocking can restore it
	DefaultWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;

	// the server sets the HP, clients receive it through replication
	if (HasAuthority())
	{
		ResetHP();
	}
	else
	{
		UpdateLifeBar();
	}
}

void ACombatCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the respawn timer
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
}

void ACombatCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACombatCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACombatCharacter::Look);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ACombatCharacter::Look);

		// Combo Attack
		EnhancedInputComponent->BindAction(ComboAttackAction, ETriggerEvent::Started, this, &ACombatCharacter::ComboAttackPressed);

		// Guard
		EnhancedInputComponent->BindAction(GuardAction, ETriggerEvent::Started, this, &ACombatCharacter::GuardPressed);
		EnhancedInputComponent->BindAction(GuardAction, ETriggerEvent::Completed, this, &ACombatCharacter::GuardReleased);
		EnhancedInputComponent->BindAction(GuardAction, ETriggerEvent::Canceled, this, &ACombatCharacter::GuardReleased);
	}
}

void ACombatCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// update the respawn transform on the Player Controller
	if (ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetController()))
	{
		PC->SetRespawnTransform(GetActorTransform());
	}
}

void ACombatCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACombatCharacter, CurrentHP);
	DOREPLIFETIME(ACombatCharacter, MaxHP);

	DOREPLIFETIME(ACombatCharacter, bIsBlocking);
	DOREPLIFETIME(ACombatCharacter, TeamId);
}
