// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GesithSoldierTypes.h"
#include "GesithCastle.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ACombatCharacter;
class APlayerState;

DECLARE_MULTICAST_DELEGATE(FOnCastleChanged);

/**
 *  A castle with its roster of soldiers and its treasury.
 *  Builds simple grey box walls so it can be dropped into a level as is.
 *  The server runs the economy: daily income, wages, food, training and desertion.
 *  Players inside the castle area can open the castle menu.
 */
UCLASS()
class AGesithCastle : public AActor
{
	GENERATED_BODY()

	/** Area that counts as inside the castle */
	UPROPERTY(VisibleAnywhere, Category="Components")
	UBoxComponent* CastleArea;

	/** Grey box walls, sized from the castle layout. The front wall is split by the gate */
	UPROPERTY(VisibleAnywhere, Category="Components")
	UStaticMeshComponent* BackWall;

	UPROPERTY(VisibleAnywhere, Category="Components")
	UStaticMeshComponent* LeftWall;

	UPROPERTY(VisibleAnywhere, Category="Components")
	UStaticMeshComponent* RightWall;

	UPROPERTY(VisibleAnywhere, Category="Components")
	UStaticMeshComponent* FrontLeftWall;

	UPROPERTY(VisibleAnywhere, Category="Components")
	UStaticMeshComponent* FrontRightWall;

	/** Grey box stand-ins for the places soldiers visit in their free time */
	UPROPERTY(VisibleAnywhere, Category="Components")
	UStaticMeshComponent* Blacksmith;

	UPROPERTY(VisibleAnywhere, Category="Components")
	UStaticMeshComponent* Well;

public:

	/** Castle name shown in the menu */
	UPROPERTY(EditAnywhere, Replicated, Category="Castle")
	FString CastleName = TEXT("Castle");

	/** Gold in the treasury */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_CastleChanged, Category="Castle")
	int32 Gold = 0;

	/** Food in the storehouse */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_CastleChanged, Category="Castle")
	int32 Food = 0;

	/** Current game day, starting at 1 */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_CastleChanged, Category="Castle")
	int32 Day = 1;

	/** Days in a row the gold wages could not be paid */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_CastleChanged, Category="Castle")
	int32 UnpaidGoldDays = 0;

	/** Days in a row the soldiers could not be fed */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_CastleChanged, Category="Castle")
	int32 UnpaidFoodDays = 0;

	/** Soldiers belonging to this castle */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_CastleChanged, Category="Castle")
	TArray<FSoldierRecord> Roster;

	/** Player who rules the castle. Null while the castle has no lord */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_CastleChanged, Category="Castle|Lord")
	TObjectPtr<APlayerState> Lord;

	/** Players who serve the lord as knights */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_CastleChanged, Category="Castle|Lord")
	TArray<TObjectPtr<APlayerState>> Knights;

	/** When knights may manage the castle */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_CastleChanged, Category="Castle|Lord")
	EKnightPermission KnightPermission = EKnightPermission::Always;

	/** True while the lord's character is inside the castle. Kept on the server so every player knows, however far away */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_CastleChanged, Category="Castle|Lord")
	bool bLordInCastle = false;

	/** Team the castle's soldiers fight for */
	UPROPERTY(EditAnywhere, Category="Castle")
	int32 TeamId = 1;

	/** Character spawned for each soldier. If empty, the first player's character class is used */
	UPROPERTY(EditAnywhere, Category="Castle")
	TSubclassOf<ACombatCharacter> SoldierClass;

	/** Broadcast on every machine whenever anything shown in the castle menu changes */
	FOnCastleChanged OnCastleChanged;

