/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once
#include "ItemStruct.generated.h"

UENUM(BlueprintType)
enum class EEquipmentSlotType_Isx :uint8
{
	None,
	Head,
	Neck,
	Shoulders,
	Back,
	Chest,
	Hands,
	Finger1,
	Finger2,
	Legs,
	Feet,
	RightHand,
	LeftHand,
	Shield,
	LeftHandSpell,
	RightHandSpell,
	RightItem,
	LeftItem,
	Glasses,
	Jacket,
	Belt,
	Gloves,
	PrimaryWeapon,
	SecondaryWeapon,
	Grenades,
	BodyArmor,
	Backpack,
	Sight,
	MedicalKit,
	Tools,
	Custom_1,
	Custom_2,
	Custom_3,
	Custom_4,
	Custom_5,
	Custom_6,
	Custom_7,
	Custom_8,
	Custom_9,
	Custom_10,
};

USTRUCT(BlueprintType)
struct FSize_isx
{
	GENERATED_BODY()

	//Columns
	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName="Columns",
		meta=(ClampMin = 1, UIMax = 20, NoResetToDefault), Category="Size")
	int32 Width;

	//Rows
	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName="Rows", meta=(ClampMin = 1, UIMax = 20, NoResetToDefault),
		Category="Size")
	int32 Height;

	FSize_isx(const int32 X = 1, const int32 Y = 1):
		Width(X),
		Height(Y)
	{
	}
};

UENUM(BlueprintType)
enum class EItemRotation : uint8
{
	Horizontal = 0,
	Vertical = 1
};

UENUM(BlueprintType)
enum class ESlotsType : uint8
{
	Primary,
	Temp,
	Storage,
	HiddenSlots,
	Equipment
};
