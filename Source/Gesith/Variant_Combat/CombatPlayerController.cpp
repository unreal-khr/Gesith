// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Combat/CombatPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "CombatCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Gesith.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "EngineUtils.h"
#include "GesithCastle.h"
#include "CastleMenuWidget.h"
#include "Engine/Engine.h"
#include "CombatSoldierAIController.h"
#include "GameFramework/PlayerState.h"

void ACombatPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (SVirtualJoystick::ShouldDisplayTouchInterface() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogGesith, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ACombatPlayerController::SetupInputComponent()
{
	// the engine creates the InputComponent here; without it nothing can be bound
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// add the input mapping context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!SVirtualJoystick::ShouldDisplayTouchInterface())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}

			// the castle menu input is built here instead of in an asset
			if (!CastleMenuAction)
			{
				CastleMenuAction = NewObject<UInputAction>(this);
				CastleMenuAction->ValueType = EInputActionValueType::Boolean;
			}

			if (!SquadFollowAction)
			{
				SquadFollowAction = NewObject<UInputAction>(this);
				SquadHoldAction = NewObject<UInputAction>(this);
				SquadChargeAction = NewObject<UInputAction>(this);
			}

			if (!CastleMappingContext)
			{
				CastleMappingContext = NewObject<UInputMappingContext>(this);
				CastleMappingContext->MapKey(CastleMenuAction, CastleMenuKey);
				CastleMappingContext->MapKey(SquadFollowAction, SquadFollowKey);
				CastleMappingContext->MapKey(SquadHoldAction, SquadHoldKey);
				CastleMappingContext->MapKey(SquadChargeAction, SquadChargeKey);
			}

			Subsystem->AddMappingContext(CastleMappingContext, 1);
		}

		UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);

		if (EnhancedInputComponent && CastleMenuAction)
		{
			EnhancedInputComponent->BindAction(CastleMenuAction, ETriggerEvent::Started, this, &ACombatPlayerController::ToggleCastleMenu);
			EnhancedInputComponent->BindAction(SquadFollowAction, ETriggerEvent::Started, this, &ACombatPlayerController::OrderSquadFollow);
			EnhancedInputComponent->BindAction(SquadHoldAction, ETriggerEvent::Started, this, &ACombatPlayerController::OrderSquadHold);
			EnhancedInputComponent->BindAction(SquadChargeAction, ETriggerEvent::Started, this, &ACombatPlayerController::OrderSquadCharge);
		}
		else
		{
			UE_LOG(LogGesith, Warning, TEXT("Castle menu input could not be bound (no enhanced input component)."));
		}
	}
}

void ACombatPlayerController::OrderSquadFollow()
{
	GiveSquadOrder(ESquadOrder::Follow);
}

void ACombatPlayerController::OrderSquadHold()
{
	GiveSquadOrder(ESquadOrder::Hold);
}

void ACombatPlayerController::OrderSquadCharge()
{
	GiveSquadOrder(ESquadOrder::Charge);
}

void ACombatPlayerController::GiveSquadOrder(ESquadOrder Order)
{
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		static const TCHAR* OrderNames[] = { TEXT("Charge!"), TEXT("Follow me"), TEXT("Hold here") };

		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, FString::Printf(TEXT("Squad: %s"), OrderNames[static_cast<uint8>(Order)]));
	}
#endif

	ServerSquadOrder(Order);
}

void ACombatPlayerController::ServerSquadOrder_Implementation(ESquadOrder Order)
{
	if (ACombatCharacter* Leader = Cast<ACombatCharacter>(GetPawn()))
	{
		ACombatSoldierAIController::OrderSquad(Leader, Order);
	}
}

void ACombatPlayerController::CastleMenu()
{
	ToggleCastleMenu();
}

void ACombatPlayerController::ShowCastleDebugMessage(const FString& Message) const
{
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Yellow, FString::Printf(TEXT("Castle menu: %s"), *Message));
	}
#endif
}

