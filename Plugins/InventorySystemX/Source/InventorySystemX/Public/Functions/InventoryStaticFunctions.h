/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once

#include "CoreMinimal.h"
#include "Structs/ItemStruct.h"
#include "UObject/Object.h"
#include "Objects/Item.h"
#include "InventoryStaticFunctions.generated.h"

/**
 * 
 */


UENUM()
enum class EDirections_ISX : uint8
{
	UP,
	UP_RIGHT,
	RIGHT,
	DOWN_RIGHT,
	DOWN,
	DOWN_LEFT,
	LEFT,
	UP_LEFT,
	NONE
};


UCLASS()
class INVENTORYSYSTEMX_API UInventoryStaticFunctions : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Static Functions")
	static int32 GetColumn(const int32 ArrayIndex, const int32 Rows);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Static Functions")
	static int32 GetRow(const int32 ArrayIndex, const int32 Rows);
	
	UFUNCTION(Category="Inventory Static Functions")
	static bool CheckSize(int32 Index, FSize_isx ItemSize, FSize_isx InventorySize);

	UFUNCTION(Category="Inventory Static Functions")
	static bool CheckSlots(const int32 Index, const TArray<FSlotStruct>& Slots, const FSize_isx ItemSize,
	                       const FSize_isx InventorySize, TArray<int32>& EmptySlots,
	                       const TArray<int32>& ExcludeSlots);

	UFUNCTION(Category="Inventory Static Functions")
	static bool FindEmptySlots(
		const TArray<FSlotStruct>& Slots,
		const FSize_isx ItemSize, const FSize_isx InventorySize,
		EItemRotation& Rotation, TArray<int32>& EmptySlots,
		const TArray<int32>& ExcludeSlots);

	UFUNCTION(Category="Inventory Static Functions")
	static void AddItemToSlots(UObject* Outer, UInventoryComponent* InventoryComponent, TArray<FSlotStruct>& Slots,
	                           TSubclassOf<UItem> ItemClass, const int32 Amount, const int32 Ammo,
	                           const EItemRotation Rotation, const TArray<int32>& SlotIndexes,
	                           const ESlotsType SlotsType);
};
