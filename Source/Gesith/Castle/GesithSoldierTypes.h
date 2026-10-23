// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GesithSoldierTypes.generated.h"

/** Soldier ranks, from recruit to elite */
UENUM(BlueprintType)
enum class ESoldierRank : uint8
{
	Recruit,
	Private,
	Regular,
	Veteran,
	Elite
};

/**
 *  One soldier in a castle's roster.
 *  Soldiers are long-lived: they keep their name and stats between battles, and are gone for good when they die.
 */
USTRUCT(BlueprintType)
struct FSoldierRecord
{
	GENERATED_BODY()

	/** Unique within its castle */
	UPROPERTY(BlueprintReadOnly, Category="Soldier")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Soldier")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category="Soldier")
	ESoldierRank Rank = ESoldierRank::Recruit;

	UPROPERTY(BlueprintReadOnly, Category="Soldier|Stats")
	float Health = 10.0f;

	UPROPERTY(BlueprintReadOnly, Category="Soldier|Stats")
	float Strength = 10.0f;

	UPROPERTY(BlueprintReadOnly, Category="Soldier|Stats")
	float Endurance = 10.0f;

	UPROPERTY(BlueprintReadOnly, Category="Soldier|Stats")
	float AttackSpeed = 10.0f;

	UPROPERTY(BlueprintReadOnly, Category="Soldier|Stats")
	float WeaponSkill = 10.0f;

	/** If true, the soldier spends their time at the training ground */
	UPROPERTY(BlueprintReadOnly, Category="Soldier")
	bool bTraining = false;

	/** Average of all stats, used for promotion */
	float GetAverageStat() const
	{
		return (Health + Strength + Endurance + AttackSpeed + WeaponSkill) / 5.0f;
	}

	/** Returns a pointer to the stat with the provided index, 0 to 4 */
	float* GetStat(int32 Index)
	{
		switch (Index)
		{
		case 0: return &Health;
		case 1: return &Strength;
		case 2: return &Endurance;
		case 3: return &AttackSpeed;
		default: return &WeaponSkill;
		}
	}
};
