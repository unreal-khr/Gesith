// Copyright Epic Games, Inc. All Rights Reserved.


#include "GesithCastle.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Gesith.h"

AGesithCastle::AGesithCastle()
{
	PrimaryActorTick.bCanEverTick = true;

	// every player needs the castle's roster, wherever they are
	bReplicates = true;
	bAlwaysRelevant = true;

	CastleArea = CreateDefaultSubobject<UBoxComponent>(TEXT("CastleArea"));
	CastleArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CastleArea->SetCanEverAffectNavigation(false);
	RootComponent = CastleArea;

	// grey box walls made from the engine cube
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CubeMesh = CubeMeshFinder.Object;

	auto MakeWall = [this, CubeMesh](const TCHAR* Name)
	{
		UStaticMeshComponent* Wall = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Wall->SetupAttachment(RootComponent);
		Wall->SetStaticMesh(CubeMesh);
		return Wall;
	};

	BackWall = MakeWall(TEXT("BackWall"));
	LeftWall = MakeWall(TEXT("LeftWall"));
	RightWall = MakeWall(TEXT("RightWall"));
	FrontLeftWall = MakeWall(TEXT("FrontLeftWall"));
	FrontRightWall = MakeWall(TEXT("FrontRightWall"));

	// wages from recruit to elite
	WageByRank = { 2, 3, 4, 6, 8 };

	// average stat needed to reach private, regular, veteran and elite
	PromotionThresholds = { 20.0f, 35.0f, 50.0f, 70.0f };
}

void AGesithCastle::PlaceWall(UStaticMeshComponent* Wall, const FVector& Center, const FVector& Size)
{
	// the engine cube is 100cm on each side and centered on its origin
	Wall->SetRelativeLocation(Center);
	Wall->SetRelativeScale3D(Size / 100.0f);
}

void AGesithCastle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	const float Half = CastleSize * 0.5f;
	const float WallZ = WallHeight * 0.5f;

	// the castle area covers the inside of the walls, from the ground up
	CastleArea->SetBoxExtent(FVector(Half, Half, WallHeight));

	// the front faces the actor's forward direction, with the gate in the middle
	PlaceWall(BackWall, FVector(-Half, 0.0f, WallZ), FVector(WallThickness, CastleSize, WallHeight));
	PlaceWall(LeftWall, FVector(0.0f, -Half, WallZ), FVector(CastleSize, WallThickness, WallHeight));
	PlaceWall(RightWall, FVector(0.0f, Half, WallZ), FVector(CastleSize, WallThickness, WallHeight));

	const float SideLength = FMath::Max((CastleSize - GateWidth) * 0.5f, 0.0f);
	const float SideCenter = (GateWidth + SideLength) * 0.5f;

	PlaceWall(FrontLeftWall, FVector(Half, -SideCenter, WallZ), FVector(WallThickness, SideLength, WallHeight));
	PlaceWall(FrontRightWall, FVector(Half, SideCenter, WallZ), FVector(WallThickness, SideLength, WallHeight));
}

void AGesithCastle::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Gold = StartingGold;
		Food = StartingFood;

		NotifyChanged();
	}
}

void AGesithCastle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// the economy only runs on the server
	if (!HasAuthority())
	{
		return;
	}

	TrainingTimer += DeltaSeconds;

	if (TrainingTimer >= TrainingInterval)
	{
		TrainingTimer -= TrainingInterval;

		RunTrainingSession();
	}

	DayTimer += DeltaSeconds;

	if (DayTimer >= DayLength)
	{
		DayTimer -= DayLength;

		AdvanceDay();
	}
}