protected:

	/** Width and depth of the castle */
	UPROPERTY(EditAnywhere, Category="Castle|Layout", meta = (ClampMin = 500, Units = "cm"))
	float CastleSize = 3000.0f;

	/** Height of the walls */
	UPROPERTY(EditAnywhere, Category="Castle|Layout", meta = (ClampMin = 100, Units = "cm"))
	float WallHeight = 400.0f;

	/** Thickness of the walls */
	UPROPERTY(EditAnywhere, Category="Castle|Layout", meta = (ClampMin = 50, Units = "cm"))
	float WallThickness = 100.0f;

	/** Width of the gate opening in the front wall */
	UPROPERTY(EditAnywhere, Category="Castle|Layout", meta = (ClampMin = 100, Units = "cm"))
	float GateWidth = 600.0f;

	/** Gold at the start of the game */
	UPROPERTY(EditAnywhere, Category="Castle|Economy")
	int32 StartingGold = 300;

	/** Food at the start of the game */
	UPROPERTY(EditAnywhere, Category="Castle|Economy")
	int32 StartingFood = 150;

	/** Gold the residents bring in each day */
	UPROPERTY(EditAnywhere, Category="Castle|Economy")
	int32 DailyGoldIncome = 30;

	/** Food the residents bring in each day */
	UPROPERTY(EditAnywhere, Category="Castle|Economy")
	int32 DailyFoodIncome = 15;

	/** Gold it costs to recruit a soldier */
	UPROPERTY(EditAnywhere, Category="Castle|Economy")
	int32 RecruitCost = 40;

	/** Daily gold wage for each rank, from recruit to elite */
	UPROPERTY(EditAnywhere, Category="Castle|Economy")
	TArray<int32> WageByRank;

	/** Food each soldier eats per day */
	UPROPERTY(EditAnywhere, Category="Castle|Economy")
	int32 FoodPerSoldier = 1;

	/** Soldiers start leaving after this many unpaid days */
	UPROPERTY(EditAnywhere, Category="Castle|Economy")
	int32 GoldDaysBeforeDesertion = 7;

	/** Soldiers start leaving after this many days without food */
	UPROPERTY(EditAnywhere, Category="Castle|Economy")
	int32 FoodDaysBeforeDesertion = 3;

	/** Real seconds in one game day */
	UPROPERTY(EditAnywhere, Category="Castle|Time", meta = (ClampMin = 1, Units = "s"))
	float DayLength = 120.0f;

	/** Real seconds between training sessions */
	UPROPERTY(EditAnywhere, Category="Castle|Training", meta = (ClampMin = 0.1, Units = "s"))
	float TrainingInterval = 5.0f;

	/** Stat points gained per training session, before slowing down near the cap */
	UPROPERTY(EditAnywhere, Category="Castle|Training", meta = (ClampMin = 0))
	float TrainingGain = 0.5f;

	/** Stats can't train past this value */
	UPROPERTY(EditAnywhere, Category="Castle|Training", meta = (ClampMin = 1))
	float StatCap = 100.0f;

	/** Stat points gained in strength and weapon skill for coming back from a fight, before slowing down near the cap */
	UPROPERTY(EditAnywhere, Category="Castle|Battle", meta = (ClampMin = 0))
	float BattleGain = 1.0f;

	/** Average stat needed to promote into each rank after recruit */
	UPROPERTY(EditAnywhere, Category="Castle|Training")
	TArray<float> PromotionThresholds;

	/** Time into the current day */
	float DayTimer = 0.0f;

	/** Time into the current training session */
	float TrainingTimer = 0.0f;

	/** Id given to the next recruited soldier */
	int32 NextSoldierId = 1;

	/** Character of each soldier in the roster, by soldier id. Server only */
	TMap<int32, TWeakObjectPtr<ACombatCharacter>> SoldierPawns;

	/** Time until the soldier characters are checked against the roster again */
	float PawnSyncTimer = 0.0f;

