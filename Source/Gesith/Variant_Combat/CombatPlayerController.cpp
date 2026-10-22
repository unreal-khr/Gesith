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
			CastleMenuAction = NewObject<UInputAction>(this, TEXT("IA_CastleMenu"));
			CastleMenuAction->ValueType = EInputActionValueType::Boolean;

			CastleMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Castle"));
			CastleMappingContext->MapKey(CastleMenuAction, CastleMenuKey);

			Subsystem->AddMappingContext(CastleMappingContext, 1);
		}

		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
		{
			EnhancedInputComponent->BindAction(CastleMenuAction, ETriggerEvent::Started, this, &ACombatPlayerController::ToggleCastleMenu);
		}
	}
}

void ACombatPlayerController::ToggleCastleMenu()
{
	if (CastleMenu)
	{
		CloseCastleMenu();
		return;
	}

	AGesithCastle* Castle = FindCastleAtPawn();

	if (!Castle)
	{
		return;
	}

	CastleMenu = CreateWidget<UCastleMenuWidget>(this, UCastleMenuWidget::StaticClass());

	if (CastleMenu)
	{
		CastleMenu->SetCastle(Castle);
		CastleMenu->AddToPlayerScreen(10);

		// free the mouse so the buttons can be clicked, while the game keeps running
		SetShowMouseCursor(true);

		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
}

void ACombatPlayerController::CloseCastleMenu()
{
	if (CastleMenu)
	{
		CastleMenu->RemoveFromParent();
		CastleMenu = nullptr;
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

bool ACombatPlayerController::CanManageCastle(const AGesithCastle* Castle) const
{
	return Castle && GetPawn() && Castle->IsInsideCastle(GetPawn()->GetActorLocation());
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