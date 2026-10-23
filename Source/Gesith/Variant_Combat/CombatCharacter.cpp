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
#include "GesithArrow.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "CombatCharacter"

namespace
{
	/** Stats used for an empty hand or an unknown weapon */
	const FGesithWeaponStats BareHands;

	/** Placeholder weapon helper: a basic shape stretched along the socket's Z axis, starting at the hand */
	FGesithWeaponStats MakeWeapon(const FText& Name, UStaticMesh* Mesh, const FVector& Scale, float GripOffset)
	{
		FGesithWeaponStats Stats;
		Stats.DisplayName = Name;
		Stats.Mesh = Mesh;
		Stats.MeshTransform = FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, GripOffset), Scale);

		return Stats;
	}
}

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

	// placeholder weapon and shield meshes, until real weapon art exists
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh(), FName("hand_r"));
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
	ShieldMesh->SetupAttachment(GetMesh(), ShieldSocket);
	ShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShieldMesh->SetStaticMesh(CylinderMesh.Object);

	// a round shield, flat against the forearm
	ShieldTransform = FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.6f, 0.6f, 0.04f));

	// the 1-4 slots: one melee weapon and the bow. Every number is a starting point for balancing
	WeaponSlots = { EGesithWeaponType::Sword, EGesithWeaponType::Bow, EGesithWeaponType::None, EGesithWeaponType::None };

	// each melee line has a one handed version with a shield and a two handed version that trades the shield for damage and reach
	FGesithWeaponStats Sword = MakeWeapon(LOCTEXT("Sword", "한손검"), CubeMesh.Object, FVector(0.01f, 0.06f, 0.9f), 45.0f);
	WeaponStats.Add(EGesithWeaponType::Sword, Sword);

	FGesithWeaponStats GreatSword = MakeWeapon(LOCTEXT("GreatSword", "양손검"), CubeMesh.Object, FVector(0.015f, 0.08f, 1.3f), 65.0f);
	GreatSword.bTwoHanded = true;
	GreatSword.DamageMultiplier = 1.4f;
	GreatSword.AttackSpeed = 0.85f;
	GreatSword.Reach = 95.0f;
	WeaponStats.Add(EGesithWeaponType::GreatSword, GreatSword);

	FGesithWeaponStats Spear = MakeWeapon(LOCTEXT("Spear", "한손창"), CylinderMesh.Object, FVector(0.035f, 0.035f, 1.6f), 50.0f);
	Spear.DamageMultiplier = 0.9f;
	Spear.AttackSpeed = 0.9f;
	Spear.Reach = 120.0f;
	Spear.SwingRadius = 45.0f;
	WeaponStats.Add(EGesithWeaponType::Spear, Spear);

	FGesithWeaponStats GreatSpear = MakeWeapon(LOCTEXT("GreatSpear", "양손창"), CylinderMesh.Object, FVector(0.04f, 0.04f, 2.2f), 60.0f);
	GreatSpear.bTwoHanded = true;
	GreatSpear.DamageMultiplier = 1.2f;
	GreatSpear.AttackSpeed = 0.8f;
	GreatSpear.Reach = 170.0f;
	GreatSpear.SwingRadius = 40.0f;
	WeaponStats.Add(EGesithWeaponType::GreatSpear, GreatSpear);

	FGesithWeaponStats Axe = MakeWeapon(LOCTEXT("Axe", "한손도끼"), CylinderMesh.Object, FVector(0.05f, 0.05f, 0.6f), 25.0f);
	Axe.DamageMultiplier = 1.3f;
	Axe.AttackSpeed = 0.8f;
	WeaponStats.Add(EGesithWeaponType::Axe, Axe);

	FGesithWeaponStats GreatAxe = MakeWeapon(LOCTEXT("GreatAxe", "양손도끼"), CylinderMesh.Object, FVector(0.06f, 0.06f, 1.1f), 45.0f);
	GreatAxe.bTwoHanded = true;
	GreatAxe.DamageMultiplier = 1.8f;
	GreatAxe.AttackSpeed = 0.65f;
	GreatAxe.Reach = 90.0f;
	WeaponStats.Add(EGesithWeaponType::GreatAxe, GreatAxe);

	// ranged, needs both hands. Held upright in the left hand
	FGesithWeaponStats Bow = MakeWeapon(LOCTEXT("Bow", "활"), CylinderMesh.Object, FVector(0.03f, 0.03f, 1.3f), 0.0f);
	Bow.bRanged = true;
	Bow.bTwoHanded = true;
	Bow.DamageMultiplier = 1.5f;
	Bow.Socket = FName("hand_l");
	WeaponStats.Add(EGesithWeaponType::Bow, Bow);

	ArrowClass = AGesithArrow::StaticClass();

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

