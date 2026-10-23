// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GesithWeaponTypes.generated.h"

class UStaticMesh;

/** Kinds of weapons a character can carry in the 1-4 slots */
UENUM(BlueprintType)
enum class EGesithWeaponType : uint8
{
	/** Empty slot */
	None,

	/** One handed sword with a shield. Balanced */
	Sword,

	/** One handed spear with a shield. Longer reach, slower swings */
	Spear,

	/** One handed axe with a shield. Slow but heavy hits */
	Axe,

	/** Ranged. Hold attack to draw, release to shoot. No shield */
	Bow,

	/** Two handed sword. No shield, more damage and reach than the one handed sword */
	GreatSword,

	/** Two handed spear. No shield, the longest reach */
	GreatSpear,

	/** Two handed axe. No shield, the heaviest hits */
	GreatAxe,

	/** An archer's weak sidearm for when enemies get close. No shield */
	Dagger
};

/** How a weapon fights and looks. Every number is a starting point for balancing */
USTRUCT(BlueprintType)
struct FGesithWeaponStats
{
	GENERATED_BODY()

	/** Name shown in the weapon bar */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	FText DisplayName;

	/** Shoots arrows instead of swinging */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	bool bRanged = false;

	/** Needs both hands, so the shield is put away */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	bool bTwoHanded = false;

	/** Multiplies the character's base damage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta = (ClampMin = 0, ClampMax = 10))
	float DamageMultiplier = 1.0f;

	/** Multiplies the attack speed stat. Real attack speed = attack speed stat x this */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta = (ClampMin = 0.1, ClampMax = 3))
	float AttackSpeed = 1.0f;

	/** How far ahead a melee swing reaches */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Melee", meta = (ClampMin = 0, ClampMax = 500, Units = "cm"))
	float Reach = 75.0f;

	/** How wide a melee swing is */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Melee", meta = (ClampMin = 0, ClampMax = 200, Units = "cm"))
	float SwingRadius = 75.0f;

	/** Time to fully draw the bow. Arrows shot earlier do less damage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Ranged", meta = (ClampMin = 0.1, ClampMax = 5, Units = "s"))
	float DrawTime = 0.8f;

	/** Arrow speed at full draw */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Ranged", meta = (ClampMin = 100, ClampMax = 20000, Units = "cm/s"))
	float ArrowSpeed = 4500.0f;

	/** Placeholder mesh held in the hand until real weapon art exists */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Look")
	TObjectPtr<UStaticMesh> Mesh;

	/** Bone or socket the mesh is attached to */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Look")
	FName Socket = FName("hand_r");

	/** Mesh placement relative to the socket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|Look")
	FTransform MeshTransform;
};
