// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WeaponHUDWidget.generated.h"

class UTextBlock;

/**
 *  Weapon bar along the bottom of the screen: the 1-4 slots, the one in hand and the arrows left.
 *  Shows a crosshair while a bow is in hand.
 *  Built entirely in C++ so it needs no widget assets. Reads the local player's character every frame.
 */
UCLASS()
class UWeaponHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:

	/** One label per weapon slot */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SlotTexts;

	/** Arrows left */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ArrowText;

	/** Squad melee or ranged stance */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StanceText;

	/** Center of the screen marker while holding a bow */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CrosshairText;

protected:

	/** Builds the widget tree */
	virtual void NativeOnInitialized() override;

	/** Updates the labels from the character */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};