void ACombatCharacter::ComboAttackReleased()
{
	// route the input
	DoComboAttackEnd();
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
	// a bow is drawn while the input is held and shot on release
	if (GetCurrentWeaponStats().bRanged)
	{
		StartDraw();

		if (bIsDrawing && !HasAuthority())
		{
			ServerStartDraw();
		}

		return;
	}

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
	if (!bIsDrawing)
	{
		return;
	}

	// players shoot where the crosshair points, AI soldiers aim with ReleaseBowAt instead
	const FVector AimPoint = IsLocallyControlled() && IsPlayerControlled() ? GetCameraAimPoint() : GetActorLocation() + GetActorForwardVector() * 5000.0f;

	if (HasAuthority())
	{
		ReleaseDraw(AimPoint);
	}
	else
	{
		// the server shoots the arrow, the client only stops drawing
		CancelDraw();

		ServerReleaseDraw(AimPoint);
	}
}

void ACombatCharacter::DoGuardStart()
{
	// with a bow the guard input aims instead
	if (GetCurrentWeaponStats().bRanged)
	{
		SetAimZoom(true);

		return;
	}

	// no guarding while taking out a weapon
	if (IsSwappingWeapon())
	{
		return;
	}

	if (HasShieldInHand())
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
	SetAimZoom(false);

	// lower the shield even if the weapon changed while it was up
	if (bIsBlocking || HasShieldInHand())
	{
		// lower the shield locally, then tell the server
		SetBlocking(false);

		if (!HasAuthority())
		{
			ServerSetBlocking(false);
		}
	}
}

void ACombatCharacter::SelectWeaponSlot(int32 Slot)
{
	if (!CanSelectWeaponSlot(Slot))
	{
		if (WeaponSlots.IsValidIndex(Slot) && WeaponSlots[Slot] == EGesithWeaponType::None)
		{
			ShowLocalMessage(FString::Printf(TEXT("Slot %d is empty"), Slot + 1), FColor::White);
		}

		return;
	}

	// swap locally right away, the server runs the same checks
	ApplyWeaponSlot(Slot);

	if (!HasAuthority())
	{
		ServerSelectWeaponSlot(Slot);
	}
}

void ACombatCharacter::ServerSelectWeaponSlot_Implementation(int32 Slot)
{
	if (CanSelectWeaponSlot(Slot))
	{
		ApplyWeaponSlot(Slot);
	}
	else
	{
		// put the owner back to the weapon the server still has in hand
		ClientRejectWeaponSlot(CurrentWeaponSlot);
	}
}

void ACombatCharacter::ClientRejectWeaponSlot_Implementation(int32 Slot)
{
	CurrentWeaponSlot = Slot;
	SwapEndTime = 0.0f;

	UpdateWeaponLook();
}

bool ACombatCharacter::CanSelectWeaponSlot(int32 Slot) const
{
	return WeaponSlots.IsValidIndex(Slot) && WeaponSlots[Slot] != EGesithWeaponType::None && Slot != CurrentWeaponSlot
		&& !IsDead() && !IsStaggered() && !bIsAttacking;
}

