// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SquadTypes.generated.h"

/** Orders a lord can give to the soldiers following them */
UENUM(BlueprintType)
enum class ESquadOrder : uint8
{
	/** Attack the nearest enemy anywhere on the field */
	Charge,

	/** Walk in formation behind the leader and fight enemies that come close */
	Follow,

	/** Stand in formation where the order was given and fight enemies that come close */
	Hold
};