public:

	/** Constructor */
	AGesithCastle();

	/** Places one wall from its center and size */
	static void PlaceWall(UStaticMeshComponent* Wall, const FVector& Center, const FVector& Size);

	/** Resizes the grey box walls when the layout changes in the editor */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Runs the economy on the server */
	virtual void Tick(float DeltaSeconds) override;

	/** Returns true if the location is inside the castle area */
	bool IsInsideCastle(const FVector& Location) const;

	/** Recruits a new soldier if there is enough gold. Server only */
	bool Recruit();

	/** Sends a soldier to the training ground or back. Server only */
	void SetTraining(int32 SoldierId, bool bTraining);

	/** Promotes a soldier to the next rank if their stats are high enough. Server only */
	bool Promote(int32 SoldierId);

	/** Returns true if the soldier's stats are high enough for the next rank */
	bool CanPromote(const FSoldierRecord& Soldier) const;

	/** Returns the average stat needed for the soldier's next rank, or 0 at the top rank */
	float GetNextRankThreshold(const FSoldierRecord& Soldier) const;

	/** Returns the total gold wages for one day */
	int32 GetDailyWages() const;

	/** Returns the gold it costs to recruit a soldier */
	int32 GetRecruitCost() const { return RecruitCost; }

	/** Returns true if the player may manage the castle right now, following the knight permission */
	bool CanManage(const APlayerState* Player) const;

	/** Returns true if the player serves this castle's lord as a knight */
	bool IsKnight(const APlayerState* Player) const;

	/** Makes the player lord of a castle that has none. Server only */
	bool Claim(APlayerState* Player);

	/** Names a player a knight of this castle, or dismisses them. Server only */
	void SetKnight(APlayerState* Player, bool bKnight);

	/** Changes when knights may manage the castle. Server only */
	void SetKnightPermission(EKnightPermission NewPermission);

	/** Returns how many soldiers are in the castle */
	int32 GetAvailableCount() const;

	/** Summons every soldier in the castle to follow a lord and returns them. Server only */
	TArray<FSoldierRecord> MusterSoldiers();

	/** A summoned soldier was dismissed and goes back to their post. Soldiers who fought get a little stronger. Server only */
	void SoldierReturned(int32 SoldierId, bool bFought);

	/** Returns true if any soldier is currently summoned */
	bool HasSoldiersInField() const;

	/** Returns the character of a soldier in the roster, or nullptr. Server only */
	ACombatCharacter* GetSoldierPawn(int32 SoldierId) const;

	/** Picks a random place for an off duty soldier to spend time: resting at the barracks, the blacksmith, the well, or a stroll */
	void PickChoreSpot(FVector& OutLocation, FVector& OutForward) const;

	/** Finds where a soldier stands while in the castle: the barracks, or the training ground while training. Returns false if the soldier isn't in the roster */
	bool GetSoldierPost(int32 SoldierId, FVector& OutLocation, FVector& OutForward, bool& bOutTraining) const;

	/** A soldier died in battle and is gone from the roster for good. Server only */
	void SoldierFell(int32 SoldierId);

	/** Returns the localized name of a rank */
	static FText GetRankName(ESoldierRank Rank);

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Registers replicated properties */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Pays wages, feeds soldiers, collects income and handles desertion */
	void AdvanceDay();

	/** Raises the stats of every soldier in training */
	void RunTrainingSession();

	/** Centers of the blacksmith and the well, relative to the castle */
	FVector GetBlacksmithCenter() const;
	FVector GetWellCenter() const;

	/** Removes one random soldier from the roster */
	void DesertOne();

	/** Spawns characters for soldiers that don't have one and removes characters of soldiers who left the roster */
	void SyncSoldierPawns();

	/** Drops players who left the game and checks whether the lord is home */
	void SyncLordship();

	/** Returns the class used for soldier characters, or nullptr if none is known yet */
	UClass* FindSoldierClass() const;

	/** Returns the soldier with the provided id, or nullptr */
	FSoldierRecord* FindSoldier(int32 SoldierId);

	/** Tells every listener that the castle changed. Clients do this through replication */
	void NotifyChanged();

	/** Called on clients when any castle value replicates */
	UFUNCTION()
	void OnRep_CastleChanged();
};
