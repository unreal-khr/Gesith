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
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

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

void UCastleSoldierRow::Setup(UCastleMenuWidget* InMenu, const FSoldierRecord& Soldier, bool bCanPromote, float NextRankThreshold, bool bCanManage)
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

	// summoned soldiers say so after their name
	const FText Status = Soldier.bInField ? LOCTEXT("InField", " (소집 중)") : FText::GetEmpty();

	InfoText->SetText(FText::Format(
		LOCTEXT("SoldierInfo", "{0}{9}  [{1}]   체력 {2}  힘 {3}  지구력 {4}  공속 {5}  숙련 {6}   평균 {7} / {8}"),
		FText::FromString(Soldier.Name),
		AGesithCastle::GetRankName(Soldier.Rank),
		FText::AsNumber(Soldier.Health, &OneDecimal),
		FText::AsNumber(Soldier.Strength, &OneDecimal),
		FText::AsNumber(Soldier.Endurance, &OneDecimal),
		FText::AsNumber(Soldier.AttackSpeed, &OneDecimal),
		FText::AsNumber(Soldier.WeaponSkill, &OneDecimal),
		FText::AsNumber(Soldier.GetAverageStat(), &OneDecimal),
		Promotion,
		Status));

	TrainButton->SetIsEnabled(bCanManage && (Soldier.IsAvailable() || bTraining));

	TrainButtonText->SetText(bTraining ? LOCTEXT("StopTraining", "훈련 중 (멈추기)") : LOCTEXT("StartTraining", "훈련 보내기"));

	PromoteButton->SetIsEnabled(bCanManage && bCanPromote);
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

void UCastlePlayerRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	WidgetTree->RootWidget = RowBox;

	NameText = UCastleMenuWidget::MakeText(this, FText::GetEmpty());

	if (UHorizontalBoxSlot* NameSlot = RowBox->AddChildToHorizontalBox(NameText))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);
	}

	UTextBlock* KnightLabel = nullptr;

	UButton* KnightButton = UCastleMenuWidget::MakeButton(this, FText::GetEmpty(), KnightLabel);
	KnightButtonText = KnightLabel;
	KnightButton->OnClicked.AddDynamic(this, &UCastlePlayerRow::HandleKnightClicked);

	RowBox->AddChildToHorizontalBox(KnightButton)->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
}

void UCastlePlayerRow::Setup(UCastleMenuWidget* InMenu, APlayerState* InPlayer, bool bIsKnight)
{
	Menu = InMenu;
	RowPlayer = InPlayer;
	bKnight = bIsKnight;

	NameText->SetText(bKnight
		? FText::Format(LOCTEXT("KnightName", "{0}  [기사]"), FText::FromString(InPlayer->GetPlayerName()))
		: FText::FromString(InPlayer->GetPlayerName()));

	KnightButtonText->SetText(bKnight ? LOCTEXT("RevokeKnight", "기사 해임") : LOCTEXT("GrantKnight", "기사 임명"));
}

