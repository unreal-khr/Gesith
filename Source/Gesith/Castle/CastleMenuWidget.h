// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CastleMenuWidget.generated.h"

class AGesithCastle;
class UButton;
class UTextBlock;
class UVerticalBox;
class UCastleMenuWidget;
struct FSoldierRecord;

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

	/** Fills the row with a soldier's data */
	void Setup(UCastleMenuWidget* InMenu, const FSoldierRecord& Soldier, bool bCanPromote, float NextRankThreshold);

protected:

	/** Builds the row's widgets */
	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleTrainClicked();

	UFUNCTION()
	void HandlePromoteClicked();
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

public:

	/** Shows the provided castle and refreshes whenever it changes */
	void SetCastle(AGesithCastle* InCastle);

	/** Asks the server to start or stop a soldier's training */
	void RequestSetTraining(int32 SoldierId, bool bTraining);

	/** Asks the server to promote a soldier */
	void RequestPromote(int32 SoldierId);

	/** Creates a text block with the menu's font size */
	static UTextBlock* MakeText(UUserWidget* Owner, const FText& Text, int32 FontSize = 14);

	/** Creates a button with a text label */
	static UButton* MakeButton(UUserWidget* Owner, const FText& Label, UTextBlock*& OutLabel);

protected:

	/** Builds the menu's widgets */
	virtual void NativeOnInitialized() override;

	/** Stops listening to the castle */
	virtual void NativeDestruct() override;

	/** Updates every value shown in the menu */
	void Refresh();

	UFUNCTION()
	void HandleRecruitClicked();

	UFUNCTION()
	void HandleMusterClicked();

	UFUNCTION()
	void HandleDismissClicked();
};