void ACombatCharacter::ApplyWeaponSlot(int32 Slot)
{
	// put the shield down and stop drawing before changing hands
	SetBlocking(false);
	CancelDraw();
	SetAimZoom(false);

	CurrentWeaponSlot = Slot;
	SwapEndTime = GetWorld()->GetTimeSeconds() + WeaponSwapTime;

	UpdateWeaponLook();

	ShowLocalMessage(FString::Printf(TEXT("Weapon %d: %s"), Slot + 1, *GetCurrentWeaponStats().DisplayName.ToString()), FColor::White);
}

void ACombatCharacter::OnRep_CurrentWeaponSlot()
{
	// other players see the swap, and the owner gets corrected if the server refused it
	SwapEndTime = GetWorld()->GetTimeSeconds() + WeaponSwapTime;

	UpdateWeaponLook();
}

EGesithWeaponType ACombatCharacter::GetWeaponInSlot(int32 Slot) const
{
	return WeaponSlots.IsValidIndex(Slot) ? WeaponSlots[Slot] : EGesithWeaponType::None;
}

const FGesithWeaponStats& ACombatCharacter::GetWeaponStats(EGesithWeaponType Type) const
{
	const FGesithWeaponStats* Stats = WeaponStats.Find(Type);

	return Stats ? *Stats : BareHands;
}

int32 ACombatCharacter::FindWeaponSlot(bool bRanged) const
{
	for (int32 Slot = 0; Slot < WeaponSlots.Num(); ++Slot)
	{
		if (WeaponSlots[Slot] != EGesithWeaponType::None && GetWeaponStats(WeaponSlots[Slot]).bRanged == bRanged)
		{
			return Slot;
		}
	}

	return -1;
}

bool ACombatCharacter::IsSwappingWeapon() const
{
	return GetWorld()->GetTimeSeconds() < SwapEndTime;
}

bool ACombatCharacter::HasShieldInHand() const
{
	const FGesithWeaponStats& Stats = GetCurrentWeaponStats();

	return bHasShield && !Stats.bTwoHanded && !Stats.bRanged;
}

float ACombatCharacter::GetDrawFraction() const
{
	if (!bIsDrawing)
	{
		return 0.0f;
	}

	return FMath::Clamp((GetWorld()->GetTimeSeconds() - DrawStartTime) / GetCurrentWeaponStats().DrawTime, 0.0f, 1.0f);
}

void ACombatCharacter::UpdateWeaponLook()
{
	const FGesithWeaponStats& Stats = GetCurrentWeaponStats();

	WeaponMesh->SetStaticMesh(Stats.Mesh);
	WeaponMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Stats.Socket);
	WeaponMesh->SetRelativeTransform(Stats.MeshTransform);
	WeaponMesh->SetVisibility(Stats.Mesh != nullptr);

	ShieldMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, ShieldSocket);
	ShieldMesh->SetRelativeTransform(ShieldTransform);
	ShieldMesh->SetVisibility(HasShieldInHand());

	UpdateFacing();
}

void ACombatCharacter::StartDraw()
{
	if (bIsDrawing || IsDead() || IsStaggered() || IsSwappingWeapon() || !GetCurrentWeaponStats().bRanged)
	{
		return;
	}

	if (Arrows <= 0)
	{
		ShowLocalMessage(TEXT("Out of arrows. Refill inside a castle"), FColor::Orange);
		return;
	}

	bIsDrawing = true;
	DrawStartTime = GetWorld()->GetTimeSeconds();

	UpdateFacing();
}

void ACombatCharacter::CancelDraw()
{
	if (bIsDrawing)
	{
		bIsDrawing = false;

		UpdateFacing();
	}
}

void ACombatCharacter::ServerStartDraw_Implementation()
{
	StartDraw();
}

void ACombatCharacter::ServerReleaseDraw_Implementation(const FVector_NetQuantize& AimPoint)
{
	ReleaseDraw(AimPoint);
}

void ACombatCharacter::SetSquadRanged(bool bRanged)
{
	if (HasAuthority())
	{
		bSquadRanged = bRanged;
	}
}

void ACombatCharacter::ReleaseBowAt(const FVector& AimPoint)
{
	if (HasAuthority())
	{
		ReleaseDraw(AimPoint);
	}
}

