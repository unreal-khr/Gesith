// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SquadTypes.h"
#include "GesithSoldierTypes.h"
#include "CombatPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class ACombatCharacter;
class AGesithCastle;
class UCastleMenuWidget;
class APlayerState;
class UWeaponHUDWidget;

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
	/** Key that orders the squad to follow */
	UPROPERTY(EditAnywhere, Category="Input|Squad")
	FKey SquadFollowKey = EKeys::F1;

	/** Key that orders the squad to hold where you stand */
	UPROPERTY(EditAnywhere, Category="Input|Squad")
	FKey SquadHoldKey = EKeys::F2;

	/** Key that orders the squad to charge */
	UPROPERTY(EditAnywhere, Category="Input|Squad")
	FKey SquadChargeKey = EKeys::F3;

	/** Squad order inputs, created at runtime like the castle menu input */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SquadFollowAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SquadHoldAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SquadChargeAction;

	/** Key that switches the squad between melee and ranged */
	UPROPERTY(EditAnywhere, Category="Input|Squad")
	FKey SquadRangedKey = EKeys::F4;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SquadRangedAction;

	/** Keys that take out the weapon in slots 1 to 4 */
	UPROPERTY(EditAnywhere, Category="Input|Weapons")
	TArray<FKey> WeaponSlotKeys = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };

	/** Weapon slot inputs, created at runtime like the castle menu input */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> WeaponSlotActions;

	/** Weapon bar along the bottom of the screen */
	UPROPERTY(Transient)
	TObjectPtr<UWeaponHUDWidget> WeaponHUDWidget;

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

	/** Asks the server to summon the castle's soldiers to follow this player's character */
	UFUNCTION(Server, Reliable)
	void ServerCastleMuster(AGesithCastle* Castle);

	/** Asks the server to send this player's summoned soldiers back to their posts */
	UFUNCTION(Server, Reliable)
	void ServerCastleDismiss(AGesithCastle* Castle);

	/** Asks the server to make this player lord of a castle without one */
	UFUNCTION(Server, Reliable)
	void ServerCastleClaim(AGesithCastle* Castle);

	/** Asks the castle's lord to take this player on as a knight */
	UFUNCTION(Server, Reliable)
	void ServerCastleRequestKnight(AGesithCastle* Castle);

	/** The lord accepts or turns away a player's knight request */
	UFUNCTION(Server, Reliable)
	void ServerCastleAnswerKnightRequest(AGesithCastle* Castle, APlayerState* TargetPlayer, bool bAccept);

	/** The lord dismisses one of their knights */
	UFUNCTION(Server, Reliable)
	void ServerCastleDismissKnight(AGesithCastle* Castle, APlayerState* TargetPlayer);

	/** This player stops serving the castle's lord */
	UFUNCTION(Server, Reliable)
	void ServerCastleLeaveKnighthood(AGesithCastle* Castle);

	/** Asks the server to change when knights may manage this player's castle */
	UFUNCTION(Server, Reliable)
	void ServerCastleSetKnightPermission(AGesithCastle* Castle, EKnightPermission Permission);

	/** Asks the server to give an order to the soldiers following this player's character */
	UFUNCTION(Server, Reliable)
	void ServerSquadOrder(ESquadOrder Order);

	/** Asks the server to switch the squad between melee and ranged */
	UFUNCTION(Server, Reliable)
	void ServerSquadRanged(bool bRanged);

	/** Console command: opens or closes the castle menu, same as the castle menu key */
	UFUNCTION(Exec)
	void CastleMenu();

protected:

	/** Squad order inputs */
	void OrderSquadFollow();
	void OrderSquadHold();
	void OrderSquadCharge();

	void ToggleSquadRanged();

	/** Weapon slot inputs */
	void SelectWeapon1();
	void SelectWeapon2();
	void SelectWeapon3();
	void SelectWeapon4();

	/** Takes out the weapon in a slot on the controlled character */
	void SelectWeapon(int32 Slot);

	/** Shows the order on screen and sends it to the server */
	void GiveSquadOrder(ESquadOrder Order);

	/** Opens the castle menu if the pawn is inside a castle, or closes it */
	void ToggleCastleMenu();

	/** Prints a castle menu message on screen for testing. Does nothing in shipping builds */
	void ShowCastleDebugMessage(const FString& Message) const;

	/** Closes the castle menu and gives the mouse back to the game */
	void CloseCastleMenu();

	/** Returns the castle the pawn is standing in, or nullptr */
	AGesithCastle* FindCastleAtPawn() const;

	/** Returns true if the castle exists and the pawn is inside it */
	bool IsInCastle(const AGesithCastle* Castle) const;

	/** Returns true if the pawn is inside the castle and this player may manage it. Used to check requests on the server */
	bool CanManageCastle(const AGesithCastle* Castle) const;

	/** Returns true if the pawn is inside the castle and this player is its lord */
	bool IsLordInCastle(const AGesithCastle* Castle) const;

protected:

	/** Called if the possessed pawn is destroyed */
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedActor);

};
