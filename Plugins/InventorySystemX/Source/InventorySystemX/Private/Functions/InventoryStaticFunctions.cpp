/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/


#include "Functions/InventoryStaticFunctions.h"
#include "Components/InventoryComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Objects/Item.h"
#include "Structs/ItemStruct.h"

int32 UInventoryStaticFunctions::GetColumn(const int32 ArrayIndex, const int32 Rows)
{
	
#if ENGINE_MAJOR_VERSION >= 5
	double Reminder;
#else
	float Reminder;
#endif

	UKismetMathLibrary::FMod(ArrayIndex, Rows, Reminder);

	return UKismetMathLibrary::FTrunc(Reminder);
}

int32 UInventoryStaticFunctions::GetRow(const int32 ArrayIndex, const int32 Rows)
{
	if (Rows <= 0) return -1;
	const double TempNum = ArrayIndex / Rows;
	return UKismetMathLibrary::FTrunc(TempNum);
}


bool UInventoryStaticFunctions::CheckSize(const int32 Index, const FSize_isx ItemSize, const FSize_isx InventorySize)
{
	if (GetRow(Index, InventorySize.Width) ==
		GetRow(Index + ItemSize.Width - 1,
		       InventorySize.Width) &&
		GetColumn(Index, InventorySize.Width) ==
		GetColumn(
			Index + (InventorySize.Width * (ItemSize.Height - 1)),
			InventorySize.Width))
	{
		return true;
	}
	return false;
}

bool UInventoryStaticFunctions::CheckSlots(const int32 Index, const TArray<FSlotStruct>& Slots, const FSize_isx ItemSize,
                                           const FSize_isx InventorySize, TArray<int32>& EmptySlots,
                                           const TArray<int32>& ExcludeSlots)
{
	EmptySlots.Empty();

	for (int i = 0; i < ItemSize.Height; ++i)
	{
		const int32 TempIndex = Index + (i == 0 ? 0 : InventorySize.Width * i);

		for (int x = TempIndex; x < TempIndex + ItemSize.Width; ++x)
		{
			if (Slots.IsValidIndex(x) && (Slots[x].IsEmpty || ExcludeSlots.Contains(x)))
			{
				EmptySlots.Add(x);
			}
			else
			{
				EmptySlots.Empty();

				return false;
			}
		}
	}
	return true;
}

bool UInventoryStaticFunctions::FindEmptySlots(const TArray<FSlotStruct>& Slots,
                                               const FSize_isx ItemSize, const FSize_isx InventorySize,
                                               EItemRotation& Rotation, TArray<int32>& EmptySlots,
                                               const TArray<int32>& ExcludeSlots)
{
	for (const FSlotStruct& InventorySlot : Slots)
	{
		if (InventorySlot.IsEmpty || ExcludeSlots.Contains(InventorySlot.Index))
		{
			//Horizontal
			if (CheckSize(InventorySlot.Index, ItemSize, InventorySize) &&
				CheckSlots(InventorySlot.Index, Slots, ItemSize, InventorySize, EmptySlots, ExcludeSlots))
			{
				Rotation = EItemRotation::Horizontal;
				return true;
			}
			if (ItemSize.Height == ItemSize.Width) continue;
			//Vertical
			FSize_isx TempSize;
			TempSize.Height = ItemSize.Width;
			TempSize.Width = ItemSize.Height;

			if (CheckSize(InventorySlot.Index, TempSize, InventorySize) &&
				CheckSlots(InventorySlot.Index, Slots, TempSize, InventorySize, EmptySlots, ExcludeSlots))
			{
				Rotation = EItemRotation::Vertical;
				return true;
			}
		}
	}
	return false;
}


void UInventoryStaticFunctions::AddItemToSlots(UObject* Outer, UInventoryComponent* InventoryComponent,
                                               TArray<FSlotStruct>& Slots, TSubclassOf<UItem> ItemClass,
                                               const int32 Amount, const int32 Ammo,
                                               const EItemRotation Rotation, const TArray<int32>& SlotIndexes,
                                               const ESlotsType SlotsType)
{
	if (!IsValid(Outer)) return;
	if (!ItemClass) return;

	UItem* NewItem = NewObject<UItem>(Outer, ItemClass);
	if (!NewItem) return;


	NewItem->Amount = NewItem->Type == EItemType::Weapon ? 1 : Amount;
	NewItem->Ammo = FMath::Clamp(Ammo, 0, NewItem->MaxStack);
	NewItem->Rotation = Rotation;
	NewItem->OccupiedSlots = SlotIndexes;
	NewItem->SlotsType = SlotsType;
	NewItem->InventoryComponentReference = InventoryComponent;


	for (int x = 0; x < SlotIndexes.Num(); ++x)
	{
		if (Slots.IsValidIndex(SlotIndexes[x]))
		{
			Slots[SlotIndexes[x]].IsEmpty = false;
			Slots[SlotIndexes[x]].ItemReference = NewItem;

			if (x == 0)
			{
				Slots[SlotIndexes[x]].IsPartOfItem = false;
			}
			else
			{
				Slots[SlotIndexes[x]].IsPartOfItem = true;
			}
		}
	}
}
