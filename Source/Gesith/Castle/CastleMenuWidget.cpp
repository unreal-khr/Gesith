// Copyright Epic Games, Inc. All Rights Reserved.


#include "CastleMenuWidget.h"
#include "GesithCastle.h"
#include "CombatPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#define LOCTEXT_NAMESPACE "CastleMenu"

void UCastleSoldierRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	WidgetTree->RootWidget = RowBox;

	InfoText = UCastleMenuWidget::MakeText(this, FText::GetEmpty());

	if (UHorizontalBoxSlot* InfoSlot = RowBox->AddChildToHorizontalBox(InfoText))
	{
		InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		InfoSlot->SetVerticalAlignment(VAlign_Center);
	}

	UTextBlock* PromoteLabel = nullptr;

	UTextBlock* TrainLabel = nullptr;

	TrainButton = UCastleMenuWidget::MakeButton(this, FText::GetEmpty(), TrainLabel);
	TrainButtonText = TrainLabel;
	PromoteButton = UCastleMenuWidget::MakeButton(this, LOCTEXT("Promote", "승급"), PromoteLabel);

	RowBox->AddChildToHorizontalBox(TrainButton)->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	RowBox->AddChildToHorizontalBox(PromoteButton)->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));

	TrainButton->OnClicked.AddDynamic(this, &UCastleSoldierRow::HandleTrainClicked);
	PromoteButton->OnClicked.AddDynamic(this, &UCastleSoldierRow::HandlePromoteClicked);
}

void UCastleSoldierRow::Setup(UCastleMenuWidget* InMenu, const FSoldierRecord& Soldier, bool bCanPromote, float NextRankThreshold)
{
	Menu = InMenu;
	SoldierId = Soldier.Id;
	bTraining = Soldier.bTraining;

	const FText Promotion = NextRankThreshold > 0.0f
		? FText::Format(LOCTEXT("NextRank", "승급 기준 {0}"), FText::AsNumber(FMath::RoundToInt(NextRankThreshold)))
		: LOCTEXT("TopRank", "최고 등급");

	FNumberFormattingOptions OneDecimal;
	OneDecimal.MinimumFractionalDigits = 1;
	OneDecimal.MaximumFractionalDigits = 1;

	InfoText->SetText(FText::Format(
		LOCTEXT("SoldierInfo", "{0}  [{1}]   체력 {2}  힘 {3}  지구력 {4}  공속 {5}  숙련 {6}   평균 {7} / {8}"),
		FText::FromString(Soldier.Name),
		AGesithCastle::GetRankName(Soldier.Rank),
		FText::AsNumber(Soldier.Health, &OneDecimal),
		FText::AsNumber(Soldier.Strength, &OneDecimal),
		FText::AsNumber(Soldier.Endurance, &OneDecimal),
		FText::AsNumber(Soldier.AttackSpeed, &OneDecimal),
		FText::AsNumber(Soldier.WeaponSkill, &OneDecimal),
		FText::AsNumber(Soldier.GetAverageStat(), &OneDecimal),
		Promotion));

	TrainButtonText->SetText(bTraining ? LOCTEXT("StopTraining", "훈련 중 (멈추기)") : LOCTEXT("StartTraining", "훈련 보내기"));

	PromoteButton->SetIsEnabled(bCanPromote);
}

void UCastleSoldierRow::HandleTrainClicked()
{
	if (Menu.IsValid())
	{
		Menu->RequestSetTraining(SoldierId, !bTraining);
	}
}

void UCastleSoldierRow::HandlePromoteClicked()
{
	if (Menu.IsValid())
	{
		Menu->RequestPromote(SoldierId);
	}
}

UTextBlock* UCastleMenuWidget::MakeText(UUserWidget* Owner, const FText& Text, int32 FontSize)
{
	UTextBlock* TextBlock = Owner->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TextBlock->SetText(Text);

	FSlateFontInfo Font = TextBlock->GetFont();
	Font.Size = FontSize;
	TextBlock->SetFont(Font);

	return TextBlock;
}

UButton* UCastleMenuWidget::MakeButton(UUserWidget* Owner, const FText& Label, UTextBlock*& OutLabel)
{
	UButton* Button = Owner->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());

	// button labels are dark so they read on the default light button style
	OutLabel = MakeText(Owner, Label, 13);
	OutLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f)));

	Button->AddChild(OutLabel);

	return Button;
}

void UCastleMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// a transparent full screen border keeps the panel in the top left corner
	UBorder* ScreenBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ScreenBorder->SetBrushColor(FLinearColor::Transparent);
	ScreenBorder->SetPadding(FMargin(40.0f));
	ScreenBorder->SetHorizontalAlignment(HAlign_Left);
	ScreenBorder->SetVerticalAlignment(VAlign_Top);
	WidgetTree->RootWidget = ScreenBorder;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	Panel->SetPadding(FMargin(16.0f));
	ScreenBorder->SetContent(Panel);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Content);

	TitleText = MakeText(this, FText::GetEmpty(), 20);
	ResourcesText = MakeText(this, FText::GetEmpty());
	WarningText = MakeText(this, FText::GetEmpty());
	WarningText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.35f, 0.25f)));

	Content->AddChildToVerticalBox(TitleText);
	Content->AddChildToVerticalBox(ResourcesText)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	Content->AddChildToVerticalBox(WarningText);

	UTextBlock* RecruitLabel = nullptr;

	RecruitButton = MakeButton(this, FText::GetEmpty(), RecruitLabel);
	RecruitButtonText = RecruitLabel;
	RecruitButton->OnClicked.AddDynamic(this, &UCastleMenuWidget::HandleRecruitClicked);

	if (UVerticalBoxSlot* RecruitSlot = Content->AddChildToVerticalBox(RecruitButton))
	{
		RecruitSlot->SetPadding(FMargin(0.0f, 10.0f));
		RecruitSlot->SetHorizontalAlignment(HAlign_Left);
	}

	// the roster scrolls once it gets long
	USizeBox* RosterSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RosterSize->SetMaxDesiredHeight(500.0f);
	Content->AddChildToVerticalBox(RosterSize);

	UScrollBox* RosterScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	RosterSize->SetContent(RosterScroll);

	RosterBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	RosterScroll->AddChild(RosterBox);

	UTextBlock* HintText = MakeText(this, LOCTEXT("Hint", "Tab 키로 닫기"), 12);
	HintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	Content->AddChildToVerticalBox(HintText)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
}

void UCastleMenuWidget::NativeDestruct()
{
	if (Castle.IsValid())
	{
		Castle->OnCastleChanged.Remove(CastleChangedHandle);
	}

	Super::NativeDestruct();
}

void UCastleMenuWidget::SetCastle(AGesithCastle* InCastle)
{
	if (Castle.IsValid())
	{
		Castle->OnCastleChanged.Remove(CastleChangedHandle);
	}

	Castle = InCastle;

	if (Castle.IsValid())
	{
		CastleChangedHandle = Castle->OnCastleChanged.AddUObject(this, &UCastleMenuWidget::Refresh);
	}

	Refresh();
}

void UCastleMenuWidget::Refresh()
{
	if (!Castle.IsValid())
	{
		return;
	}

	TitleText->SetText(FText::Format(LOCTEXT("Title", "{0}  -  {1}일째"), FText::FromString(Castle->CastleName), FText::AsNumber(Castle->Day)));

	ResourcesText->SetText(FText::Format(
		LOCTEXT("Resources", "금 {0}   식량 {1}   병사 {2}명   하루 급여 금 {3}"),
		FText::AsNumber(Castle->Gold),
		FText::AsNumber(Castle->Food),
		FText::AsNumber(Castle->Roster.Num()),
		FText::AsNumber(Castle->GetDailyWages())));

	// warn about unpaid wages and hunger, since they lead to desertion
	TArray<FText> Warnings;

	if (Castle->UnpaidGoldDays > 0)
	{
		Warnings.Add(FText::Format(LOCTEXT("UnpaidGold", "급여 {0}일 밀림, 훈련 중단"), FText::AsNumber(Castle->UnpaidGoldDays)));
	}

	if (Castle->UnpaidFoodDays > 0)
	{
		Warnings.Add(FText::Format(LOCTEXT("UnpaidFood", "식량 {0}일 부족"), FText::AsNumber(Castle->UnpaidFoodDays)));
	}

	WarningText->SetText(FText::Join(FText::FromString(TEXT("   ")), Warnings));
	WarningText->SetVisibility(Warnings.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	RecruitButtonText->SetText(FText::Format(LOCTEXT("Recruit", "병사 모집 (금 {0})"), FText::AsNumber(Castle->GetRecruitCost())));
	RecruitButton->SetIsEnabled(Castle->Gold >= Castle->GetRecruitCost());

	// rebuild the roster rows
	RosterBox->ClearChildren();

	for (const FSoldierRecord& Soldier : Castle->Roster)
	{
		UCastleSoldierRow* Row = CreateWidget<UCastleSoldierRow>(this, UCastleSoldierRow::StaticClass());

		if (Row)
		{
			Row->Setup(this, Soldier, Castle->CanPromote(Soldier), Castle->GetNextRankThreshold(Soldier));
			RosterBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 2.0f));
		}
	}
}

void UCastleMenuWidget::HandleRecruitClicked()
{
	if (ACombatPlayerController* PC = GetOwningPlayer<ACombatPlayerController>())
	{
		PC->ServerCastleRecruit(Castle.Get());
	}
}

void UCastleMenuWidget::RequestSetTraining(int32 SoldierId, bool bTraining)
{
	if (ACombatPlayerController* PC = GetOwningPlayer<ACombatPlayerController>())
	{
		PC->ServerCastleSetTraining(Castle.Get(), SoldierId, bTraining);
	}
}

void UCastleMenuWidget::RequestPromote(int32 SoldierId)
{
	if (ACombatPlayerController* PC = GetOwningPlayer<ACombatPlayerController>())
	{
		PC->ServerCastlePromote(Castle.Get(), SoldierId);
	}
}

#undef LOCTEXT_NAMESPACE
