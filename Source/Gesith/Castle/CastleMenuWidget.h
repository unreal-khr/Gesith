// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GesithSoldierTypes.h"
#include "CastleMenuWidget.generated.h"

class AGesithCastle;
class UButton;
class UTextBlock;
class UVerticalBox;
class UCastleMenuWidget;
class APlayerState;

/**
 *  One soldier's line in the castle menu: name, rank and stats, with train and promote buttons.
 *  Built entirely in C++, so it needs no widget Blueprint.
 */
UCLASS()
class UCastleSoldierRow : public UUserWidget
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UTextBlock> InfoText;

	UPROPERTY()
	TObjectPtr<UButton> TrainButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> TrainButtonText;

	UPROPERTY()
	TObjectPtr<UButton> PromoteButton;

	/** Menu that owns this row */
	TWeakObjectPtr<UCastleMenuWidget> Menu;

	/** Soldier shown in this row */
	int32 SoldierId = 0;

	/** True if the soldier is currently training */
	bool bTraining = false;

public:

	/** Fills the row with a soldier's data. The buttons only work for players who may manage the castle */
	void Setup(UCastleMenuWidget* InMenu, const FSoldierRecord& Soldier, bool bCanPromote, float NextRankThreshold, bool bCanManage);

protected:

	/** Builds the row's widgets */
	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleTrainClicked();

	UFUNCTION()
	void HandlePromoteClicked();
};

/**
 *  One player's line in the lord's knight list, with a button to name or dismiss them as a knight.
 *  Built entirely in C++, so it needs no widget Blueprint.
 */
UCLASS()
class UCastlePlayerRow : public UUserWidget
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> KnightButtonText;

	/** Menu that owns this row */
	TWeakObjectPtr<UCastleMenuWidget> Menu;

	/** Player shown in this row */
	TWeakObjectPtr<APlayerState> Player;

	/** True if the player is currently a knight */
	bool bKnight = false;

public:

	/** Fills the row with a player */
	void Setup(UCastleMenuWidget* InMenu, APlayerState* InPlayer, bool bIsKnight);

protected:

	/** Builds the row's widgets */
	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleKnightClicked();
};

/**
 *  The castle management menu: treasury, wages, recruiting and the soldier roster.
 *  Built entirely in C++, so it needs no widget Blueprint. Opened by the player controller inside a castle.
 */
UCLASS()
class UCastleMenuWidget : public UUserWidget
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> LordText;

	UPROPERTY()
	TObjectPtr<UTextBlock> AccessText;

	UPROPERTY()
	TObjectPtr<UButton> ClaimButton;

	/** Settings only the lord sees: knight permission and the knight list */
	UPROPERTY()
	TObjectPtr<UVerticalBox> LordPanel;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> PermissionButtons;

	UPROPERTY()
	TObjectPtr<UVerticalBox> KnightBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> ResourcesText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WarningText;

	UPROPERTY()
	TObjectPtr<UButton> RecruitButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> RecruitButtonText;

	UPROPERTY()
	TObjectPtr<UButton> MusterButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> MusterButtonText;

	UPROPERTY()
	TObjectPtr<UButton> DismissButton;

	UPROPERTY()
	TObjectPtr<UVerticalBox> RosterBox;

	/** Castle shown in the menu */
	TWeakObjectPtr<AGesithCastle> Castle;

	/** Handle for the castle changed listener */
	FDelegateHandle CastleChangedHandle;

	/** Players in the game when the knight list was last built, to notice joins and leaves */
	int32 LastPlayerCount = -1;

public:

	/** Shows the provided castle and refreshes whenever it changes */
	void SetCastle(AGesithCastle* InCastle);

	/** Asks the server to start or stop a soldier's training */
	void RequestSetTraining(int32 SoldierId, bool bTraining);

	/** Asks the server to promote a soldier */
	void RequestPromote(int32 SoldierId);

	/** Asks the server to name a player a knight, or dismiss them */
	void RequestSetKnight(APlayerState* Player, bool bKnight);

	/** Creates a text block with the menu's font size */
	static UTextBlock* MakeText(UUserWidget* Owner, const FText& Text, int32 FontSize = 14);

	/** Creates a button with a text label */
	static UButton* MakeButton(UUserWidget* Owner, const FText& Label, UTextBlock*& OutLabel);

protected:

	/** Builds the menu's widgets */
	virtual void NativeOnInitialized() override;

	/** Stops listening to the castle */
	virtual void NativeDestruct() override;

	/** Rebuilds the knight list when players join or leave */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Rebuilds the lord's knight list */
	void RefreshKnights();

	/** Asks the server to change the knight permission */
	void RequestKnightPermission(EKnightPermission Permission);

	/** Updates every value shown in the menu */
	void Refresh();

	UFUNCTION()
	void HandleRecruitClicked();

	UFUNCTION()
	void HandleMusterClicked();

	UFUNCTION()
	void HandleDismissClicked();

	UFUNCTION()
	void HandleClaimClicked();

	UFUNCTION()
	void HandlePermissionAlwaysClicked();

	UFUNCTION()
	void HandlePermissionLordAwayClicked();

	UFUNCTION()
	void HandlePermissionNeverClicked();
};