void UCastlePlayerRow::HandleKnightClicked()
{
	if (Menu.IsValid() && RowPlayer.IsValid())
	{
		Menu->RequestSetKnight(RowPlayer.Get(), !bKnight);
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

	LordText = MakeText(this, FText::GetEmpty());
	AccessText = MakeText(this, FText::GetEmpty(), 13);

	Content->AddChildToVerticalBox(TitleText);
	Content->AddChildToVerticalBox(LordText)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	Content->AddChildToVerticalBox(AccessText);

	// anyone can claim a castle without a lord
	UTextBlock* ClaimLabel = nullptr;

	ClaimButton = MakeButton(this, LOCTEXT("Claim", "이 성의 영주 되기"), ClaimLabel);
	ClaimButton->OnClicked.AddDynamic(this, &UCastleMenuWidget::HandleClaimClicked);

	if (UVerticalBoxSlot* ClaimSlot = Content->AddChildToVerticalBox(ClaimButton))
	{
		ClaimSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		ClaimSlot->SetHorizontalAlignment(HAlign_Left);
	}

	// the lord picks when knights may manage the castle, and who the knights are
	LordPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Content->AddChildToVerticalBox(LordPanel)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));

	LordPanel->AddChildToVerticalBox(MakeText(this, LOCTEXT("PermissionHeader", "기사의 성 관리 권한"), 13));

	UHorizontalBox* PermissionBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	LordPanel->AddChildToVerticalBox(PermissionBox)->SetPadding(FMargin(0.0f, 2.0f));

	const FText PermissionLabels[] = {
		LOCTEXT("PermissionAlways", "항상 가능"),
		LOCTEXT("PermissionLordAway", "영주 부재 시만"),
		LOCTEXT("PermissionNever", "불가")
	};

	for (const FText& Label : PermissionLabels)
	{
		UTextBlock* PermissionLabel = nullptr;
		UButton* PermissionButton = MakeButton(this, Label, PermissionLabel);

		PermissionBox->AddChildToHorizontalBox(PermissionButton)->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
		PermissionButtons.Add(PermissionButton);
	}

	PermissionButtons[0]->OnClicked.AddDynamic(this, &UCastleMenuWidget::HandlePermissionAlwaysClicked);
	PermissionButtons[1]->OnClicked.AddDynamic(this, &UCastleMenuWidget::HandlePermissionLordAwayClicked);
	PermissionButtons[2]->OnClicked.AddDynamic(this, &UCastleMenuWidget::HandlePermissionNeverClicked);

	LordPanel->AddChildToVerticalBox(MakeText(this, LOCTEXT("KnightHeader", "기사 임명"), 13))->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));

	KnightBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	LordPanel->AddChildToVerticalBox(KnightBox);

	Content->AddChildToVerticalBox(ResourcesText)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
	Content->AddChildToVerticalBox(WarningText);

	UTextBlock* RecruitLabel = nullptr;

	RecruitButton = MakeButton(this, FText::GetEmpty(), RecruitLabel);
	RecruitButtonText = RecruitLabel;
	RecruitButton->OnClicked.AddDynamic(this, &UCastleMenuWidget::HandleRecruitClicked);

	UTextBlock* MusterLabel = nullptr;

	MusterButton = MakeButton(this, FText::GetEmpty(), MusterLabel);
	MusterButtonText = MusterLabel;
	MusterButton->OnClicked.AddDynamic(this, &UCastleMenuWidget::HandleMusterClicked);

	UTextBlock* DismissLabel = nullptr;

	DismissButton = MakeButton(this, LOCTEXT("Dismiss", "소집 해제"), DismissLabel);
	DismissButton->OnClicked.AddDynamic(this, &UCastleMenuWidget::HandleDismissClicked);

	// recruiting, summoning and dismissing sit side by side
	UHorizontalBox* ActionBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ActionBox->AddChildToHorizontalBox(RecruitButton);
	ActionBox->AddChildToHorizontalBox(MusterButton)->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	ActionBox->AddChildToHorizontalBox(DismissButton)->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));

	if (UVerticalBoxSlot* ActionSlot = Content->AddChildToVerticalBox(ActionBox))
	{
		ActionSlot->SetPadding(FMargin(0.0f, 10.0f));
		ActionSlot->SetHorizontalAlignment(HAlign_Left);
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

	// who rules here, and what this player may do
	APlayerState* Me = GetOwningPlayerState();
	const bool bIsLord = Me && Castle->Lord == Me;
	const bool bCanManage = Castle->CanManage(Me);

	const FText LordName = Castle->Lord ? FText::FromString(Castle->Lord->GetPlayerName()) : LOCTEXT("NoLord", "없음");
	const FText MyRole = bIsLord ? LOCTEXT("RoleLord", "영주") : Castle->IsKnight(Me) ? LOCTEXT("RoleKnight", "기사") : LOCTEXT("RoleVisitor", "방문자");

	LordText->SetText(FText::Format(LOCTEXT("LordInfo", "영주: {0}   내 역할: {1}"), LordName, MyRole));

	FText Access;

	if (!Castle->Lord)
	{
		Access = LOCTEXT("AccessNoLord", "영주가 없는 성이에요. 영주가 되면 관리할 수 있어요.");
	}
	else if (bCanManage)
	{
		Access = LOCTEXT("AccessYes", "이 성을 관리할 수 있어요.");
	}
	else if (Castle->IsKnight(Me) && Castle->KnightPermission == EKnightPermission::LordAway)
	{
		Access = LOCTEXT("AccessLordHome", "영주가 성에 있어서 지금은 관리할 수 없어요.");
	}
	else
	{
		Access = LOCTEXT("AccessNo", "이 성을 관리할 권한이 없어요.");
	}

	AccessText->SetText(Access);
	AccessText->SetColorAndOpacity(FSlateColor(bCanManage ? FLinearColor(0.5f, 1.0f, 0.5f) : FLinearColor(1.0f, 0.8f, 0.4f)));

	ClaimButton->SetVisibility(Castle->Lord ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	LordPanel->SetVisibility(bIsLord ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	// the current permission is shown as the one button that can't be pressed
	for (int32 Index = 0; Index < PermissionButtons.Num(); ++Index)
	{
		PermissionButtons[Index]->SetIsEnabled(static_cast<int32>(Castle->KnightPermission) != Index);
	}

	RefreshKnights();

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
	RecruitButton->SetIsEnabled(bCanManage && Castle->Gold >= Castle->GetRecruitCost());

	const int32 Available = Castle->GetAvailableCount();

	MusterButtonText->SetText(FText::Format(LOCTEXT("Muster", "병사 소집 ({0}명)"), FText::AsNumber(Available)));
	MusterButton->SetIsEnabled(bCanManage && Available > 0);
	DismissButton->SetIsEnabled(Castle->HasSoldiersInField());

	// rebuild the roster rows
	RosterBox->ClearChildren();

	for (const FSoldierRecord& Soldier : Castle->Roster)
	{
		UCastleSoldierRow* Row = CreateWidget<UCastleSoldierRow>(this, UCastleSoldierRow::StaticClass());

		if (Row)
		{
			Row->Setup(this, Soldier, Castle->CanPromote(Soldier), Castle->GetNextRankThreshold(Soldier), bCanManage);
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

void UCastleMenuWidget::HandleMusterClicked()
{
	if (ACombatPlayerController* PC = GetOwningPlayer<ACombatPlayerController>())
	{
		PC->ServerCastleMuster(Castle.Get());
	}
}

void UCastleMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;

	if (GameState && GameState->PlayerArray.Num() != LastPlayerCount)
	{
		RefreshKnights();
	}
}

void UCastleMenuWidget::RefreshKnights()
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;

	if (!Castle.IsValid() || !GameState)
	{
		return;
	}

	LastPlayerCount = GameState->PlayerArray.Num();

	KnightBox->ClearChildren();

	// every other player can be named a knight
	for (APlayerState* Player : GameState->PlayerArray)
	{
		if (!Player || Player == Castle->Lord)
		{
			continue;
		}

		if (UCastlePlayerRow* Row = CreateWidget<UCastlePlayerRow>(this, UCastlePlayerRow::StaticClass()))
		{
			Row->Setup(this, Player, Castle->IsKnight(Player));
			KnightBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 2.0f));
		}
	}

	if (KnightBox->GetChildrenCount() == 0)
	{
		KnightBox->AddChildToVerticalBox(MakeText(this, LOCTEXT("NoPlayers", "접속한 다른 플레이어가 없어요."), 12));
	}
}

void UCastleMenuWidget::HandleClaimClicked()
{
	if (ACombatPlayerController* PC = GetOwningPlayer<ACombatPlayerController>())
	{
		PC->ServerCastleClaim(Castle.Get());
	}
}

void UCastleMenuWidget::HandlePermissionAlwaysClicked()
{
	RequestKnightPermission(EKnightPermission::Always);
}

void UCastleMenuWidget::HandlePermissionLordAwayClicked()
{
	RequestKnightPermission(EKnightPermission::LordAway);
}

void UCastleMenuWidget::HandlePermissionNeverClicked()
{
	RequestKnightPermission(EKnightPermission::Never);
}

void UCastleMenuWidget::RequestKnightPermission(EKnightPermission Permission)
{
	if (ACombatPlayerController* PC = GetOwningPlayer<ACombatPlayerController>())
	{
		PC->ServerCastleSetKnightPermission(Castle.Get(), Permission);
	}
}

void UCastleMenuWidget::RequestSetKnight(APlayerState* TargetPlayer, bool bKnight)
{
	if (ACombatPlayerController* PC = GetOwningPlayer<ACombatPlayerController>())
	{
		PC->ServerCastleSetKnight(Castle.Get(), TargetPlayer, bKnight);
	}
}

void UCastleMenuWidget::HandleDismissClicked()
{
	if (ACombatPlayerController* PC = GetOwningPlayer<ACombatPlayerController>())
	{
		PC->ServerCastleDismiss(Castle.Get());
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
