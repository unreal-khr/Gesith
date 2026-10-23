// Copyright Epic Games, Inc. All Rights Reserved.


#include "WeaponHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "CastleMenuWidget.h"
#include "CombatCharacter.h"

#define LOCTEXT_NAMESPACE "WeaponHUD"

void UWeaponHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	// crosshair in the middle of the screen
	CrosshairText = UCastleMenuWidget::MakeText(this, LOCTEXT("Crosshair", "+"), 22);
	CrosshairText->SetVisibility(ESlateVisibility::Hidden);

	if (UOverlaySlot* CrosshairSlot = Root->AddChildToOverlay(CrosshairText))
	{
		CrosshairSlot->SetHorizontalAlignment(HAlign_Center);
		CrosshairSlot->SetVerticalAlignment(VAlign_Center);
	}

	// weapon bar along the bottom
	UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Bar->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	Bar->SetPadding(FMargin(12.0f, 6.0f));

	if (UOverlaySlot* BarSlot = Root->AddChildToOverlay(Bar))
	{
		BarSlot->SetHorizontalAlignment(HAlign_Center);
		BarSlot->SetVerticalAlignment(VAlign_Bottom);
		BarSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Bar->SetContent(Row);

	for (int32 Index = 0; Index < ACombatCharacter::WeaponSlotCount; ++Index)
	{
		UTextBlock* SlotText = UCastleMenuWidget::MakeText(this, FText::GetEmpty(), 14);
		SlotTexts.Add(SlotText);

		Row->AddChildToHorizontalBox(SlotText)->SetPadding(FMargin(8.0f, 0.0f));
	}

	ArrowText = UCastleMenuWidget::MakeText(this, FText::GetEmpty(), 14);
	Row->AddChildToHorizontalBox(ArrowText)->SetPadding(FMargin(16.0f, 0.0f, 8.0f, 0.0f));

	StanceText = UCastleMenuWidget::MakeText(this, FText::GetEmpty(), 14);
	Row->AddChildToHorizontalBox(StanceText)->SetPadding(FMargin(16.0f, 0.0f, 8.0f, 0.0f));
}

void UWeaponHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ACombatCharacter* Character = Cast<ACombatCharacter>(GetOwningPlayerPawn());

	// nothing to show while dead or between respawns
	if (!Character || Character->IsDead())
	{
		SetRenderOpacity(0.0f);
		return;
	}

	SetRenderOpacity(1.0f);

	const int32 Current = Character->GetCurrentWeaponSlot();

	for (int32 Index = 0; Index < SlotTexts.Num(); ++Index)
	{
		const EGesithWeaponType Type = Character->GetWeaponInSlot(Index);
		const FText Name = Type == EGesithWeaponType::None ? LOCTEXT("EmptySlot", "-") : Character->GetWeaponStats(Type).DisplayName;
		const bool bInHand = Index == Current;

		// the weapon in hand is bracketed and bright
		SlotTexts[Index]->SetText(FText::Format(bInHand ? LOCTEXT("SlotInHand", "[{0} {1}]") : LOCTEXT("Slot", "{0} {1}"), FText::AsNumber(Index + 1), Name));
		SlotTexts[Index]->SetColorAndOpacity(FSlateColor(bInHand ? FLinearColor(1.0f, 0.85f, 0.3f) : FLinearColor(0.7f, 0.7f, 0.7f)));
	}

	ArrowText->SetText(FText::Format(LOCTEXT("Arrows", "화살 {0}/{1}"), FText::AsNumber(Character->GetArrows()), FText::AsNumber(Character->GetMaxArrows())));

	StanceText->SetText(Character->IsSquadRanged() ? LOCTEXT("StanceRanged", "부대: 원거리 (F4)") : LOCTEXT("StanceMelee", "부대: 근접 (F4)"));

	// the crosshair turns red at full draw
	const bool bBow = Character->GetCurrentWeaponStats().bRanged;
	CrosshairText->SetVisibility(bBow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	CrosshairText->SetColorAndOpacity(FSlateColor(Character->GetDrawFraction() >= 1.0f ? FLinearColor(1.0f, 0.3f, 0.2f) : FLinearColor::White));
}

#undef LOCTEXT_NAMESPACE