bool AGesithCastle::IsInsideCastle(const FVector& Location) const
{
	const FVector Local = CastleArea->GetComponentTransform().InverseTransformPosition(Location);
	const FVector Extent = CastleArea->GetUnscaledBoxExtent();

	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

bool AGesithCastle::Recruit()
{
	if (!HasAuthority() || Gold < RecruitCost)
	{
		return false;
	}

	// names are placeholders until soldiers get proper name lists
	static const TCHAR* Names[] = {
		TEXT("Aldric"), TEXT("Bertram"), TEXT("Cedric"), TEXT("Dunstan"), TEXT("Edmund"), TEXT("Godric"),
		TEXT("Hugh"), TEXT("Ivo"), TEXT("Leofric"), TEXT("Osbert"), TEXT("Ranulf"), TEXT("Siward"),
		TEXT("Thurstan"), TEXT("Walter"), TEXT("Wulfric"), TEXT("Alaric"), TEXT("Baldwin"), TEXT("Gilbert")
	};

	Gold -= RecruitCost;

	FSoldierRecord Soldier;
	Soldier.Id = NextSoldierId++;
	Soldier.Name = Names[FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Names)) - 1)];

	// recruits start a little different from each other
	for (int32 StatIndex = 0; StatIndex < 5; ++StatIndex)
	{
		*Soldier.GetStat(StatIndex) = FMath::RandRange(7.0f, 13.0f);
	}

	Roster.Add(Soldier);

	NotifyChanged();

	return true;
}

void AGesithCastle::SetTraining(int32 SoldierId, bool bTraining)
{
	if (!HasAuthority())
	{
		return;
	}

	FSoldierRecord* Soldier = FindSoldier(SoldierId);

	// soldiers out in the field or recovering can't train
	if (Soldier && (Soldier->IsAvailable() || !bTraining))
	{
		Soldier->bTraining = bTraining;

		NotifyChanged();
	}
}

bool AGesithCastle::Promote(int32 SoldierId)
{
	if (!HasAuthority())
	{
		return false;
	}

	FSoldierRecord* Soldier = FindSoldier(SoldierId);

	if (!Soldier || !CanPromote(*Soldier))
	{
		return false;
	}

	// promotion costs nothing, the higher rank only raises the wage
	Soldier->Rank = static_cast<ESoldierRank>(static_cast<uint8>(Soldier->Rank) + 1);

	NotifyChanged();

	return true;
}

bool AGesithCastle::CanPromote(const FSoldierRecord& Soldier) const
{
	const float Threshold = GetNextRankThreshold(Soldier);

	return Threshold > 0.0f && Soldier.GetAverageStat() >= Threshold;
}

float AGesithCastle::GetNextRankThreshold(const FSoldierRecord& Soldier) const
{
	const int32 RankIndex = static_cast<int32>(Soldier.Rank);

	return PromotionThresholds.IsValidIndex(RankIndex) ? PromotionThresholds[RankIndex] : 0.0f;
}

int32 AGesithCastle::GetDailyWages() const
{
	int32 Total = 0;

	for (const FSoldierRecord& Soldier : Roster)
	{
		const int32 RankIndex = static_cast<int32>(Soldier.Rank);

		Total += WageByRank.IsValidIndex(RankIndex) ? WageByRank[RankIndex] : 0;
	}

	return Total;
}

int32 AGesithCastle::GetAvailableCount() const
{
	int32 Count = 0;

	for (const FSoldierRecord& Soldier : Roster)
	{
		Count += Soldier.IsAvailable() ? 1 : 0;
	}

	return Count;
}

TArray<FSoldierRecord> AGesithCastle::MusterSoldiers()
{
	TArray<FSoldierRecord> Mustered;

	if (!HasAuthority())
	{
		return Mustered;
	}

	for (FSoldierRecord& Soldier : Roster)
	{
		if (Soldier.IsAvailable())
		{
			Soldier.bInField = true;
			Soldier.bTraining = false;

			Mustered.Add(Soldier);
		}
	}

	if (Mustered.Num() > 0)
	{
		NotifyChanged();
	}

	return Mustered;
}

void AGesithCastle::SoldierReturned(int32 SoldierId, bool bFought)
{
	FSoldierRecord* Soldier = HasAuthority() ? FindSoldier(SoldierId) : nullptr;

	if (!Soldier)
	{
		return;
	}

	Soldier->bInField = false;

	// a real fight teaches more than the training ground
	if (bFought)
	{
		for (float* Stat : { &Soldier->Strength, &Soldier->WeaponSkill })
		{
			*Stat = FMath::Min(*Stat + BattleGain * (1.0f - *Stat / StatCap), StatCap);
		}
	}

	NotifyChanged();
}