void ACombatCharacter::ReleaseDraw(const FVector& AimPoint)
{
	if (!bIsDrawing)
	{
		return;
	}

	const float DrawFraction = GetDrawFraction();

	CancelDraw();

	// let go too early and the arrow stays on the string
	if (DrawFraction < MinDrawFraction || Arrows <= 0 || !ArrowClass)
	{
		return;
	}

	const FGesithWeaponStats& Stats = GetCurrentWeaponStats();

	// leave from in front of the chest
	const FVector Forward = GetActorForwardVector();
	const FVector Muzzle = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f) + Forward * 60.0f;

	// never shoot backwards, even if the camera looks behind the character
	FVector Direction = (AimPoint - Muzzle).GetSafeNormal();

	if (FVector::DotProduct(Direction, Forward) < 0.0f)
	{
		Direction = Forward;
	}

	AGesithArrow* Arrow = GetWorld()->SpawnActorDeferred<AGesithArrow>(ArrowClass, FTransform(Direction.Rotation(), Muzzle), this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Arrow)
	{
		return;
	}

	// a half drawn bow shoots slower and hits softer
	Arrow->Launch(MeleeDamage * Stats.DamageMultiplier * DrawFraction, Direction * Stats.ArrowSpeed * (0.5f + 0.5f * DrawFraction));
	Arrow->FinishSpawning(FTransform(Direction.Rotation(), Muzzle));

	--Arrows;
}

void ACombatCharacter::UpdateFacing()
{
	// AI soldiers turn themselves toward their target, and the walking setting isn't saved before BeginPlay
	if (!IsPlayerControlled() || !HasActorBegunPlay())
	{
		return;
	}

	// an archer faces where the camera looks the whole time the bow is out, not only while drawing
	const bool bFaceCamera = bIsDrawing || GetCurrentWeaponStats().bRanged;

	// otherwise go back to however the Blueprint turns the character
	bUseControllerRotationYaw = bFaceCamera || bDefaultUseControllerRotationYaw;
	GetCharacterMovement()->bOrientRotationToMovement = bFaceCamera ? false : bDefaultOrientRotationToMovement;
}

void ACombatCharacter::SetAimZoom(bool bZoom)
{
	if (IsLocallyControlled() && IsPlayerControlled())
	{
		GetFollowCamera()->SetFieldOfView(bZoom ? AimFieldOfView : DefaultFieldOfView);
	}
}

FVector ACombatCharacter::GetCameraAimPoint() const
{
	const FVector Start = GetFollowCamera()->GetComponentLocation();
	const FVector End = Start + GetFollowCamera()->GetForwardVector() * 20000.0f;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	FHitResult Hit;

	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams) ? Hit.ImpactPoint : End;
}

void ACombatCharacter::CheckArrowRefill()
{
	if (Arrows >= MaxArrows || IsDead())
	{
		return;
	}

	for (TActorIterator<AGesithCastle> It(GetWorld()); It; ++It)
	{
		// an enemy castle doesn't hand out arrows
		const bool bEnemyCastle = TeamId != 0 && It->TeamId != 0 && It->TeamId != TeamId;

		if (!bEnemyCastle && It->IsInsideCastle(GetActorLocation()))
		{
			Arrows = MaxArrows;

			ShowCombatDebugMessage(TEXT("Arrows refilled"), FColor::Green);

			return;
		}
	}
}

void ACombatCharacter::WeaponPose(float Pitch, float Yaw, float Roll, float X, float Y, float Z)
{
#if !UE_BUILD_SHIPPING
	FGesithWeaponStats* Stats = WeaponStats.Find(GetWeaponInSlot(CurrentWeaponSlot));

	if (!Stats)
	{
		return;
	}

	Stats->MeshTransform.SetRotation(FRotator(Pitch, Yaw, Roll).Quaternion());
	Stats->MeshTransform.SetLocation(FVector(X, Y, Z));

	UpdateWeaponLook();

	ShowLocalMessage(FString::Printf(TEXT("%s pose: Pitch %.0f Yaw %.0f Roll %.0f, X %.0f Y %.0f Z %.0f"), *Stats->DisplayName.ToString(), Pitch, Yaw, Roll, X, Y, Z), FColor::Green);
#endif
}