void ACombatPlayerController::ToggleCastleMenu()
{
	if (CastleMenuWidget)
	{
		CloseCastleMenu();
		return;
	}

	AGesithCastle* Castle = FindCastleAtPawn();

	if (!Castle)
	{
		int32 CastleCount = 0;

		for (TActorIterator<AGesithCastle> It(GetWorld()); It; ++It)
		{
			++CastleCount;
		}

		ShowCastleDebugMessage(CastleCount == 0
			? TEXT("no castle in this level")
			: FString::Printf(TEXT("not inside a castle (%d in level, you are at %s)"), CastleCount, GetPawn() ? *GetPawn()->GetActorLocation().ToCompactString() : TEXT("no pawn")));

		return;
	}

	CastleMenuWidget = CreateWidget<UCastleMenuWidget>(this, UCastleMenuWidget::StaticClass());

	if (!CastleMenuWidget)
	{
		ShowCastleDebugMessage(TEXT("could not create the menu widget"));
		return;
	}

	{
		CastleMenuWidget->SetCastle(Castle);
		CastleMenuWidget->AddToPlayerScreen(10);

		ShowCastleDebugMessage(FString::Printf(TEXT("opened %s"), *Castle->CastleName));

		// free the mouse so the buttons can be clicked, while the game keeps running
		SetShowMouseCursor(true);

		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
}

void ACombatPlayerController::CloseCastleMenu()
{
	if (CastleMenuWidget)
	{
		CastleMenuWidget->RemoveFromParent();
		CastleMenuWidget = nullptr;
	}

	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
}

AGesithCastle* ACombatPlayerController::FindCastleAtPawn() const
{
	if (!GetPawn())
	{
		return nullptr;
	}

	for (TActorIterator<AGesithCastle> It(GetWorld()); It; ++It)
	{
		if (It->IsInsideCastle(GetPawn()->GetActorLocation()))
		{
			return *It;
		}
	}

	return nullptr;
}

bool ACombatPlayerController::IsInCastle(const AGesithCastle* Castle) const
{
	return Castle && GetPawn() && Castle->IsInsideCastle(GetPawn()->GetActorLocation());
}

bool ACombatPlayerController::CanManageCastle(const AGesithCastle* Castle) const
{
	return IsInCastle(Castle) && Castle->CanManage(PlayerState);
}

bool ACombatPlayerController::IsLordInCastle(const AGesithCastle* Castle) const
{
	return IsInCastle(Castle) && PlayerState && Castle->Lord == PlayerState;
}

void ACombatPlayerController::ServerCastleRecruit_Implementation(AGesithCastle* Castle)
{
	if (CanManageCastle(Castle))
	{
		Castle->Recruit();
	}
}

void ACombatPlayerController::ServerCastleSetTraining_Implementation(AGesithCastle* Castle, int32 SoldierId, bool bTraining)
{
	if (CanManageCastle(Castle))
	{
		Castle->SetTraining(SoldierId, bTraining);
	}
}

void ACombatPlayerController::ServerCastlePromote_Implementation(AGesithCastle* Castle, int32 SoldierId)
{
	if (CanManageCastle(Castle))
	{
		Castle->Promote(SoldierId);
	}
}

void ACombatPlayerController::ServerCastleMuster_Implementation(AGesithCastle* Castle)
{
	ACombatCharacter* Leader = Cast<ACombatCharacter>(GetPawn());

	if (Leader && CanManageCastle(Castle))
	{
		Leader->MusterFromCastle(Castle);
	}
}

void ACombatPlayerController::ServerCastleDismiss_Implementation(AGesithCastle* Castle)
{
	ACombatCharacter* Leader = Cast<ACombatCharacter>(GetPawn());

	// anyone leading the castle's soldiers can send them home, even after losing the right to manage
	if (Leader && IsInCastle(Castle))
	{
		Leader->DismissToCastle(Castle);
	}
}

void ACombatPlayerController::ServerCastleClaim_Implementation(AGesithCastle* Castle)
{
	if (IsInCastle(Castle))
	{
		Castle->Claim(PlayerState);
	}
}

void ACombatPlayerController::ServerCastleRequestKnight_Implementation(AGesithCastle* Castle)
{
	if (IsInCastle(Castle))
	{
		Castle->RequestKnighthood(PlayerState);
	}
}

void ACombatPlayerController::ServerCastleAnswerKnightRequest_Implementation(AGesithCastle* Castle, APlayerState* TargetPlayer, bool bAccept)
{
	if (IsLordInCastle(Castle))
	{
		Castle->AnswerKnightRequest(TargetPlayer, bAccept);
	}
}

void ACombatPlayerController::ServerCastleDismissKnight_Implementation(AGesithCastle* Castle, APlayerState* TargetPlayer)
{
	if (IsLordInCastle(Castle))
	{
		Castle->RemoveKnight(TargetPlayer);
	}
}

void ACombatPlayerController::ServerCastleLeaveKnighthood_Implementation(AGesithCastle* Castle)
{
	if (IsInCastle(Castle))
	{
		Castle->RemoveKnight(PlayerState);
	}
}

void ACombatPlayerController::ServerCastleSetKnightPermission_Implementation(AGesithCastle* Castle, EKnightPermission Permission)
{
	if (IsLordInCastle(Castle))
	{
		Castle->SetKnightPermission(Permission);
	}
}

void ACombatPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// subscribe to the pawn's OnDestroyed delegate
	InPawn->OnDestroyed.AddDynamic(this, &ACombatPlayerController::OnPawnDestroyed);
}

void ACombatPlayerController::SetRespawnTransform(const FTransform& NewRespawn)
{
	// save the new respawn transform
	RespawnTransform = NewRespawn;
}

void ACombatPlayerController::OnPawnDestroyed(AActor* DestroyedActor)
{
	// spawn a new character at the respawn transform
	if (ACombatCharacter* RespawnedCharacter = GetWorld()->SpawnActor<ACombatCharacter>(CharacterClass, RespawnTransform))
	{
		// possess the character
		Possess(RespawnedCharacter);
	}
}