void AGesithCastle::SoldierFell(int32 SoldierId)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Roster.RemoveAll([SoldierId](const FSoldierRecord& Soldier) { return Soldier.Id == SoldierId; }) > 0)
	{
		NotifyChanged();
	}
}

FText AGesithCastle::GetRankName(ESoldierRank Rank)
{
	switch (Rank)
	{
	case ESoldierRank::Recruit: return NSLOCTEXT("Gesith", "RankRecruit", "신병");
	case ESoldierRank::Private: return NSLOCTEXT("Gesith", "RankPrivate", "병졸");
	case ESoldierRank::Regular: return NSLOCTEXT("Gesith", "RankRegular", "정병");
	case ESoldierRank::Veteran: return NSLOCTEXT("Gesith", "RankVeteran", "고참병");
	default: return NSLOCTEXT("Gesith", "RankElite", "정예병");
	}
}

void AGesithCastle::AdvanceDay()
{
	++Day;

	// residents bring in the day's income first
	Gold += DailyGoldIncome;
	Food += DailyFoodIncome;

	// pay the wages in full, or not at all
	const int32 Wages = GetDailyWages();

	if (Gold >= Wages)
	{
		Gold -= Wages;
		UnpaidGoldDays = 0;
	}
	else
	{
		++UnpaidGoldDays;
	}

	// feed the soldiers
	const int32 FoodNeeded = Roster.Num() * FoodPerSoldier;

	if (Food >= FoodNeeded)
	{
		Food -= FoodNeeded;
		UnpaidFoodDays = 0;
	}
	else
	{
		++UnpaidFoodDays;
	}

	// one soldier leaves for each day past the limit
	if (UnpaidGoldDays >= GoldDaysBeforeDesertion || UnpaidFoodDays >= FoodDaysBeforeDesertion)
	{
		DesertOne();
	}

	NotifyChanged();
}

void AGesithCastle::RunTrainingSession()
{
	// training stops while wages are owed
	if (UnpaidGoldDays > 0)
	{
		return;
	}

	bool bAnyTrained = false;

	for (FSoldierRecord& Soldier : Roster)
	{
		if (!Soldier.bTraining || !Soldier.IsAvailable())
		{
			continue;
		}

		// each session works on one stat, and gains slow down as it nears the cap
		float* Stat = Soldier.GetStat(FMath::RandRange(0, 4));
		*Stat = FMath::Min(*Stat + TrainingGain * (1.0f - *Stat / StatCap), StatCap);

		bAnyTrained = true;
	}

	if (bAnyTrained)
	{
		NotifyChanged();
	}
}

void AGesithCastle::DesertOne()
{
	if (Roster.Num() == 0)
	{
		return;
	}

	const int32 Index = FMath::RandRange(0, Roster.Num() - 1);

	UE_LOG(LogGesith, Log, TEXT("%s left %s over unpaid wages or food"), *Roster[Index].Name, *CastleName);

	Roster.RemoveAt(Index);
}

FSoldierRecord* AGesithCastle::FindSoldier(int32 SoldierId)
{
	return Roster.FindByPredicate([SoldierId](const FSoldierRecord& Soldier) { return Soldier.Id == SoldierId; });
}

void AGesithCastle::NotifyChanged()
{
	OnCastleChanged.Broadcast();
}

void AGesithCastle::OnRep_CastleChanged()
{
	OnCastleChanged.Broadcast();
}

void AGesithCastle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGesithCastle, CastleName);
	DOREPLIFETIME(AGesithCastle, Gold);
	DOREPLIFETIME(AGesithCastle, Food);
	DOREPLIFETIME(AGesithCastle, Day);
	DOREPLIFETIME(AGesithCastle, UnpaidGoldDays);
	DOREPLIFETIME(AGesithCastle, UnpaidFoodDays);
	DOREPLIFETIME(AGesithCastle, Roster);
}
