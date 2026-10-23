// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CombatPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class ACombatCharacter;
class AGesithCastle;
class UCastleMenuWidget;

/**
 *  Simple Player Controller for a third person combat game
 *  Manages input mappings
 *  Respawns the player character at the checkpoint when it's destroyed
 */
UCLASS(abstract)
class ACombatPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:

	/** Input mapping context for this player */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** Character class to respawn when the possessed pawn is destroyed */
	UPROPERTY(EditAnywhere, Category="Respawn")
	TSubclassOf<ACombatCharacter> CharacterClass;

	/** Transform to respawn the character at. Can be set to create checkpoints */
	FTransform RespawnTransform;

	/** Key that opens and closes the castle menu */
	UPROPERTY(EditAnywhere, Category="Input|Castle")
	FKey CastleMenuKey = EKeys::Tab;

	/** Castle menu input, created at runtime so it needs no input assets */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CastleMenuAction;

	/** Mapping context holding the castle menu input */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CastleMappingContext;

	/** Castle menu while it's open */
	UPROPERTY(Transient)
	TObjectPtr<UCastleMenuWidget> CastleMenuWidget;

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;

	/** Pawn initialization */
	virtual void OnPossess(APawn* InPawn) override;

public:

	/** Updates the character respawn transform */
	void SetRespawnTransform(const FTransform& NewRespawn);

	/** Asks the server to recruit a soldier at the castle */
	UFUNCTION(Server, Reliable)
	void ServerCastleRecruit(AGesithCastle* Castle);

	/** Asks the server to start or stop a soldier's training */
	UFUNCTION(Server, Reliable)
	void ServerCastleSetTraining(AGesithCastle* Castle, int32 SoldierId, bool bTraining);

	/** Asks the server to promote a soldier */
	UFUNCTION(Server, Reliable)
	void ServerCastlePromote(AGesithCastle* Castle, int32 SoldierId);

	/** Console command: opens or closes the castle menu, same as the castle menu key */
	UFUNCTION(Exec)
	void CastleMenu();

protected:

	/** Opens the castle menu if the pawn is inside a castle, or closes it */
	void ToggleCastleMenu();

	/** Prints a castle menu message on screen for testing. Does nothing in shipping builds */
	void ShowCastleDebugMessage(const FString& Message) const;

	/** Closes the castle menu and gives the mouse back to the game */
	void CloseCastleMenu();

	/** Returns the castle the pawn is standing in, or nullptr */
	AGesithCastle* FindCastleAtPawn() const;

	/** Returns true if the castle exists and the pawn is inside it. Used to check requests on the server */
	bool CanManageCastle(const AGesithCastle* Castle) const;

protected:

	/** Called if the possessed pawn is destroyed */
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedActor);

};