void ACombatCharacter::ShieldPose(float Pitch, float Yaw, float Roll, float X, float Y, float Z)
{
#if !UE_BUILD_SHIPPING
	ShieldTransform.SetRotation(FRotator(Pitch, Yaw, Roll).Quaternion());
	ShieldTransform.SetLocation(FVector(X, Y, Z));

	UpdateWeaponLook();

	ShowLocalMessage(FString::Printf(TEXT("Shield pose: Pitch %.0f Yaw %.0f Roll %.0f, X %.0f Y %.0f Z %.0f"), Pitch, Yaw, Roll, X, Y, Z), FColor::Green);
#endif
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

void ACombatCharacter::ApplySoldierStats(const FSoldierRecord& Record)
{
	if (!HasAuthority())
	{
		return;
	}

	// start from the class defaults so applying stats again after training doesn't stack
	const ACombatCharacter* Defaults = GetClass()->GetDefaultObject<ACombatCharacter>();

	// a fresh recruit (10) fights like the base character, the stat cap (100) is much tougher
	MaxHP = Defaults->MaxHP * (0.5f + Record.Health / 20.0f);
	MeleeDamage = Defaults->MeleeDamage * (0.5f + Record.Strength / 20.0f);

	SetMeleeWeapon(Record.MeleeWeapon);

	ResetHP();
}

void ACombatCharacter::SetMeleeWeapon(EGesithWeaponType Weapon)
{
	// only melee weapons go in slot 1
	if (!HasAuthority() || Weapon == EGesithWeaponType::None || GetWeaponStats(Weapon).bRanged)
	{
		return;
	}

	MeleeWeapon = Weapon;

	UpdateMeleeSlot();
}

void ACombatCharacter::UpdateMeleeSlot()
{
	if (WeaponSlots.IsValidIndex(0))
	{
		WeaponSlots[0] = MeleeWeapon;
	}

	UpdateWeaponLook();
}

void ACombatCharacter::OnRep_MeleeWeapon()
{
	UpdateMeleeSlot();
}

void ACombatCharacter::SetMelee(int32 WeaponNumber)
{
	if (HasAuthority())
	{
		ServerSetMelee_Implementation(WeaponNumber);
	}
	else
	{
		ServerSetMelee(WeaponNumber);
	}
}

void ACombatCharacter::ServerSetMelee_Implementation(int32 WeaponNumber)
{
#if !UE_BUILD_SHIPPING
	static const EGesithWeaponType MeleeWeapons[] = { EGesithWeaponType::Sword, EGesithWeaponType::GreatSword, EGesithWeaponType::Spear, EGesithWeaponType::GreatSpear, EGesithWeaponType::Axe, EGesithWeaponType::GreatAxe };

	if (WeaponNumber >= 1 && WeaponNumber <= static_cast<int32>(UE_ARRAY_COUNT(MeleeWeapons)))
	{
		SetMeleeWeapon(MeleeWeapons[WeaponNumber - 1]);

		ShowCombatDebugMessage(FString::Printf(TEXT("Melee weapon: %s"), *GetCurrentWeaponStats().DisplayName.ToString()), FColor::Green);
	}
#endif
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
	// ignore attack inputs while dead, staggered, holding up a shield or taking out a weapon
	if (IsDead() || IsStaggered() || bIsBlocking || IsSwappingWeapon() || GetCurrentWeaponStats().bRanged)
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
		// real attack speed is the attack speed stat times the weapon's speed
		const float PlayRate = AttackSpeed * GetCurrentWeaponStats().AttackSpeed;
		const float MontageLength = AnimInstance->Montage_Play(ComboAttackMontage, PlayRate, EMontagePlayReturnType::MontageLength, 0.0f, true);

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
	// can't raise the shield while dead, staggered, attacking or without a shield on the arm
	if (bNewBlocking && (IsDead() || IsStaggered() || bIsAttacking || !HasShieldInHand()))
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
	// the server only parries with weapons that can parry
	if (!HasShieldInHand() && !GetCurrentWeaponStats().bRanged && !IsSwappingWeapon())
	{
		TryParry();
	}
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

void ACombatCharacter::ShowLocalMessage(const FString& Message, const FColor& Color) const
{
#if !UE_BUILD_SHIPPING
	if (GEngine && IsLocallyControlled() && IsPlayerControlled())
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, Color, Message);
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

	// lower the shield and the bow
	SetBlocking(false);
	CancelDraw();

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
	const FGesithWeaponStats& Weapon = GetCurrentWeaponStats();
	const FVector TraceEnd = TraceStart + (GetActorForwardVector() * Weapon.Reach);

	// check for pawn and world dynamic collision object types
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	// use a sphere shape for the sweep
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(Weapon.SwingRadius);

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

				// the weapon in hand scales the damage
				const float Damage = MeleeDamage * Weapon.DamageMultiplier;

				// pass the damage event to the actor
				Damageable->ApplyDamage(Damage, this, CurrentHit.ImpactPoint, Impulse);

				// play effects on every machine
				MulticastDealtDamage(Damage, CurrentHit.ImpactPoint);
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
		// auto guard parries without a shield and blocks with one. Nothing guards while holding a bow
		const bool bCanGuard = !GetCurrentWeaponStats().bRanged;
		const bool bAutoParry = bDebugAutoGuard && bCanGuard && !HasShieldInHand();
		const bool bAutoBlock = bDebugAutoGuard && HasShieldInHand();

		// arrows can be blocked with a shield but not parried
		ACombatCharacter* Attacker = Cast<ACombatCharacter>(DamageCauser);

		// a parry stops the damage and staggers the attacker
		if (Attacker && (bAutoParry || GetWorld()->GetTimeSeconds() < ParryWindowEndTime))
		{
			ParryWindowEndTime = 0.0f;

			Attacker->Stagger(ParryStaggerTime);

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

	// save the camera and turning settings so the bow can restore them
	DefaultFieldOfView = GetFollowCamera()->FieldOfView;
	bDefaultOrientRotationToMovement = GetCharacterMovement()->bOrientRotationToMovement;
	bDefaultUseControllerRotationYaw = bUseControllerRotationYaw;

	// slot 1 holds the melee weapon, then show the weapon in hand
	UpdateMeleeSlot();

	if (HasAuthority())
	{
		Arrows = MaxArrows;

		GetWorld()->GetTimerManager().SetTimer(ArrowRefillTimer, this, &ACombatCharacter::CheckArrowRefill, ArrowRefillInterval, true, FMath::FRandRange(0.0f, ArrowRefillInterval));
	}

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

	// clear the timers
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
	GetWorld()->GetTimerManager().ClearTimer(ArrowRefillTimer);
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
		EnhancedInputComponent->BindAction(ComboAttackAction, ETriggerEvent::Completed, this, &ACombatCharacter::ComboAttackReleased);
		EnhancedInputComponent->BindAction(ComboAttackAction, ETriggerEvent::Canceled, this, &ACombatCharacter::ComboAttackReleased);

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

	// the bow turns the character with the camera only once a player controls it
	UpdateWeaponLook();
}

void ACombatCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACombatCharacter, CurrentHP);
	DOREPLIFETIME(ACombatCharacter, MaxHP);

	DOREPLIFETIME(ACombatCharacter, bIsBlocking);
	DOREPLIFETIME(ACombatCharacter, TeamId);

	DOREPLIFETIME(ACombatCharacter, CurrentWeaponSlot);
	DOREPLIFETIME(ACombatCharacter, Arrows);
	DOREPLIFETIME(ACombatCharacter, MaxArrows);
	DOREPLIFETIME(ACombatCharacter, MeleeWeapon);
	DOREPLIFETIME_CONDITION(ACombatCharacter, bSquadRanged, COND_OwnerOnly);
}

#undef LOCTEXT_NAMESPACE
