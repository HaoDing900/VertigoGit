/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/


#include "Components/InventoryComponent.h"
#include "GameFramework/Controller.h"
#include "Components/StorageComponent.h"
#include "Engine/ActorChannel.h"
#include "Functions/InventoryStaticFunctions.h"
#include "Interfaces/InventorySystemInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Structs/CombinesStruct.h"
#include "GameFramework/Pawn.h"
#include <functional>


// Sets default values for this component's properties
UInventoryComponent::UInventoryComponent():
	UseTempSlots(true),
	HiddenItemSlotsType(ESlotsType::Primary),
	CanAddUnRemovableItemsToStorage(false),
	CanAddUnRemovableItemsToGlobalStorage(false),
	AddSwappedItemFromStorageToInventoryOnClose(false),
	SelectedShortcut(0)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	// ...
}


// Called when the game starts
void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	//Get Reference To Interface
	GetPlayerObject();


	if (GetOwnerRole() == ROLE_Authority)
	{
		//Initialize Base And Temp Slots
		InitializeSlots();

		//Initialize Shortcuts
		for (int x = 0; x < 4; ++x)
		{
			FShortcut Shortcut;
			Shortcut.IsEmpty = true;
			Shortcut.Index = x;
			Shortcut.Item = nullptr;
			Shortcuts.Add(Shortcut);
		}

		//Initialize Hidden Slot
		FSlotStruct SlotStruct;
		SlotStruct.Index = 0;
		SlotStruct.IsEmpty = true;
		SlotStruct.IsPartOfItem = false;
		SlotStruct.ItemReference = nullptr;
		SlotStruct.EquipmentSlotType = EEquipmentSlotType_Isx::None;
		HiddenItemSlotsType = ESlotsType::Primary;
		HiddenSlots.Add(SlotStruct);

		//Initialize Equipment Slots
		int32 x = 0;
		for (auto const& Slot : DefaultSlots)
		{
			FSlotStruct NewSlot;
			NewSlot.Index = x;
			NewSlot.IsEmpty = true;
			NewSlot.EquipmentSlotType = Slot;
			NewSlot.IsPartOfItem = false;
			EquipmentSlots.Add(NewSlot);
			++x;
		}


		OnDiscardItem_Server.AddDynamic(this, &UInventoryComponent::CallOnDiscardItemInterface);

		OnChangingAdditionalSlots_Server.AddDynamic(this, &UInventoryComponent::OnAdditionSlotsChanged);
	}

	//Calls the inventory initialization interface.
	if (GetPlayerObject() && PlayerObject->GetClass()->ImplementsInterface(
		UInventorySystemInterface::StaticClass()))
	{
		IInventorySystemInterface::Execute_OnInventoryComponentInitialized(PlayerObject, this);
	}

	//Calls the inventory initialization interface after a character change.
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		if (AController* Controller = Cast<AController>(Owner))
		{
			Controller->GetOnNewPawnNotifier().AddLambda([this](APawn* Pawn)
			{
				if (Pawn && Pawn->GetClass()->ImplementsInterface(UInventorySystemInterface::StaticClass()))
				{
					IInventorySystemInterface::Execute_OnInventoryComponentInitialized(Pawn, this);
				}
			});
		}
	}
}


void UInventoryComponent::CallOnDiscardItemInterface(TSubclassOf<UItem> ItemClass, const int32 Amount, const int32 Ammo,
                                                     const ESlotsType SlotsType)
{
	if (GetPlayerObject() && PlayerObject->GetClass()->ImplementsInterface(
		UInventorySystemInterface::StaticClass()))
		IInventorySystemInterface::Execute_OnDiscardItem(PlayerObject, ItemClass, Amount, Ammo, SlotsType);
}

UObject* UInventoryComponent::GetPlayerObject()
{
	if (IsValid(PlayerObject)) return PlayerObject;

	PlayerObject = nullptr;
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		if (const AController* Controller = Cast<AController>(Owner))
		{
			if (Controller->GetPawn())
			{
				PlayerObject = Controller->GetPawn();
			}
		}
		else if (APawn* Pawn = Cast<APawn>(Owner))
		{
			PlayerObject = Pawn;
		}
		else return nullptr;
	}
	return PlayerObject;
}


void UInventoryComponent::Server_SetUniqueID_Implementation(const bool GenerateRandom, const FString& InUniqueID)
{
	if (GenerateRandom)
		UniqueID = FGuid::NewGuid().ToString();
	else
		UniqueID = InUniqueID;
}


FSize_isx& UInventoryComponent::GetInventorySize(const ESlotsType SlotsType)
{
	if (SlotsType == ESlotsType::Primary)
	{
		return InventorySize;
	}
	if (SlotsType == ESlotsType::Temp)
	{
		return TempInventorySize;
	}
	if (SlotsType == ESlotsType::Storage)
	{
		return IsValid(StorageComponent) ? StorageComponent->GetSize() : TempInventorySize;
	}
	return InventorySize;
}

void UInventoryComponent::SetInventorySize_Implementation(const ESlotsType SlotsType, const FSize_isx NewSize)
{
	if (SlotsType == ESlotsType::Primary)
	{
		InventorySize = NewSize;
	}
	if (SlotsType == ESlotsType::Temp)
	{
		TempInventorySize = NewSize;
	}
}

void UInventoryComponent::Client_OnItemAdded_Implementation(const ESlotsType SlotsType, TSubclassOf<UItem> ItemClass,
                                                            const TArray<int32>& Slots, const int32 Amount,
                                                            const EItemRotation Rotation, const bool CanDestroy,
                                                            const int32 ShortcutSlot, const bool IsEquipped)
{
	OnItemAdded.Broadcast(SlotsType, ItemClass, Slots, Amount, Rotation, CanDestroy, ShortcutSlot, IsEquipped);
}

void UInventoryComponent::Client_OnItemRemoved_Implementation(const TArray<int32>& Slots, const ESlotsType SlotsType)
{
	OnItemRemoved.Broadcast(Slots, SlotsType);
}

void UInventoryComponent::Client_OnItemChangeAmount_Implementation(const int32 Index, const ESlotsType SlotsType,
                                                                   const int32 Amount)
{
	OnItemChangeAmount.Broadcast(Index, SlotsType, Amount);
}

void UInventoryComponent::Client_OnItemMoved_Implementation(const bool IsMoved, const bool TempSlotsHasItems,
                                                            const int32 Index, const ESlotsType SlotsType)
{
	OnItemMoved.Broadcast(IsMoved, TempSlotsHasItems, Index, SlotsType);
}

void UInventoryComponent::Client_OnAllItemsRemoved_Implementation(const ESlotsType SlotsType)
{
	OnAllItemsRemoved.Broadcast(SlotsType);
}

void UInventoryComponent::Client_OnChangingAdditionalSlots_Implementation(
	const bool TempSlotsOccupied, const bool HiddenSlotsOccupied)
{
	OnChangingAdditionalSlots.Broadcast(TempSlotsOccupied, HiddenSlotsOccupied);
}


void UInventoryComponent::Client_OnPrimarySlotsLoaded_Implementation()
{
	OnPrimarySlotsLoaded.Broadcast();
}


void UInventoryComponent::Client_OnLoad_Implementation(const int32 InSelectedShortcut)
{
	OnLoad.Broadcast(InSelectedShortcut);
}

TArray<FSlotStruct>& UInventoryComponent::GetSlots(const ESlotsType Type)
{
	if (Type == ESlotsType::Primary)
		return InventorySlots;
	if (Type == ESlotsType::Temp)
		return TempInventorySlots;
	if (Type == ESlotsType::Storage)
		return IsValid(StorageComponent) ? StorageComponent->GetSlots() : TempInventorySlots;
	if (Type == ESlotsType::HiddenSlots)
		return HiddenSlots;
	if (Type == ESlotsType::Equipment)
		return EquipmentSlots;
	return InventorySlots;
}

void UInventoryComponent::InitializeSlots_Implementation()
{
	int32 const Size = InventorySize.Height * InventorySize.Width;
	for (int32 x = 0; x < Size; x++)
	{
		FSlotStruct Slot;
		Slot.Index = x;
		Slot.IsEmpty = true;
		Slot.IsPartOfItem = false;
		Slot.ItemReference = nullptr;

		InventorySlots.Add(Slot);
	}

	if (UseTempSlots)
	{
		int32 const SizeTemp = TempInventorySize.Height * TempInventorySize.Width;
		for (int32 x = 0; x < SizeTemp; x++)
		{
			FSlotStruct Slot;
			Slot.Index = x;
			Slot.IsEmpty = true;
			Slot.IsPartOfItem = false;
			Slot.ItemReference = nullptr;
			Slot.EquipmentSlotType = EEquipmentSlotType_Isx::None;

			TempInventorySlots.Add(Slot);
		}
	}
	else
	{
		TempInventorySize = FSize_isx(0, 0);
	}
}

void UInventoryComponent::Server_AddItem_2_Implementation(TSubclassOf<UItem> ItemClass, int32 Amount, const int32 Ammo,
                                                          const ESlotsType SlotType,
                                                          const bool AddRemainingToTempSlots)
{
	if (!ItemClass) return; //false;
	if (Amount <= 0) return; //false;

	const UItem* ItemObject = ItemClass.GetDefaultObject();
	if (!ItemObject) return; // false;

	const int32 MaxStack = ItemObject->MaxStack;

	if (MaxStack > 1 && ItemObject->IsStackable())
	{
		int32 CanAdd;
		UItem* Item;

		while (FindItemToStack(ItemClass, SlotType, CanAdd, Item))
		{
			if (!Item) break; // false;

			if (CanAdd >= Amount)
			{
				SetItemAmount(Item, Item->Amount + Amount);
				return; // true;
			}
			SetItemAmount(Item, Item->Amount + CanAdd);
			Amount -= CanAdd;

			if (Amount <= 0) return; // true;
		}
	}

	TArray<int32> EmptySlots;
	EItemRotation Rotation;

	while (FindEmptySlots(SlotType, ItemObject->Size.Width, ItemObject->Size.Height, Rotation,
	                      EmptySlots))
	{
		if (ItemObject->IsStackable())
		{
			if (Amount <= MaxStack)
			{
				AddItemToSlots(ItemClass, Amount, Rotation, EmptySlots, SlotType, Ammo);
				return; // true;
			}
			AddItemToSlots(ItemClass, MaxStack, Rotation, EmptySlots, SlotType, Ammo);
			Amount -= MaxStack;
		}
		else
		{
			AddItemToSlots(ItemClass, 1, Rotation, EmptySlots, SlotType, Ammo);
			Amount -= 1;
		}
		if (Amount <= 0)
		{
			return; // true;
		}
	}

	if (SlotType == ESlotsType::Primary && Amount > 0 && AddRemainingToTempSlots)
	{
		Server_AddItem_2(ItemClass, Amount, Ammo, ESlotsType::Temp);
	}
}


int32 UInventoryComponent::CanAddItemCount(TSubclassOf<UItem> ItemClass, int32 Amount, const ESlotsType SlotsType)
{
	if (!ItemClass) return 0;
	if (Amount <= 0) return 0;

	const int32 TempAmount = Amount;

	TArray<FSlotStruct> Slots = GetSlots(SlotsType);

	const UItem* ItemDefaults = ItemClass.GetDefaultObject();

	if (!IsValid(ItemDefaults)) return 0;

	const int32 MaxStack = ItemDefaults->MaxStack;

	if (ItemDefaults->IsStackable())
	{
		for (FSlotStruct& Slot : Slots)
		{
			if (Slot.IsEmpty || Slot.IsPartOfItem || !IsValid(Slot.ItemReference)) continue;

			UItem* NewItem = NewObject<UItem>(this, Slot.ItemReference->GetClass());
			if (!NewItem) continue;
			NewItem->Amount = Slot.ItemReference->Amount;
			NewItem->OccupiedSlots = Slot.ItemReference->OccupiedSlots;
			NewItem->Rotation = Slot.ItemReference->Rotation;

			Slot.ItemReference = NewItem;
		}

		int32 CanAdd;
		UItem* Item;

		while (FindItemToStack(ItemClass, Slots, CanAdd, Item))
		{
			if (!Item) break; // false;

			if (CanAdd >= Amount)
			{
				SetItemAmount(Item, Item->Amount + Amount);
				return TempAmount; // true;
			}
			SetItemAmount(Item, Item->Amount + CanAdd);

			Amount -= CanAdd;

			if (Amount <= 0) return TempAmount; // true;
		}
	}


	TArray<int32> EmptySlots;
	EItemRotation Rotation;

	if (SlotsType == ESlotsType::Equipment)
	{
		while (Amount > 0)
		{
			const int32 FreeSlot = FindFreeEquipmentSlot(ItemClass, Rotation);
			if (FreeSlot == INDEX_NONE) return TempAmount - Amount;
			if (Amount <= MaxStack)
			{
				AddItemToSlotsLocal(ItemClass, Amount, Rotation, {FreeSlot}, Slots);
				return TempAmount;
			}
			AddItemToSlotsLocal(ItemClass, MaxStack, Rotation, {FreeSlot}, Slots);
			Amount -= MaxStack;
		}
		return TempAmount;
	}

	while (FindEmptySlotsAdvantage(Slots, SlotsType, ItemDefaults->Size.Width, ItemDefaults->Size.Height, Rotation,
	                               EmptySlots, TArray<int32>()))
	{
		if (ItemDefaults->IsStackable())
		{
			if (Amount <= MaxStack)
			{
				AddItemToSlotsLocal(ItemClass, Amount, Rotation, EmptySlots, Slots);
				return TempAmount; // true;
			}
			AddItemToSlotsLocal(ItemClass, MaxStack, Rotation, EmptySlots, Slots);
			Amount -= MaxStack;
		}
		else
		{
			AddItemToSlotsLocal(ItemClass, 1, Rotation, EmptySlots, Slots);
			Amount -= 1;
		}
		if (Amount <= 0)
		{
			return TempAmount; // true;
		}
	}
	return TempAmount - Amount;
}

bool UInventoryComponent::CheckSlots(const int32 Index, const int32 Width, const int32 Height,
                                     TArray<int32>& EmptySlots,
                                     const ESlotsType SlotType, const TArray<int32>& ExcludeSlots)
{
	EmptySlots.Empty();

	for (int i = 0; i < Height; ++i)
	{
		const int32 TempIndex = Index + (i == 0 ? 0 : GetInventorySize(SlotType).Width * i);

		for (int x = TempIndex; x < TempIndex + Width; ++x)
		{
			if (GetSlots(SlotType).IsValidIndex(x) && (GetSlots(SlotType)[x].IsEmpty || ExcludeSlots.Contains(x)))
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

bool UInventoryComponent::FindEmptySlotsAdvantage(const TArray<FSlotStruct>& Slots, const ESlotsType SlotsType,
                                                  const int32 Width, const int32 Height,
                                                  EItemRotation& Rotation, TArray<int32>& EmptySlots,
                                                  const TArray<int32>& ExcludeSlots)
{
	for (const FSlotStruct& InventorySlot : Slots)
	{
		if (InventorySlot.IsEmpty || ExcludeSlots.Contains(InventorySlot.Index))
		{
			//Horizontal
			if (CheckSize(InventorySlot.Index, Width, Height, SlotsType) &&
				CheckSlots(InventorySlot.Index, Width,
				           Height, EmptySlots, SlotsType, ExcludeSlots))
			{
				Rotation = EItemRotation::Horizontal;
				return true;
			}
			if (Height == Width) continue;
			//Vertical
			if (CheckSize(InventorySlot.Index, Height, Width, SlotsType) &&
				CheckSlots(InventorySlot.Index, Height,
				           Width, EmptySlots, SlotsType, ExcludeSlots))
			{
				Rotation = EItemRotation::Vertical;
				return true;
			}
		}
	}
	return false;
}

bool UInventoryComponent::CheckSize(const int32 Index, const int32 Width, const int32 Height,
                                    const ESlotsType SlotsType)
{
	if (UInventoryStaticFunctions::GetRow(Index, GetInventorySize(SlotsType).Width) ==
		UInventoryStaticFunctions::GetRow(Index + Width - 1,
		                                  GetInventorySize(SlotsType).Width) &&
		UInventoryStaticFunctions::GetColumn(Index, GetInventorySize(SlotsType).Width) ==
		UInventoryStaticFunctions::GetColumn(
			Index + (GetInventorySize(SlotsType).Width * (Height - 1)),
			GetInventorySize(SlotsType).Width))
	{
		return true;
	}
	return false;
}

void UInventoryComponent::AddItemToSlots_Implementation(TSubclassOf<UItem> ItemClass, const int32 Amount,
                                                        const EItemRotation Rotation, const TArray<int32>& Slots,
                                                        const ESlotsType SlotsType, const int32 Ammo)
{
	if (!ItemClass) return;

	UItem* NewItem = NewObject<UItem>(this, ItemClass);
	if (!NewItem) return;

	if (NewItem->Type == EItemType::Weapon)
	{
		NewItem->Amount = 1;
		NewItem->Ammo = FMath::Clamp(Ammo, 0, NewItem->MaxStack);
	}
	else
	{
		NewItem->Amount = FMath::Clamp(Amount, 0, NewItem->MaxStack);
	}

	NewItem->Rotation = Rotation;
	NewItem->OccupiedSlots = Slots;
	NewItem->SlotsType = SlotsType;
	NewItem->InventoryComponentReference = this;


	FillSlots(NewItem, Slots, SlotsType);

	if (SlotsType != ESlotsType::Storage || GEngine->GetNetMode(GetWorld()) == NM_Standalone)
		Client_OnItemAdded(SlotsType, ItemClass, Slots, NewItem->Type == EItemType::Weapon ? NewItem->Ammo : Amount,
		                   Rotation, NewItem->CanDestroy(this, GetInventoryOwnerPawn()), -1, IsEquippedItem(NewItem));


	Client_OnChangingAdditionalSlots(IsSlotsHaveItems(ESlotsType::Temp), IsValidSwappedItem());
	OnChangingAdditionalSlots_Server.Broadcast();
}

void UInventoryComponent::AddItemToSlotsLocal(TSubclassOf<UItem> ItemClass, const int32 Amount,
                                              const EItemRotation Rotation, const TArray<int32>& Slots,
                                              TArray<FSlotStruct>& SlotsStruct)
{
	if (!ItemClass) return;

	UItem* NewItem = NewObject<UItem>(this, ItemClass);
	if (!NewItem) return;

	NewItem->Amount = Amount;
	NewItem->Rotation = Rotation;
	NewItem->OccupiedSlots = Slots;
	NewItem->InventoryComponentReference = this;


	for (int x = 0; x < Slots.Num(); ++x)
	{
		if (SlotsStruct.IsValidIndex(Slots[x]))
		{
			SlotsStruct[Slots[x]].IsEmpty = false;
			if (x == 0)
			{
				SlotsStruct[Slots[x]].IsPartOfItem = false;
			}
			else
			{
				SlotsStruct[Slots[x]].IsPartOfItem = true;
			}
			SlotsStruct[Slots[x]].ItemReference = NewItem;
		}
	}
}

bool UInventoryComponent::FindItemToStack(const TSubclassOf<UItem> ItemClass, const TArray<FSlotStruct>& Slots,
                                          int32& CanAdd, UItem*& Item)
{
	const UItem* ItemObject = ItemClass.GetDefaultObject();
	if (!ItemObject) return false;

	if (ItemObject->IsStackable())
	{
		const int32 MaxStack = ItemObject->MaxStack;
		for (const FSlotStruct& Slot : Slots)
		{
			if (!Slot.IsEmpty && !Slot.IsPartOfItem && IsValid(Slot.ItemReference) && Slot.ItemReference->GetClass() ==
				ItemClass)
			{
				if (Slot.ItemReference->Amount < MaxStack)
				{
					CanAdd = MaxStack - Slot.ItemReference->Amount;
					Item = Slot.ItemReference;
					return true;
				}
			}
		}
	}
	return false;
}

bool UInventoryComponent::GetItemInSlot(const int32 SlotIndex, const ESlotsType SlotType, UItem*& Item)
{
	if (GetSlots(SlotType).IsValidIndex(SlotIndex) && IsValid(GetSlots(SlotType)[SlotIndex].ItemReference))
	{
		Item = GetSlots(SlotType)[SlotIndex].ItemReference;
		return true;
	}
	Item = nullptr;
	return false;
}


void UInventoryComponent::Server_DiscardItem_Implementation(const int32 SlotIndex, const ESlotsType SlotType,
                                                            const int32 Amount, const bool RemoveAll)
{
	UItem* Item;
	if (!GetItemInSlot(SlotIndex, SlotType, Item)) return;
	const int32 CanDiscard = RemoveAll ? Item->Amount : Item->Amount - Amount >= 0 ? Amount : Item->Amount;
	Server_RemoveItemsInSlot(SlotIndex, SlotType, Amount, RemoveAll);

	OnDiscardItem_Server.Broadcast(Item->GetClass(), CanDiscard, Item->Ammo, SlotType);
}

void UInventoryComponent::Server_RemoveItemByRef_Implementation(const UItem* Item, const int32 Amount,
                                                                const bool RemoveAll)
{
	if (!IsValid(Item)) return;

	if (!Item->OccupiedSlots.IsValidIndex(0)) return;

	Server_RemoveItemsInSlot(Item->OccupiedSlots[0], Item->SlotsType, Amount, RemoveAll);
}


void UInventoryComponent::Server_RemoveItemsInSlot_Implementation(const int32 SlotIndex, const ESlotsType SlotType,
                                                                  const int32 Amount,
                                                                  const bool RemoveAll)
{
	if (Amount <= 0) return; // false;
	UItem* Item;
	if (GetItemInSlot(SlotIndex, SlotType, Item))
	{
		if (RemoveAll)
		{
			Server_UnequipItem_2(Item);
			Server_CheckAndRemoveItemFromShortcuts(Item);
			ClearSlots(SlotType, Item->OccupiedSlots);
			return; // true;
		}
		const int32 TempAmount = Item->GetAmount() - Amount;
		if (TempAmount <= 0)
		{
			Server_UnequipItem_2(Item);
			Server_CheckAndRemoveItemFromShortcuts(Item);
			ClearSlots(SlotType, Item->OccupiedSlots);
			return; // true;
		}
		SetItemAmount(Item, TempAmount);
	}
	return; // false;
}

void UInventoryComponent::ClearSlots_Implementation(const ESlotsType SlotsType, const TArray<int32>& Slots)
{
	for (const int32& SlotIndex : Slots)
	{
		if (GetSlots(SlotsType).IsValidIndex(SlotIndex))
		{
			UItem* EquippedItemRef = nullptr;
			if (SlotsType == ESlotsType::Equipment)
			{
				FSlotStruct SlotInfo;
				if (GetSlotInfo(SlotIndex, SlotsType, SlotInfo))
				{
					EquippedItemRef = SlotInfo.ItemReference;
				}
			}
			GetSlots(SlotsType)[SlotIndex].IsEmpty = true;
			GetSlots(SlotsType)[SlotIndex].IsPartOfItem = false;
			GetSlots(SlotsType)[SlotIndex].ItemReference = nullptr;

			if (SlotsType == ESlotsType::Equipment)
			{
				OnEquipmentSlotChanged_Server.Broadcast(false, SlotIndex, EquippedItemRef);
			}
		}
	}

	if (SlotsType != ESlotsType::Storage || (GEngine->GetNetMode(GetWorld()) == NM_Standalone))
		Client_OnItemRemoved(Slots, SlotsType);
}

void UInventoryComponent::SetItemAmount_Implementation(UItem* Item, const int32 Amount)
{
	if (IsValid(Item))
	{
		if (Amount <= 0)
			Server_RemoveItemByRef(Item, 1, true);
		else
			Item->Amount = Amount;

		if (Item->OccupiedSlots.IsValidIndex(0))
		{
			const int32 Index = Item->OccupiedSlots[0];
			const ESlotsType SlotsType = Item->SlotsType;
			Client_OnItemChangeAmount(Index, SlotsType, Amount);
		}


		return; // true;
	}
	return; // false;
}

void UInventoryComponent::Server_DiscardAllItems_Implementation(const ESlotsType SlotsType)
{
	for (UItem* const& Item : GetAllItems(SlotsType))
	{
		if (!IsValid(Item)) continue;
		Server_UnequipItem_2(Item);
		Server_CheckAndRemoveItemFromShortcuts(Item);
		OnDiscardItem_Server.Broadcast(Item->GetClass(), Item->Amount, Item->Ammo, SlotsType);
	}
	Server_ClearAllSlots(SlotsType);
}

bool UInventoryComponent::GetSlotInfo(const int32 Index, const ESlotsType SlotsType, FSlotStruct& SlotInfo)
{
	if (GetSlots(SlotsType).IsValidIndex(Index))
	{
		SlotInfo = GetSlots(SlotsType)[Index];
		return true;
	}
	return false;
}

bool UInventoryComponent::CanItemAddedToSlots(const int32 SlotOfItem, const ESlotsType ItemSlotType,
                                              const EItemRotation Rotation,
                                              const int32 SlotToAdd, const ESlotsType SlotsTypeToAdd,
                                              TArray<int32>& EmptySlots)
{
	if (IsValidSwappedItem() && ItemSlotType != ESlotsType::HiddenSlots) return false;

	UItem* ItemInSlot;
	if (IsValidSwappedItem() && GetItemInSlot(SlotToAdd, SlotsTypeToAdd, ItemInSlot))
	{
		if (GetSlots(ESlotsType::HiddenSlots)[0].ItemReference == ItemInSlot) return false;
		if (ItemInSlot->SlotsType == ItemSlotType) return false;
	}

	if (SlotsTypeToAdd == ESlotsType::Storage && !IsValid(StorageComponent)) return false;

	FSlotStruct SlotInfo;
	if (!GetSlotInfo(SlotOfItem, ItemSlotType, SlotInfo)) return false;
	if (!IsValid(SlotInfo.ItemReference)) return false;


	if (SlotsTypeToAdd == ESlotsType::Equipment)
	{
		bool CanEquip;
		EEquipmentSlotType_Isx EquipmentType;
		EItemRotation ItemRotation;


		SlotInfo.ItemReference->CanEquip(this, GetInventoryOwnerPawn(), CanEquip, EquipmentType, ItemRotation);

		if (!CanEquip) return false;

		FSlotStruct SlotToAddInfo;
		if (!GetSlotInfo(SlotToAdd, SlotsTypeToAdd, SlotToAddInfo)) return false;
		if (SlotToAddInfo.EquipmentSlotType != EquipmentType) return false;

		if (SlotToAddInfo.IsEmpty) return true;

		return false;
	}

	if (!SlotInfo.ItemReference->CanDestroy(this, GetInventoryOwnerPawn())
		&& SlotsTypeToAdd == ESlotsType::Storage)
	{
		if (StorageComponent->IsGlobalStorage)
		{
			if (!CanAddUnRemovableItemsToGlobalStorage)
				return false;
		}
		else
		{
			if (!CanAddUnRemovableItemsToStorage)
				return false;
		}
	}


	const FSize_isx Size = SlotInfo.ItemReference->Size;
	TArray<int32> ExcludeSlots = SlotInfo.ItemReference->OccupiedSlots;
	if (IsValidSwappedItem()) ExcludeSlots.Empty();

	if (IsEmptySlotsForItem(SlotToAdd, Size.Width, Size.Height, SlotsTypeToAdd, Rotation, EmptySlots,
	                        ItemSlotType == SlotsTypeToAdd ? ExcludeSlots : TArray<int32>()))
	{
		return true;
	}
	return false;
}

bool UInventoryComponent::IsEmptySlotsForItem(const int32 Index, const int32 Width, const int32 Height,
                                              const ESlotsType SlotType, const EItemRotation Rotation,
                                              TArray<int32>& EmptySlots, const TArray<int32>& ExcludeSlots)
{
	const int32 ItemWidth = Rotation == EItemRotation::Horizontal ? Width : Height;
	const int32 ItemHeight = Rotation == EItemRotation::Horizontal ? Height : Width;

	if (CheckSize(Index, ItemWidth, ItemHeight, SlotType) &&
		CheckSlots(Index, ItemWidth, ItemHeight, EmptySlots, SlotType, ExcludeSlots))
	{
		return true;
	}
	return false;
}

bool UInventoryComponent::IsSlotsHaveItems(const ESlotsType SlotsType)
{
	for (const FSlotStruct& Slot : GetSlots(SlotsType))
	{
		if (!Slot.IsEmpty) return true;
	}
	return false;
}

void UInventoryComponent::Server_ClearAllSlots_Implementation(const ESlotsType SlotsType)
{
	for (const FSlotStruct& Slot : GetSlots(SlotsType))
	{
		if (!GetSlots(SlotsType).IsValidIndex(Slot.Index)) continue;
		GetSlots(SlotsType)[Slot.Index].IsEmpty = true;
		GetSlots(SlotsType)[Slot.Index].IsPartOfItem = false;
		GetSlots(SlotsType)[Slot.Index].ItemReference = nullptr;
	}
	Client_OnAllItemsRemoved(SlotsType);
}

void UInventoryComponent::LoadPrimarySlotsFromArray_Implementation()
{
	if (SavedPrimaryItemsArray.Num() > 0 && IsSlotsHaveItems(ESlotsType::Temp) || IsValidSwappedItem() &&
		HiddenItemSlotsType != ESlotsType::Storage)
	{
		Server_ClearAllSlots(ESlotsType::Primary);
		Server_ClearAllSlots(ESlotsType::Temp);

		TArray<int32> EquipmentSlotsIndexes;
		for (FSlotStruct const& Slot : GetSlots(ESlotsType::Equipment))
		{
			if (!Slot.IsEmpty) EquipmentSlotsIndexes.Add(Slot.Index);
		}

		Server_ClearAllSlots(ESlotsType::Equipment);

		for (int32 const& Index : EquipmentSlotsIndexes)
		{
			OnEquipmentSlotChanged_Server.Broadcast(false, Index, nullptr);
		}


		for (const FItemDataInfo& Data : SavedPrimaryItemsArray)
		{
			if (!IsValid(Data.Item)) continue;


			Data.Item->OccupiedSlots = Data.Slots;
			Data.Item->Rotation = Data.Rotation;
			Data.Item->SlotsType = Data.SlotsType;

			FillSlots(Data.Item, Data.Slots, Data.SlotsType);

			if (Data.SlotsType == ESlotsType::Equipment)
			{
				OnEquipmentSlotChanged_Server.Broadcast(true, Data.Slots.IsValidIndex(0) ? Data.Slots[0] : -1,
				                                        Data.Item);
			}
		}
	}

	if (HiddenSlots.IsValidIndex(0) && IsValid(HiddenSlots[0].ItemReference))
	{
		if (HiddenItemSlotsType == ESlotsType::Storage)
		{
			if (AddSwappedItemFromStorageToInventoryOnClose)
			{
				UItem* Item = HiddenSlots[0].ItemReference;
				TArray<int32> EmptySlots;
				EItemRotation Rotation;
				if (FindEmptySlots(ESlotsType::Primary, Item->Size.Width, Item->Size.Height, Rotation,
				                   EmptySlots))
				{
					AddExistingItemToSlots(Item, ESlotsType::Primary, Rotation, EmptySlots);
				}
				else
					Server_DiscardItem(0, ESlotsType::HiddenSlots, 1, true);
			}
			else
				Server_DiscardItem(0, ESlotsType::HiddenSlots, 1, true);
		}
		HiddenSlots[0].ItemReference = nullptr;
		HiddenSlots[0].IsEmpty = true;
		HiddenItemSlotsType = ESlotsType::Primary;
	}
	Client_OnPrimarySlotsLoaded();
}

void UInventoryComponent::SavePrimarySlotsInArray_Implementation()
{
	SavedPrimaryItemsArray.Empty();

	for (const auto& Slot : GetSlots(ESlotsType::Primary))
	{
		if (!Slot.IsEmpty && !Slot.IsPartOfItem && IsValid(Slot.ItemReference))
		{
			FItemDataInfo Data;
			Data.Item = Slot.ItemReference;
			Data.Slots = Slot.ItemReference->OccupiedSlots;
			Data.Rotation = Slot.ItemReference->Rotation;
			Data.SlotsType = Slot.ItemReference->SlotsType;
			SavedPrimaryItemsArray.Add(Data);
		}
	}

	for (const auto& Slot : GetSlots(ESlotsType::Equipment))
	{
		if (!Slot.IsEmpty && !Slot.IsPartOfItem && IsValid(Slot.ItemReference))
		{
			FItemDataInfo Data;
			Data.Item = Slot.ItemReference;
			Data.Slots = Slot.ItemReference->OccupiedSlots;
			Data.Rotation = Slot.ItemReference->Rotation;
			Data.SlotsType = Slot.ItemReference->SlotsType;
			SavedPrimaryItemsArray.Add(Data);
		}
	}
}

void UInventoryComponent::MoveItem_Implementation(const int32 SlotOfItem, const ESlotsType ItemSlotType,
                                                  const EItemRotation Rotation,
                                                  const int32 SlotToAdd, const ESlotsType SlotsTypeToAdd)
{
	TArray<int32> EmptySlots;
	if (CanItemAddedToSlots(SlotOfItem, ItemSlotType, Rotation, SlotToAdd, SlotsTypeToAdd, EmptySlots))
	{
		if (!GetSlots(ItemSlotType).IsValidIndex(SlotOfItem))
		{
			Client_OnItemMoved(false, IsSlotsHaveItems(ESlotsType::Temp), SlotToAdd, SlotsTypeToAdd);

			return;
		} // false;
		UItem* Item; // = GetSlots(ItemSlotType)[SlotOfItem].ItemReference;
		GetItemInSlot(SlotOfItem, ItemSlotType, Item);
		if (!IsValid(Item))
		{
			Client_OnItemMoved(false, IsSlotsHaveItems(ESlotsType::Temp), SlotToAdd, SlotsTypeToAdd);
			return;
		} // false;

		if (SlotsTypeToAdd != ESlotsType::Equipment)
			ClearSlots(ItemSlotType, Item->OccupiedSlots);

		HiddenItemSlotsType = ESlotsType::Primary;


		if (ItemSlotType == ESlotsType::Primary && SlotsTypeToAdd == ESlotsType::Storage)
		{
			const int32 ShortcutIndex = FindItemInShortcuts(Item);
			if (ShortcutIndex != INDEX_NONE)
				Server_RemoveItemFromShortcut(ShortcutIndex);
			Server_UnequipItem_2(Item);
		}


		if (SlotsTypeToAdd == ESlotsType::Equipment)
			AddItemToEquipmentSlot_2(SlotToAdd, Item);
		else
			AddExistingItemToSlots(Item, SlotsTypeToAdd, Rotation, EmptySlots);

		Client_OnItemMoved(true, IsSlotsHaveItems(ESlotsType::Temp), SlotToAdd, SlotsTypeToAdd);


		if (ItemSlotType != SlotsTypeToAdd)
		{
			Client_OnChangingAdditionalSlots(IsSlotsHaveItems(ESlotsType::Temp), IsValidSwappedItem());
			OnChangingAdditionalSlots_Server.Broadcast();
		}
		return; // true;
	}
	Client_OnItemMoved(false, IsSlotsHaveItems(ESlotsType::Temp), SlotToAdd, SlotsTypeToAdd);
	return; // false;
}

void UInventoryComponent::AddExistingItemToSlots(UItem* Item, const ESlotsType SlotsType, const EItemRotation Rotation,
                                                 const TArray<int32>& EmptySlots)
{
	if (!IsValid(Item)) return;

	Item->OccupiedSlots = EmptySlots;
	Item->Rotation = Rotation;
	Item->SlotsType = SlotsType;

	if (SlotsType == ESlotsType::Storage)
		Item->InventoryComponentReference = nullptr;
	else
		Item->InventoryComponentReference = this;

	if (Item->Size.Width == Item->Size.Height)
	{
		Item->Rotation = EItemRotation::Horizontal;
	}

	FillSlots(Item, EmptySlots, SlotsType);
	Client_OnChangingAdditionalSlots(IsSlotsHaveItems(ESlotsType::Temp), IsValidSwappedItem());
	OnChangingAdditionalSlots_Server.Broadcast();

	if (SlotsType != ESlotsType::Storage || (GEngine->GetNetMode(GetWorld()) == NM_Standalone))
		Client_OnItemAdded(SlotsType, Item->GetClass(), EmptySlots,
		                   Item->Type == EItemType::Weapon ? Item->Ammo : Item->Amount, Item->Rotation,
		                   Item->CanDestroy(this, GetInventoryOwnerPawn()), FindItemInShortcuts(Item),
		                   IsEquippedItem(Item));
}

void UInventoryComponent::FillSlots_Implementation(UItem* Item, const TArray<int32>& Slots, const ESlotsType SlotsType)
{
	if (!IsValid(Item)) return;
	for (int x = 0; x < Slots.Num(); ++x)
	{
		if (GetSlots(SlotsType).IsValidIndex(Slots[x]))
		{
			Item->SlotsType = SlotsType;
			Item->OccupiedSlots = Slots;

			GetSlots(SlotsType)[Slots[x]].IsEmpty = false;
			GetSlots(SlotsType)[Slots[x]].ItemReference = Item;
			if (x == 0)
			{
				GetSlots(SlotsType)[Slots[x]].IsPartOfItem = false;
			}
			else
			{
				GetSlots(SlotsType)[Slots[x]].IsPartOfItem = true;
			}
		}
	}
}

bool UInventoryComponent::CanCombineItem(TSubclassOf<UItem> ItemClass) const
{
	if (!IsValid(ItemClass)) return false;
	if (!IsValid(CombinesDataTable)) return false;


	TArray<FCombinesStruct*> AllRows;
	CombinesDataTable->GetAllRows<FCombinesStruct>(FString(), AllRows);


	for (const FCombinesStruct* Row : AllRows)
	{
		if (Row->FirstItem == ItemClass || Row->SecondItem == ItemClass)
		{
			return true;
		}
	}
	return false;
}

bool UInventoryComponent::CanCombineItems(TSubclassOf<UItem> FirstItemClass, TSubclassOf<UItem> SecondItemClass)
{
	if (!IsValid(FirstItemClass)) return false;
	if (!IsValid(SecondItemClass)) return false;
	if (!IsValid(CombinesDataTable)) return false;


	TArray<FCombinesStruct*> AllRows;
	CombinesDataTable->GetAllRows<FCombinesStruct>(FString(), AllRows);


	for (const FCombinesStruct* Row : AllRows)
	{
		if (Row->FirstItem == FirstItemClass && Row->SecondItem == SecondItemClass ||
			Row->FirstItem == SecondItemClass && Row->SecondItem == FirstItemClass)
		{
			return true;
		}
	}
	return false;
}

void UInventoryComponent::SplitItem_Implementation(const int32 Index, ESlotsType SlotType)
{
	if (!GetSlots(SlotType).IsValidIndex(Index)) return;
	const UItem* Item = GetSlots(SlotType)[Index].ItemReference;
	if (!IsValid(Item)) return;
	if (Item->Amount <= 1) return;
	int32 Amount = Item->Amount;
	const int32 Mod = Amount % 2;
	Amount -= Mod == 0 ? 0 : 1;
	Amount /= 2;
	TArray<int32> EmptySlots;
	EItemRotation Rotation;
	SlotType = SlotType == ESlotsType::Equipment ? ESlotsType::Primary : SlotType;
	if (FindEmptySlots(SlotType, Item->Size.Width, Item->Size.Height, Rotation,
	                   EmptySlots))
	{
		Server_RemoveItemByRef(Item, Amount);
		AddItemToSlots(Item->GetClass(), Amount, Rotation, EmptySlots, SlotType);
	}
}

void UInventoryComponent::Server_SplitItem_2_Implementation(const int32 Index, ESlotsType SlotType,
                                                            const int32 Split, bool ReverseSplit)
{
	if (!GetSlots(SlotType).IsValidIndex(Index)) return;
	const UItem* Item = GetSlots(SlotType)[Index].ItemReference;
	if (!IsValid(Item)) return;
	const int32 Amount = Item->Amount;
	if (Amount <= 1) return;
	if (Split >= Amount || Split <= 0) return;
	TArray<int32> EmptySlots;
	EItemRotation Rotation;
	SlotType = SlotType == ESlotsType::Equipment ? ESlotsType::Primary : SlotType;
	if (FindEmptySlots(SlotType, Item->Size.Width, Item->Size.Height, Rotation,
	                   EmptySlots))
	{
		if (!ReverseSplit)
		{
			AddItemToSlots(Item->GetClass(), Split, Rotation, EmptySlots, SlotType);
			Server_RemoveItemByRef(Item, Split);
		}
		else
		{
			AddItemToSlots(Item->GetClass(), Amount - Split, Rotation, EmptySlots, SlotType);
			Server_RemoveItemByRef(Item, Amount - Split);
		}
	}
}


void UInventoryComponent::Server_CombineItems_Implementation(UItem* ItemOne, UItem* ItemTwo)
{
	if (!IsValid(ItemOne)) return;
	if (!IsValid(ItemTwo)) return;
	const TSubclassOf<UItem> ItemOneClass = ItemOne->GetClass();
	const TSubclassOf<UItem> ItemTwoClass = ItemTwo->GetClass();

	if (ItemOneClass == ItemTwoClass && ItemOne != ItemTwo)
	{
		if (ItemOne->IsStackable())
		{
			const int32 MaxStack = ItemOne->MaxStack;
			const int32 CanAdd = MaxStack - ItemTwo->Amount;
			if (ItemOne->Amount >= CanAdd)
			{
				SetItemAmount(ItemTwo, ItemTwo->Amount + CanAdd);
				Server_RemoveItemByRef(ItemOne, CanAdd);
				return;
			}
			else
			{
				SetItemAmount(ItemTwo, ItemOne->Amount + ItemTwo->Amount);
				Server_RemoveItemByRef(ItemOne, 1, true);
				return;
			}
		};
	}

	if (!IsValid(CombinesDataTable)) return;

	TArray<FCombinesStruct*> AllRows;
	CombinesDataTable->GetAllRows<FCombinesStruct>(FString(), AllRows);

	for (const FCombinesStruct* Row : AllRows)
	{
		if (Row->FirstItem == ItemOneClass && Row->SecondItem == ItemTwoClass || Row->FirstItem == ItemTwoClass && Row->
			SecondItem == ItemOneClass)
		{
			UItem* ItemToStack;
			int32 CanAdd;

			if (!FindItemToStack(Row->Result, ESlotsType::Primary, CanAdd, ItemToStack))
			{
				const UItem* ClassDefaults = Row->Result.GetDefaultObject();
				if (!IsValid(ClassDefaults)) return;

				TArray<int32> ExcludeSlots;
				if (ItemOne->Amount == 1)
					ExcludeSlots.Append(ItemOne->OccupiedSlots);

				if (ItemTwo->Amount == 1)
					ExcludeSlots.Append(ItemTwo->OccupiedSlots);


				EItemRotation Rotation;
				TArray<int32> EmptySlots;
				if (!FindEmptySlotsAdvantage(GetSlots(ESlotsType::Primary), ItemTwo->SlotsType,
				                             ClassDefaults->Size.Width,
				                             ClassDefaults->Size.Height,
				                             Rotation,
				                             EmptySlots, ExcludeSlots))
				{
					Client_OnCombineResult(false);
					return;
				}
			}

			const ESlotsType SlotsTypeToAdd = ItemTwo->SlotsType;
			if (SlotsTypeToAdd == ESlotsType::Temp || SlotsTypeToAdd == ESlotsType::HiddenSlots)
			{
				Client_OnCombineResult(false);
				return;
			}

			Server_RemoveItemByRef(ItemOne, 1);
			Server_RemoveItemByRef(ItemTwo, 1);
			Server_AddItem_2(Row->Result, Row->Amount, Row->Amount, SlotsTypeToAdd);
			Client_OnCombineResult(true);
			return;
		}
	}
}


void UInventoryComponent::Server_AddItemToShortcut_Implementation(const int32 Index, UItem* Item)
{
	if (!IsValid(Item)) return;


	//Remove the item from the shortcuts if it's already there, or move it to the slot of the item we're adding.
	int32 ShortcutIndex;
	if (IsShortcutItem(Item, ShortcutIndex))
	{
		if (GetShortcuts().IsValidIndex(ShortcutIndex))
		{
			if (GetShortcuts().IsValidIndex(Index) && !GetShortcuts()[Index].IsEmpty && IsValid(
				GetShortcuts()[Index].Item))
			{
				GetShortcuts()[ShortcutIndex].IsEmpty = false;
				GetShortcuts()[ShortcutIndex].Item = GetShortcuts()[Index].Item;
			}
			else
				Server_RemoveItemFromShortcut(ShortcutIndex);
		}
	}
	///


	for (FShortcut& Shortcut : Shortcuts)
	{
		if (Shortcut.Index == Index)
		{
			Shortcut.IsEmpty = false;
			Shortcut.Item = Item;
			OnShortcutsChanged_Server.Broadcast(GetAllShortcutItems());
			return;
		}
	}
}

void UInventoryComponent::Server_RemoveItemFromShortcut_Implementation(const int32 Index)
{
	for (FShortcut& Shortcut : Shortcuts)
	{
		if (Shortcut.Index == Index)
		{
			Shortcut.Item = nullptr;
			Shortcut.IsEmpty = true;
			OnShortcutsChanged_Server.Broadcast(GetAllShortcutItems());
			return;
		}
	}
}


void UInventoryComponent::Server_CheckAndRemoveItemFromShortcuts_Implementation(UItem* Item)
{
	if (!IsValid(Item)) return;
	for (const FShortcut& Shortcut : GetShortcuts())
		if (Shortcut.Item == Item)
			Server_RemoveItemFromShortcut(Shortcut.Index);
}


void UInventoryComponent::ResizeSlots_Implementation(const ESlotsType SlotType, const FSize_isx NewSize)
{
	SetInventorySize(SlotType, NewSize);
	Server_ClearAllSlots(SlotType);

	TArray<FSlotStruct> Slots;
	int32 const Size = NewSize.Height * NewSize.Width;
	for (int32 x = 0; x < Size; x++)
	{
		FSlotStruct Slot;
		Slot.Index = x;
		Slot.IsEmpty = true;
		Slot.IsPartOfItem = false;
		Slot.ItemReference = nullptr;

		Slots.Add(Slot);
	}
	SetSlots(SlotType, Slots);
}


void UInventoryComponent::InitializeStorage_Implementation(UStorageComponent* InStorageComponent)
{
	StorageComponent = InStorageComponent;
	if (IsValid(StorageComponent))
	{
		const TArray<FStorageData> StorageData;
		CheckStorageSlotsTimer(StorageData);
	}
}

void UInventoryComponent::DeInitializeStorage_Implementation()
{
	StorageComponent = nullptr;
}


void UInventoryComponent::CheckStorageSlotsTimer_Implementation(const TArray<FStorageData>& StorageData)
{
	const auto TimerDelegate = FTimerDelegate::CreateLambda([StorageData,this]() { CheckStorageSlots(StorageData); });

	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, 0.1, false);
}

int32 UInventoryComponent::FindItemInShortcuts(UItem* Item)
{
	if (!IsValid(Item)) return -1;
	for (FShortcut const& Shortcut : GetShortcuts())
	{
		if (Shortcut.Item == Item)
			return Shortcut.Index;
	}
	return -1;
}

int32 UInventoryComponent::GetFreeShortcut()
{
	for (FShortcut const& Shortcut : GetShortcuts())
	{
		if (Shortcut.IsEmpty == true)
		{
			return Shortcut.Index;
		}
	}
	return -1;
}

FShortcut UInventoryComponent::GetSelectedShortcutData()
{
	if (GetShortcuts().IsValidIndex(SelectedShortcut))
	{
		return GetShortcuts()[SelectedShortcut];
	}
	return FShortcut();
}

void UInventoryComponent::Server_AddItemAmmo_Implementation(UItem* Item, const int32 AddAmmo)
{
	if (!IsValid(Item)) return;
	Server_SetItemAmmo(Item, Item->Ammo + AddAmmo);
}

void UInventoryComponent::Server_SelectShortcut_Implementation(const int32 Index)
{
	if (!GetShortcuts().IsValidIndex(Index)) return;

	SelectedShortcut = FMath::Clamp(Index, 0, GetShortcuts().Num() - 1);
	Server_UnequipItem();

	Server_OnShortcutSelected.Broadcast(GetShortcuts()[Index].IsEmpty, Index, GetShortcuts()[Index].Item);
}

void UInventoryComponent::CheckStorageSlots(const TArray<FStorageData>& StorageData)
{
	if (!IsValid(StorageComponent)) return;

	TArray<FStorageData> TempStorageData;
	for (FSlotStruct const& Slot : GetSlots(ESlotsType::Storage))
	{
		if (!Slot.IsEmpty && !Slot.IsPartOfItem && Slot.ItemReference)
		{
			UItem* Item = Slot.ItemReference;
			FStorageData Data;

			Data.Item = Item;
			Data.Amount = Item->Amount;
			Data.FirstSlot = Item->OccupiedSlots.IsValidIndex(0) ? Item->OccupiedSlots[0] : -1;
			Data.Rotation = Item->Rotation;
			TempStorageData.Add(Data);
		}
	}
	if (TempStorageData.Num() != StorageData.Num())
	{
		OnUpdateStorageSlots.Broadcast(GetSlots(ESlotsType::Storage));
		CheckStorageSlotsTimer(TempStorageData);
		return;
	}
	for (const auto& TempData : TempStorageData)
	{
		bool Contains = false;
		for (FStorageData const& Data : StorageData)
		{
			if (Data.Item == TempData.Item && Data.Amount == TempData.Amount && Data.FirstSlot == TempData.FirstSlot &&
				Data.Rotation == TempData.Rotation)
			{
				Contains = true;
			}
		}
		if (!Contains)
		{
			OnUpdateStorageSlots.Broadcast(GetSlots(ESlotsType::Storage));
			CheckStorageSlotsTimer(TempStorageData);
			return;
		}
	}
	CheckStorageSlotsTimer(TempStorageData);
}

void UInventoryComponent::Server_AutoSort_Implementation()
{
	if (IsSlotsHaveItems(ESlotsType::Temp)) return;
	TArray<UItem*> Items;
	for (FSlotStruct const& Slot : GetSlots(ESlotsType::Primary))
	{
		if (!Slot.IsEmpty && !Slot.IsPartOfItem && IsValid(Slot.ItemReference))
		{
			Items.Add(Slot.ItemReference);
		}
	}

	TArray<UItem*> RemovedItems;
	for (UItem* const& Item_1 : Items)
	{
		for (UItem* const& Item_2 : Items)
		{
			if (Item_1->GetClass() != Item_2->GetClass()) continue;
			if (Item_1 == Item_2 || !Item_1->IsStackable()) continue;
			const int32 MaxStack = Item_1->MaxStack;
			if (Item_1->Amount >= MaxStack) continue;
			const int32 CanAdd = MaxStack - Item_1->Amount;
			if (Item_2->Amount >= CanAdd)
			{
				Item_1->Amount += CanAdd;
				Item_2->Amount -= CanAdd;
			}
			else
			{
				Item_1->Amount += Item_2->Amount;
				Item_2->Amount = 0;
			}

			if (Item_2->GetAmount() <= 0)
			{
				RemovedItems.Add(Item_2);
				const int32 ShortcutIndex = FindItemInShortcuts(Item_2);
				if (Shortcuts.IsValidIndex(ShortcutIndex))
				{
					if (FindItemInShortcuts(Item_1) == INDEX_NONE)
					{
						Shortcuts[ShortcutIndex].Item = Item_1;
					}
					else
					{
						Server_RemoveItemFromShortcut(ShortcutIndex);
					}
				}
			}
		}
	}

	for (UItem* const& RemovedItem : RemovedItems)
	{
		Items.Remove(RemovedItem);
	}

	Items.Sort([](const UItem& A, const UItem& B)
	{
		return (A.Size.Height * A.Size.Width) > (B.Size.Height * B.Size.Width);
	});

	Server_ClearAllSlots(ESlotsType::Primary);

	for (UItem* const& Item : Items)
	{
		if (!IsValid(Item)) continue;
		if (Item->Amount <= 0) continue;

		TArray<int32> EmptySlots;
		EItemRotation Rotation;
		if (!FindEmptySlots(ESlotsType::Primary, Item->Size.Width, Item->Size.Height, Rotation,
		                    EmptySlots))
			continue;
		AddExistingItemToSlots(Item, ESlotsType::Primary, Rotation, EmptySlots);
	}
}

void UInventoryComponent::Server_SetItemAmmo_Implementation(UItem* Item, const int32 Ammo)
{
	if (!IsValid(Item)) return;
	Item->SetAmmo(Ammo);
	Client_OnAmmoChanged(Item, FMath::Clamp(Ammo, 0, Item->MaxStack));
}

void UInventoryComponent::Server_AddSelectedShortcutAmount_Implementation(const int32 AddAmount)
{
	if (!GetShortcuts().IsValidIndex(GetSelectedShortcutIndex())) return;
	UItem* ShortcutItem = GetShortcuts()[GetSelectedShortcutIndex()].Item;
	if (!IsValid(ShortcutItem)) return;
	if (ShortcutItem->GetItemType() == EItemType::Weapon)
		Server_AddItemAmmo(ShortcutItem, AddAmount);
	else
		SetItemAmount(ShortcutItem, ShortcutItem->GetAmount() + AddAmount);
}

void UInventoryComponent::Server_SetSelectedShortcutAmount_Implementation(const int32 NewAmount)
{
	if (!GetShortcuts().IsValidIndex(GetSelectedShortcutIndex())) return;
	UItem* ShortcutItem = GetShortcuts()[GetSelectedShortcutIndex()].Item;
	if (!IsValid(ShortcutItem)) return;
	if (ShortcutItem->GetItemType() == EItemType::Weapon)
		Server_SetItemAmmo(ShortcutItem, NewAmount);
	else
		SetItemAmount(ShortcutItem, NewAmount);
}

bool UInventoryComponent::IsShortcutItem(UItem* Item, int32& ShortcutIndex)
{
	if (!IsValid(Item))
	{
		ShortcutIndex = -1;
		return false;
	}

	for (FShortcut const& Shortcut : GetShortcuts())
	{
		if (Shortcut.IsEmpty) continue;
		if (!IsValid(Shortcut.Item)) continue;
		if (Shortcut.Item == Item)
		{
			ShortcutIndex = Shortcut.Index;
			return true;
		}
	}
	ShortcutIndex = -1;
	return false;
}

void UInventoryComponent::Server_UnequipItem_Implementation()
{
	if (!IsValid(EquippedItem)) return;
	UItem* UnequippedItem = EquippedItem;
	EquippedItem = nullptr;
	OnEquipServer.Broadcast(false, UnequippedItem);
	Client_OnEquip(false, nullptr);
}

void UInventoryComponent::Server_EquipItem_Implementation(UItem* Item)
{
	if (!IsValid(Item)) return;
	EquippedItem = Item;
	OnEquipServer.Broadcast(true, Item);
	Client_OnEquip(true, Item);
}


void UInventoryComponent::Server_UnequipItem_2_Implementation(UItem* Item)
{
	if (!IsValid(Item)) return;
	if (GetEquippedItem() == Item)
	{
		Server_UnequipItem();
	}
}

bool UInventoryComponent::IsEquippedItem(UItem* Item) const
{
	if (Item && GetEquippedItem() == Item) return true;
	return false;
}

bool UInventoryComponent::HasEquippedItem() const
{
	if (IsValid(GetEquippedItem())) return true;
	return false;
}

int32 UInventoryComponent::FindEquipmentSlotToEquip(const int32 SlotIndex, const ESlotsType SlotType)
{
	UItem* Item;
	GetItemInSlot(SlotIndex, SlotType, Item);
	if (!IsValid(Item)) return -1;
	bool CanEquip;
	EEquipmentSlotType_Isx EquipmentType;
	EItemRotation ItemRotation;
	Item->CanEquip(this, GetInventoryOwnerPawn(), CanEquip, EquipmentType, ItemRotation);
	if (!CanEquip) return -1;

	TArray<FSlotStruct> SlotsToEquip;
	for (FSlotStruct const& Slot : GetSlots(ESlotsType::Equipment))
	{
		if (Slot.EquipmentSlotType == EquipmentType)
		{
			SlotsToEquip.Add(Slot);
		}
	}
	if (SlotsToEquip.Num() == 0) return -1;
	if (SlotsToEquip.Num() == 1 && SlotsToEquip.IsValidIndex(0)) return SlotsToEquip[0].Index;

	TArray<UItem*> Items;

	for (FSlotStruct const& ToEquip : SlotsToEquip)
	{
		if (ToEquip.IsEmpty) return ToEquip.Index;
		Items.Add(ToEquip.ItemReference);
	}


	if (GetPlayerObject() && PlayerObject->GetClass()->ImplementsInterface(
		UInventorySystemInterface::StaticClass()))
	{
		UItem* ItemToReplace;
		IInventorySystemInterface::Execute_PreEquip(GetPlayerObject(), this, Items, ItemToReplace);
		if (IsValid(ItemToReplace))
		{
			for (FSlotStruct const& ToEquip : SlotsToEquip)
			{
				if (ToEquip.ItemReference == ItemToReplace)
					return ToEquip.Index;
			}
		}
	}


	if (SlotsToEquip.IsValidIndex(0)) return SlotsToEquip[0].Index;

	return -1;
}

void UInventoryComponent::AddItemToEquipment_Implementation(TSubclassOf<UItem> ItemClass, int32 Amount,
                                                            const int32 Ammo)
{
	if (!IsValid(ItemClass)) return;

	int32 CanAdd;
	UItem* Item;
	while (FindItemToStack(ItemClass, ESlotsType::Equipment, CanAdd, Item))
	{
		if (!IsValid(Item)) break;

		if (Amount > Item->GetAmount() + CanAdd)
		{
			SetItemAmount(Item, Item->MaxStack);
			Amount = Amount - CanAdd;
		}
		else
		{
			SetItemAmount(Item, Item->GetAmount() + Amount);
			Amount = 0;
		}
		if (Amount <= 0) return;;
	}

	if (Amount <= 0) return;

	const UItem* ItemDefaults = ItemClass.GetDefaultObject();
	if (!ItemDefaults) return;
	const int32 MaxStack = ItemDefaults->MaxStack;
	EItemRotation Rotation;
	while (Amount > 0)
	{
		const int32 FreeSlot = FindFreeEquipmentSlot(ItemClass, Rotation);
		if (FreeSlot == INDEX_NONE) return;
		AddItemToSlots(ItemClass, Amount, Rotation, {FreeSlot}, ESlotsType::Equipment, Ammo);
		Amount = Amount - MaxStack;
		FSlotStruct SlotInfo;
		GetSlotInfo(FreeSlot, ESlotsType::Equipment, SlotInfo);
		OnEquipmentSlotChanged_Server.Broadcast(true, FreeSlot, SlotInfo.ItemReference);
		if (Amount <= 0) return;
	}
}

int32 UInventoryComponent::FindFreeEquipmentSlot(TSubclassOf<UItem> ItemClass, EItemRotation& Rotation)
{
	const UItem* ItemDefaults = ItemClass.GetDefaultObject();
	bool CanEquip;
	EEquipmentSlotType_Isx EquipmentType;

	ItemDefaults->CanEquip(this, GetInventoryOwnerPawn(), CanEquip, EquipmentType, Rotation);
	if (!CanEquip) return -1;

	TArray<FSlotStruct> SlotsToEquip;
	for (FSlotStruct const& Slot : GetSlots(ESlotsType::Equipment))
	{
		if (Slot.EquipmentSlotType == EquipmentType)
		{
			SlotsToEquip.Add(Slot);
		}
	}
	for (FSlotStruct const& ToEquip : SlotsToEquip)
	{
		if (ToEquip.IsEmpty) return ToEquip.Index;
	}

	return -1;
}

int32 UInventoryComponent::FindItemInEquipmentSlots(UItem* Item)
{
	if (!IsValid(Item)) return -1;
	for (auto const& Slot : GetSlots(ESlotsType::Equipment))
	{
		if (Slot.IsEmpty || Slot.IsPartOfItem) continue;

		if (Slot.ItemReference == Item) return Slot.Index;
	}

	return -1;
}


void UInventoryComponent::EquipEquipmentItem_Implementation(const int32 Index, const ESlotsType SlotsType)
{
	//UnEquip
	if (SlotsType == ESlotsType::Equipment)
	{
		FSlotStruct SlotInfo;
		if (GetSlotInfo(Index, SlotsType, SlotInfo) && IsValid(SlotInfo.ItemReference))
		{
			TArray<int32> EmptySlots;
			EItemRotation Rotation;
			if (FindEmptySlots(ESlotsType::Primary, SlotInfo.ItemReference->Size.Width,
			                   SlotInfo.ItemReference->Size.Height,
			                   Rotation, EmptySlots))
			{
				ClearSlots(SlotsType, SlotInfo.ItemReference->OccupiedSlots);
				AddExistingItemToSlots(SlotInfo.ItemReference, ESlotsType::Primary, Rotation, EmptySlots);
			}
		}
		return;
	}

	const int32 EquipmentSlot = FindEquipmentSlotToEquip(Index, SlotsType);
	if (EquipmentSlot == INDEX_NONE) return;
	FSlotStruct SlotInfo;
	if (!GetSlotInfo(EquipmentSlot, ESlotsType::Equipment, SlotInfo)) return;
	UItem* ItemInEquipmentSlot = SlotInfo.ItemReference;

	UItem* Item;
	GetItemInSlot(Index, SlotsType, Item);

	if (CanSwapEquipmentItem(Index, SlotsType, EquipmentSlot, ESlotsType::Equipment))
	{
		Server_CheckAndRemoveItemFromShortcuts(Item);
		Server_SwapSameSizeItems(Index, SlotsType, EquipmentSlot, ESlotsType::Equipment);

		OnEquipmentSlotChanged_Server.Broadcast(false, SlotInfo.Index, ItemInEquipmentSlot);
		OnEquipmentSlotChanged_Server.Broadcast(true, SlotInfo.Index, Item);
		return;
	}


	AddItemToEquipmentSlot(EquipmentSlot, Index, SlotsType);
	if (!IsValid(ItemInEquipmentSlot)) return;

	TArray<int32> EmptySlots;
	EItemRotation Rotation;
	if (FindEmptySlots(ESlotsType::Primary, ItemInEquipmentSlot->Size.Width, ItemInEquipmentSlot->Size.Height,
	                   Rotation, EmptySlots))
	{
		AddExistingItemToSlots(ItemInEquipmentSlot, ESlotsType::Primary, Rotation, EmptySlots);
		return;
	}


	OnDiscardItem_Server.Broadcast(ItemInEquipmentSlot->GetClass(), ItemInEquipmentSlot->Amount,
	                               ItemInEquipmentSlot->Ammo, ESlotsType::Equipment);
}

void UInventoryComponent::AddItemToEquipmentSlot_2_Implementation(const int32 EquipmentSlotIndex, UItem* Item)
{
	if (!IsValid(Item)) return;

	bool CanEquip;
	EEquipmentSlotType_Isx EquipmentType;
	EItemRotation Rotation;

	Item->CanEquip(this, GetInventoryOwnerPawn(), CanEquip, EquipmentType, Rotation);
	if (!CanEquip) return;


	if (GetSlots(ESlotsType::Equipment).IsValidIndex(EquipmentSlotIndex) && (GetSlots(ESlotsType::Equipment)[
		EquipmentSlotIndex].EquipmentSlotType == EquipmentType))
	{
		Server_CheckAndRemoveItemFromShortcuts(Item);

		ClearSlots(Item->SlotsType, Item->OccupiedSlots);

		Item->SlotsType = ESlotsType::Equipment;
		Item->OccupiedSlots = TArray<int32>{EquipmentSlotIndex};
		Item->Rotation = Rotation;
		GetSlots(ESlotsType::Equipment)[EquipmentSlotIndex].IsEmpty = false;
		GetSlots(ESlotsType::Equipment)[EquipmentSlotIndex].ItemReference = Item;
		Client_OnItemAdded(ESlotsType::Equipment, Item->GetClass(), TArray<int32>{EquipmentSlotIndex},
		                   Item->GetAmmoOrAmount(), Rotation, true, FindItemInShortcuts(Item), false);

		OnEquipmentSlotChanged_Server.Broadcast(true, EquipmentSlotIndex, Item);
	}
}


void UInventoryComponent::AddItemToEquipmentSlot_Implementation(const int32 EquipmentSlotIndex, const int32 Index,
                                                                const ESlotsType SlotsType)
{
	UItem* Item;
	if (!GetItemInSlot(Index, SlotsType, Item)) return;
	if (!IsValid(Item)) return;

	AddItemToEquipmentSlot_2(EquipmentSlotIndex, Item);
}

void UInventoryComponent::AddNewItemToEquipmentSlot_Implementation(TSubclassOf<UItem> ItemClass, const int32 Amount,
                                                                   const int32 Ammo)
{
	if (!IsValid(ItemClass)) return;

	EItemRotation Rotation;
	const int32 FreeSlot = FindFreeEquipmentSlot(ItemClass, Rotation);
	if (FreeSlot == INDEX_NONE) return;
	const TArray<int32> Slots = {FreeSlot};
	AddItemToSlots(ItemClass, Amount, Rotation, Slots, ESlotsType::Equipment, Ammo);
	FSlotStruct SlotInfo;
	GetSlotInfo(FreeSlot, ESlotsType::Equipment, SlotInfo);
	OnEquipmentSlotChanged_Server.Broadcast(true, FreeSlot, SlotInfo.ItemReference);
}

APawn* UInventoryComponent::GetInventoryOwnerPawn()
{
	if (IsValid(GetPlayerObject()))
	{
		return Cast<APawn>(GetPlayerObject());
	}
	return nullptr;
}

int32 UInventoryComponent::FindItemInInventory(UItem* Item)
{
	if (!IsValid(Item)) return -1;
	for (FSlotStruct const& Slot : GetSlots(Item->SlotsType))
	{
		if (Slot.IsEmpty && Slot.IsPartOfItem) continue;
		if (Item == Slot.ItemReference) return Slot.Index;
	}
	return -1;
}

TArray<UItem*> UInventoryComponent::GetAllItems(const ESlotsType SlotsType)
{
	TArray<UItem*> Items;
	for (FSlotStruct const& Slot : GetSlots(SlotsType))
	{
		if (Slot.IsEmpty || Slot.IsPartOfItem) continue;
		if (!IsValid(Slot.ItemReference) && Slot.ItemReference->SlotsType == SlotsType) continue;
		Items.Add(Slot.ItemReference);
	}
	return Items;
}

TArray<UItem*> UInventoryComponent::GetAllItemsOfClass(const TSubclassOf<UItem> ItemClass)
{
	TArray<UItem*> Items;
	for (FSlotStruct const& Slot : GetSlots(ESlotsType::Primary))
	{
		if (Slot.IsEmpty || Slot.IsPartOfItem) continue;
		if (IsValid(Slot.ItemReference) && Slot.ItemReference->GetClass() == ItemClass)
			Items.Add(Slot.ItemReference);
	}
	return Items;
}

int32 UInventoryComponent::GetNumberOfItems(const TSubclassOf<UItem> ItemClass)
{
	if (!ItemClass) return 0;
	int32 Number = 0;
	for (UItem* const& Item : GetAllItemsOfClass(ItemClass))
	{
		if (!IsValid(Item)) continue;
		Number += Item->Amount;
	}
	return Number;
}

void UInventoryComponent::Server_AddExistingItem_Implementation(UItem* Item, const ESlotsType SlotType)
{
	if (!IsValid(Item)) return; //false;
	if (Item->Amount <= 0) return; //false;

	const TSubclassOf<UItem> ItemClass = Item->GetClass();
	if (!ItemClass) return;

	const UItem* ItemObject = ItemClass.GetDefaultObject();
	if (!ItemObject) return; // false;

	const int32 MaxStack = ItemObject->MaxStack;

	if (MaxStack > 1 && ItemObject->IsStackable())
	{
		int32 CanAdd;
		UItem* FoundItem;

		while (FindItemToStack(ItemClass, SlotType, CanAdd, FoundItem))
		{
			if (!Item) break; // false;
			if (!FoundItem) break;

			if (CanAdd >= Item->Amount)
			{
				SetItemAmount(FoundItem, Item->Amount + Item->Amount);
				return; // true;
			}
			SetItemAmount(FoundItem, Item->Amount + CanAdd);
			if (!IsValid(FoundItem)) break;
			Item->Amount -= CanAdd;

			if (Item->Amount <= 0) return; // true;
		}
	}

	if (!IsValid(Item)) return;

	TArray<int32> EmptySlots;
	EItemRotation Rotation;

	if (FindEmptySlots(SlotType, ItemObject->Size.Width, ItemObject->Size.Height, Rotation,
	                   EmptySlots))
	{
		AddExistingItemToSlots(Item, SlotType, Rotation, EmptySlots);
	}
}

void UInventoryComponent::Server_RemoveItemsOfClass_Implementation(TSubclassOf<UItem> ItemClass, int32 Amount)
{
	if (ItemClass == nullptr) return;
	if (Amount <= 0) return;

	for (UItem* const& Item : GetAllItemsOfClass(ItemClass))
	{
		if (!IsValid(Item)) continue;
		if (Amount > Item->GetAmount())
		{
			Amount -= Item->GetAmount();
			Server_RemoveItemByRef(Item, Amount, true);
		}
		else
		{
			Server_RemoveItemByRef(Item, Amount);
			return;
		}
	}
}

void UInventoryComponent::Server_ReloadWeapon_Implementation(UItem* Weapon)
{
	if (!IsValid(Weapon)) return;
	if (!Weapon->IsWeapon()) return;

	const int32 CurrentAmmo = Weapon->GetAmmo();
	const int32 MaxAmmo = Weapon->GetMaxAmmo();

	const TSubclassOf<UItem> AmmoClass = Weapon->GetAmmoClass();
	if (AmmoClass == nullptr) return;
	const int32 CanAdd = GetNumberOfItems(AmmoClass);
	const int32 NeedAmmo = MaxAmmo - CurrentAmmo;
	if (CanAdd <= 0) return;

	if (CanAdd >= NeedAmmo)
	{
		Server_RemoveItemsOfClass(AmmoClass, NeedAmmo);
		Server_SetItemAmmo(Weapon, MaxAmmo);
		return;
	}
	else
	{
		Server_RemoveItemsOfClass(AmmoClass, CanAdd);
		Server_SetItemAmmo(Weapon, CanAdd + CurrentAmmo);
	}
}

void UInventoryComponent::Server_ResizeInventory_Implementation(const FSize_isx NewSize)
{
	LoadPrimarySlotsFromArray();
	Server_AutoSort();
	const TArray<UItem*> AllItems = GetAllItems(ESlotsType::Primary);

	Server_ResizeSlots(ESlotsType::Primary, NewSize);

	TArray<UItem*> CantRemoveItems;
	for (UItem* const& Item : AllItems)
	{
		if (!IsValid(Item)) continue;
		if (!Item->CanDestroy(this, GetInventoryOwnerPawn()))
		{
			CantRemoveItems.Add(Item);
		}
	}

	for (UItem* const& Item : CantRemoveItems)
	{
		if (!IsValid(Item)) continue;

		TArray<int32> EmptySlots;
		EItemRotation Rotation;
		if (FindEmptySlots(ESlotsType::Primary, Item->Size.Width, Item->Size.Height, Rotation,
		                   EmptySlots))
		{
			AddExistingItemToSlots(Item, ESlotsType::Primary, Rotation, EmptySlots);
		}
		else
		{
			Server_UnequipItem_2(Item);
			Server_CheckAndRemoveItemFromShortcuts(Item);
			OnDiscardItem_Server.Broadcast(Item->GetClass(), Item->Amount, Item->Ammo, ESlotsType::Primary);
		}
	}

	for (UItem* const& Item : AllItems)
	{
		if (!IsValid(Item)) continue;
		if (CantRemoveItems.Contains(Item)) continue;
		TArray<int32> EmptySlots;
		EItemRotation Rotation;
		if (FindEmptySlots(ESlotsType::Primary, Item->Size.Width, Item->Size.Height, Rotation,
		                   EmptySlots))
		{
			AddExistingItemToSlots(Item, ESlotsType::Primary, Rotation, EmptySlots);
		}
		else
		{
			Server_UnequipItem_2(Item);
			Server_CheckAndRemoveItemFromShortcuts(Item);
			OnDiscardItem_Server.Broadcast(Item->GetClass(), Item->Amount, Item->Ammo, ESlotsType::Primary);
		}
	}

	Client_OnResizeInventory(NewSize);
}

void UInventoryComponent::Server_ResizeInventoryFromItem_Implementation(const FSize_isx NewSize, UItem* Resizer)
{
	if (IsValid(Resizer))
	{
		Server_RemoveItemByRef(Resizer, 1, false);
	}
	Server_ResizeInventory(NewSize);
}

void UInventoryComponent::Server_ResizeSlots_Implementation(const ESlotsType SlotsType, const FSize_isx NewSize)
{
	GetSlots(SlotsType).Empty();


	int32 const Size = NewSize.Height * NewSize.Width;
	for (int32 x = 0; x < Size; x++)
	{
		FSlotStruct Slot;
		Slot.Index = x;
		Slot.IsEmpty = true;
		Slot.IsPartOfItem = false;
		Slot.ItemReference = nullptr;

		GetSlots(SlotsType).Add(Slot);
	}

	GetInventorySize(SlotsType) = NewSize;
}

FInventorySaveData UInventoryComponent::GetSaveData()
{
	LoadPrimarySlotsFromArray();
	FInventorySaveData InventorySaveData;
	for (FSlotStruct const& Slot : GetSlots(ESlotsType::Primary))
	{
		if (Slot.IsEmpty || Slot.IsPartOfItem) continue;
		if (!IsValid(Slot.ItemReference)) continue;

		UItem* Item = Slot.ItemReference;
		FSaveDataWithShortcuts SaveData;
		SaveData.ItemClass = Item->GetClass();
		SaveData.OccupiedSlots = Item->OccupiedSlots;
		SaveData.SlotsType = Item->SlotsType;
		SaveData.Rotation = Item->Rotation;
		SaveData.Amount = Item->GetAmount();
		SaveData.Ammo = Item->GetAmmo();
		SaveData.InShortcutSlot = FindItemInShortcuts(Item);


		InventorySaveData.SaveData.Add(SaveData);
	}

	for (FSlotStruct const& Slot : GetSlots(ESlotsType::Equipment))
	{
		if (Slot.IsEmpty || Slot.IsPartOfItem) continue;
		if (!IsValid(Slot.ItemReference)) continue;

		UItem* Item = Slot.ItemReference;
		FSaveDataWithShortcuts SaveData;
		SaveData.ItemClass = Item->GetClass();
		SaveData.OccupiedSlots = Item->OccupiedSlots;
		SaveData.SlotsType = Item->SlotsType;
		SaveData.Rotation = Item->Rotation;
		SaveData.Amount = Item->GetAmount();
		SaveData.Ammo = Item->GetAmmo();
		SaveData.InShortcutSlot = FindItemInShortcuts(Item);

		InventorySaveData.SaveData.Add(SaveData);
	}

	InventorySaveData.InventorySize = GetInventorySize(ESlotsType::Primary);
	InventorySaveData.EquippedItemSlot = IsValid(GetEquippedItem()) ? GetEquippedItem()->GetSlotIndex() : -1;
	InventorySaveData.UniqueID = UniqueID;
	InventorySaveData.SelectedShortcut = GetSelectedShortcutIndex();
	return InventorySaveData;
}

void UInventoryComponent::Server_LoadInventoryFromSave_Implementation(const FInventorySaveData& SaveData)
{
	Server_ClearAllSlots(ESlotsType::Primary);
	Server_ClearAllSlots(ESlotsType::Temp);
	Server_ClearAllSlots(ESlotsType::HiddenSlots);
	HiddenItemSlotsType = ESlotsType::Primary;
	SavedPrimaryItemsArray.Empty();

	Server_ResizeSlots(ESlotsType::Primary, SaveData.InventorySize);


	//Clear Shortcuts
	for (FShortcut& Shortcut : GetShortcuts())
	{
		Shortcut.Item = nullptr;
		Shortcut.IsEmpty = true;
	}

	for (FSaveDataWithShortcuts const& Data : SaveData.SaveData)
	{
		//Add Item
		AddItemToSlots(Data.ItemClass, Data.Amount, Data.Rotation, Data.OccupiedSlots, Data.SlotsType, Data.Ammo);

		//Adding an item to the shortcut without calling the delegate
		if (Data.InShortcutSlot == -1 && !GetShortcuts().IsValidIndex(Data.InShortcutSlot)) continue;
		if (!Data.OccupiedSlots.IsValidIndex(0)) continue;
		if (!GetSlots(ESlotsType::Primary).IsValidIndex(Data.OccupiedSlots[0])) continue;

		UItem* Item = GetSlots(ESlotsType::Primary)[Data.OccupiedSlots[0]].ItemReference;
		if (IsValid(Item))
		{
			GetShortcuts()[Data.InShortcutSlot].IsEmpty = false;
			GetShortcuts()[Data.InShortcutSlot].Item = Item;
		}
	}

	//Set Equipped Item
	if (SaveData.EquippedItemSlot != -1)
	{
		FSlotStruct SlotInfo;
		if (GetSlotInfo(SaveData.EquippedItemSlot, ESlotsType::Primary, SlotInfo))
		{
			if (SlotInfo.ItemReference)
				EquippedItem = SlotInfo.ItemReference;
		}
	}


	//Now I select the active shortcut or Equipped Item and call the delegate
	if (EquippedItem)
		Server_EquipItem(EquippedItem);
	else
		Server_SelectShortcut(SaveData.SelectedShortcut);

	Client_OnResizeInventory(SaveData.InventorySize);
	Client_OnLoad(SaveData.SelectedShortcut);
}


int32 UInventoryComponent::CalculateItemAmountAfterStuck(const int32 SelectedSlotIndex,
                                                         const ESlotsType SelectedSlotType,
                                                         const int32 DraggedItemSlotIndex,
                                                         const ESlotsType DraggedItemSlotType,
                                                         int32& SelectedItemAmount)
{
	SelectedItemAmount = 0;
	UItem* SelectedItem;
	if (!GetItemInSlot(SelectedSlotIndex, SelectedSlotType, SelectedItem)) return 0;

	UItem* DraggedItem;
	if (!GetItemInSlot(DraggedItemSlotIndex, DraggedItemSlotType, DraggedItem)) return 0;

	if (!IsValid(SelectedItem) || !IsValid(DraggedItem)) return 0;

	if (SelectedItem->GetClass() == DraggedItem->GetClass() && SelectedItem != DraggedItem)
	{
		if (SelectedItem->IsStackable())
		{
			const int32 MaxStack = SelectedItem->MaxStack;
			const int32 CanAdd = MaxStack - SelectedItem->Amount;
			const int32 DraggedItemAmount = DraggedItem->Amount;
			if (DraggedItemAmount >= CanAdd)
			{
				SelectedItemAmount = SelectedItem->Amount + CanAdd;
				return DraggedItemAmount - CanAdd;
			}
			else
			{
				SelectedItemAmount = SelectedItem->Amount + DraggedItem->Amount;
				return 0;
			}
		};
	}
	return 0;
}

TArray<UItem*> UInventoryComponent::GetAllItemsInSlots(TArray<int32> Slots, ESlotsType SlotsType)
{
	TArray<UItem*> ItemsInSlots;
	for (int32 const& Slot : Slots)
	{
		if (!GetSlots(SlotsType).IsValidIndex(Slot)) continue;
		if (GetSlots(SlotsType)[Slot].IsEmpty) continue;

		UItem* Item = GetSlots(SlotsType)[Slot].ItemReference;

		if (IsValid(Item) && (ItemsInSlots.Find(Item) == -1))
		{
			ItemsInSlots.Add(Item);
		}
	}

	return ItemsInSlots;
}

TArray<int32> UInventoryComponent::GetSlotsByItemSize(const int32 FistSlot, const ESlotsType SlotsType,
                                                      const FSize_isx ItemSize, const EItemRotation Rotation)
{
	TArray<int32> EmptySlots;
	const int32 Height = Rotation == EItemRotation::Horizontal ? ItemSize.Height : ItemSize.Width;
	const int32 Width = Rotation == EItemRotation::Horizontal ? ItemSize.Width : ItemSize.Height;

	if (!CheckSize(FistSlot, Width, Height, SlotsType)) return EmptySlots;

	for (int i = 0; i < Height; ++i)
	{
		const int32 TempIndex = FistSlot + (i == 0 ? 0 : GetInventorySize(SlotsType).Width * i);

		for (int x = TempIndex; x < TempIndex + Width; ++x)
		{
			if (GetSlots(SlotsType).IsValidIndex(x))
			{
				EmptySlots.Add(x);
			}
		}
	}
	return EmptySlots;
}

bool UInventoryComponent::CanSwapDraggedItem(const int32 ItemToIgnoreSlotIndex, const ESlotsType ItemToIgnoreSlotsType,
                                             const int32 SelectedIndex, const ESlotsType SelectedSlotsType,
                                             TSubclassOf<UItem> DraggedItemClass,
                                             const EItemRotation DraggedItemRotation)
{
	if (!DraggedItemClass) return false;
	//if ((SelectedSlotsType == ESlotsType::Storage) || (ItemToIgnoreSlotsType == ESlotsType::Storage)) return false;

	if (SelectedSlotsType == ESlotsType::Equipment)
	{
		FSlotStruct SlotInfo;
		if (!GetSlotInfo(SelectedIndex, SelectedSlotsType, SlotInfo)) return false;
		if (SlotInfo.IsEmpty || !SlotInfo.ItemReference) return false;
		UItem* DraggedItem;
		if (!GetItemInSlot(ItemToIgnoreSlotIndex, ItemToIgnoreSlotsType, DraggedItem
		))
			return false;
		if (!IsValid(DraggedItem)) return false;

		if (DraggedItem == SlotInfo.ItemReference) return false;
		bool CanEquip;
		EEquipmentSlotType_Isx EquipmentType;
		EItemRotation ItemRotation;
		DraggedItem->CanEquip(this, GetInventoryOwnerPawn(), CanEquip, EquipmentType, ItemRotation);
		if (!CanEquip) return false;
		return true;
	}

	if (IsValidSwappedItem() && (ItemToIgnoreSlotsType != ESlotsType::HiddenSlots)) return false;

	const UItem* ItemDefaults = DraggedItemClass.GetDefaultObject();
	if (!ItemDefaults) return false;
	const FSize_isx DraggedItemSize = ItemDefaults->Size;

	const TArray<int32> Slots = GetSlotsByItemSize(SelectedIndex, SelectedSlotsType, DraggedItemSize,
	                                               DraggedItemRotation);
	if (Slots.Num() != (DraggedItemSize.Height * DraggedItemSize.Width)) return false;
	TArray<UItem*> Items = GetAllItemsInSlots(Slots, SelectedSlotsType);


	UItem* ItemInSlots;
	if (GetItemInSlot(ItemToIgnoreSlotIndex, ItemToIgnoreSlotsType, ItemInSlots) && !(SelectedSlotsType ==
			ESlotsType::Primary || SelectedSlotsType == ESlotsType::Temp) &&
		!ItemInSlots->CanDestroy(this, GetInventoryOwnerPawn()))
	{
		return false;
	}


	if (Items.Num() != 1) // || !Items.IsValidIndex(0)
	{
		if (IsValidSwappedItem()) return false;
		if ((SelectedSlotsType == ItemToIgnoreSlotsType) &&
			GetItemInSlot(ItemToIgnoreSlotIndex, ItemToIgnoreSlotsType, ItemInSlots))
		{
			const int32 ItemIndex = Items.Find(ItemInSlots);
			if (ItemIndex == -1) return false;
			Items.RemoveAt(ItemIndex);
		}
		if (Items.Num() != 1) return false;
	}
	if (!Items.IsValidIndex(0)) return false;

	if (GetSlots(ESlotsType::HiddenSlots).IsValidIndex(0))
	{
		if (GetSlots(ESlotsType::HiddenSlots)[0].ItemReference == Items[0]) return false;
	}

	if (GetItemInSlot(ItemToIgnoreSlotIndex, ItemToIgnoreSlotsType, ItemInSlots))
	{
		if (Items[0] == ItemInSlots) return false;
	}


	UItem* Item = Items[0];
	if (!IsValid(Item)) return false;

	if (!Item->CanDestroy(this, GetInventoryOwnerPawn()))
	{
		//if (SelectedSlotsType == ESlotsType::Storage ||  ItemToIgnoreSlotsType == ESlotsType::Storage)
		//{
		if (!IsValidSwappedItem())
		{
			if (SelectedSlotsType == ESlotsType::Storage || ItemToIgnoreSlotsType == ESlotsType::Storage)
				return false;
		}
		else if (HiddenItemSlotsType == ESlotsType::Storage || Item->SlotsType == ESlotsType::Storage)
		{
			return false;
		}
		//	}
	}

	return true;
}

void UInventoryComponent::Server_SwapDraggedItem_Implementation(const int32 ItemIndex, const ESlotsType SlotsType,
                                                                TSubclassOf<UItem> ItemClass,
                                                                const int32 SelectedIndex,
                                                                const ESlotsType SelectedSlotsType,
                                                                const EItemRotation DraggedItemRotation)
{
	const UItem* ItemDefaults = ItemClass.GetDefaultObject();
	if (!ItemDefaults)
	{
		Client_OnDraggedItemSwapped(false);
		return;
	}
	const FSize_isx ItemSize = ItemDefaults->Size;

	UItem* DraggedItem;
	if (!GetItemInSlot(ItemIndex, SlotsType, DraggedItem))
	{
		Client_OnDraggedItemSwapped(false);
		return;
	}

	if (!CanSwapDraggedItem(ItemIndex, SlotsType, SelectedIndex, SelectedSlotsType,
	                        ItemClass, DraggedItemRotation))
	{
		Client_OnDraggedItemSwapped(false);
		return;
	}


	TArray<int32> Slots = GetSlotsByItemSize(SelectedIndex, SelectedSlotsType, ItemSize, DraggedItemRotation);
	if (SelectedSlotsType != ESlotsType::Equipment)
	{
		if (Slots.Num() != (ItemSize.Height * ItemSize.Width))
		{
			Client_OnDraggedItemSwapped(false);
			return;
		}
	}


	TArray<UItem*> Items = GetAllItemsInSlots(Slots, SelectedSlotsType);

	if (SelectedSlotsType != ESlotsType::Equipment)
	{
		if (Items.Num() != 1 || !Items.IsValidIndex(0))
		{
			if (DraggedItem->SlotsType == SlotsType)
			{
				const int32 ArrayItemIndex = Items.Find(DraggedItem);
				if (ArrayItemIndex != -1)
				{
					Items.RemoveAt(ArrayItemIndex);
				}
				if (Items.Num() != 1)
				{
					Client_OnDraggedItemSwapped(false);
					return;
				}
			}
			else
			{
				Client_OnDraggedItemSwapped(false);
				return;
			}
		}
	}
	if (SelectedSlotsType == ESlotsType::Equipment)
	{
		UItem* Item;
		GetItemInSlot(SelectedIndex, SelectedSlotsType, Item);
		Items.Empty();
		Slots.Empty();
		Items.Add(Item);
		Slots.Add(SelectedIndex);
	}

	if (!Items.IsValidIndex(0))
	{
		Client_OnDraggedItemSwapped(false);
		return;
	}
	UItem* ItemToDrag = Items[0];
	if (!IsValid(ItemToDrag) || !GetSlots(ESlotsType::HiddenSlots).IsValidIndex(0))
	{
		Client_OnDraggedItemSwapped(false);
		return;
	}

	if (ItemClass != DraggedItem->GetClass()) return;

	if (!HiddenSlots.IsValidIndex(0)) return;

	FSlotStruct HiddenSlot;
	HiddenSlot.IsEmpty = false;
	HiddenSlot.ItemReference = ItemToDrag;
	HiddenItemSlotsType = ItemToDrag->SlotsType == ESlotsType::Storage || DraggedItem->SlotsType ==
	                      ESlotsType::Storage || (IsValidSwappedItem() && (HiddenItemSlotsType ==
		                      ESlotsType::Storage))
		                      ? ESlotsType::Storage
		                      : ESlotsType::Primary;

	ClearSlots(DraggedItem->SlotsType, DraggedItem->OccupiedSlots);
	ClearSlots(SelectedSlotsType, ItemToDrag->OccupiedSlots);


	ItemToDrag->SlotsType = ESlotsType::HiddenSlots;
	ItemToDrag->OccupiedSlots = TArray<int32>{0};
	HiddenSlots[0] = HiddenSlot;


	AddExistingItemToSlots(DraggedItem, SelectedSlotsType, DraggedItemRotation, Slots);
	if (SelectedSlotsType == ESlotsType::Storage || SelectedSlotsType == ESlotsType::Equipment)
	{
		Server_CheckAndRemoveItemFromShortcuts(DraggedItem);
	}
	if (SelectedSlotsType == ESlotsType::Equipment)
	{
		OnEquipmentSlotChanged_Server.Broadcast(true, SelectedIndex, DraggedItem);
	}
	Client_OnDraggedItemSwapped(true);
}


bool UInventoryComponent::IsValidSwappedItem() const
{
	if (HiddenSlots.IsValidIndex(0) && HiddenSlots[0].ItemReference)
		return true;
	return false;
}

void UInventoryComponent::OnAdditionSlotsChanged()
{
	if ((!IsSlotsHaveItems(ESlotsType::Temp) || IsValid(StorageComponent)))
	{
		if ((!IsValidSwappedItem() ||
			(IsValidSwappedItem() && HiddenSlots[0].ItemReference->SlotsType == ESlotsType::Storage)))
			SavePrimarySlotsInArray();
	}
}

bool UInventoryComponent::CanStackDraggedItem(const int32 SelectedSlotIndex, const ESlotsType SelectedSlotType,
                                              const int32 DraggedItemSlotIndex, const ESlotsType DraggedItemSlotType,
                                              const EItemRotation DraggedItemRotation)
{
	UItem* SelectedItem;
	if (!GetItemInSlot(SelectedSlotIndex, SelectedSlotType, SelectedItem)) return false;

	if (SelectedItem->GetSlotIndex() != SelectedSlotIndex) return false;
	if (SelectedItem->Rotation != DraggedItemRotation) return false;

	UItem* DraggedItem;
	if (!GetItemInSlot(DraggedItemSlotIndex, DraggedItemSlotType, DraggedItem)) return false;


	if (!IsValid(SelectedItem) || !IsValid(DraggedItem)) return false;

	if ((SelectedItem->GetClass() == DraggedItem->GetClass()) && (SelectedItem != DraggedItem))
	{
		if (SelectedItem->IsStackable())
		{
			const int32 MaxStack = SelectedItem->MaxStack;
			if (SelectedItem->Amount >= MaxStack) return false;
			//if (DraggedItem->Amount >= MaxStack) return false;
			const int32 CanAdd = MaxStack - SelectedItem->Amount;
			if (CanAdd <= 0) return false;
			return true;
		}
	}
	return false;
}

bool UInventoryComponent::CanSwapSameSizeItems(const UItem* Item_One, const UItem* Item_Two) const
{
	if (IsValidSwappedItem()) return false;
	if (!IsValid(Item_One) || !IsValid(Item_Two)) return false;

	const FSize_isx ItemOneSize = Item_One->Size;
	const FSize_isx ItemTwoSize = Item_Two->Size;

	if (ItemOneSize.Height != ItemTwoSize.Height) return false;
	if (ItemOneSize.Width != ItemTwoSize.Width) return false;

	return true;
}

bool UInventoryComponent::CanSwapSameSizeItems_2(const int32 SlotIndex_1, const ESlotsType SlotsType_1,
                                                 const int32 SlotIndex_2, const ESlotsType SlotsType_2)
{
	if (!GetSlots(SlotsType_1).IsValidIndex(SlotIndex_1)) return false;
	if (!GetSlots(SlotsType_2).IsValidIndex(SlotIndex_2)) return false;
	const UItem* ItemOne = GetSlots(SlotsType_1)[SlotIndex_1].ItemReference;
	const UItem* ItemTwo = GetSlots(SlotsType_2)[SlotIndex_2].ItemReference;

	return CanSwapSameSizeItems(ItemOne, ItemTwo);
}

bool UInventoryComponent::CanSwapEquipmentItem(const int32 DraggedItemIndex, const ESlotsType DraggedItemSlotsType,
                                               const int32 EquipSlotIndex, const ESlotsType EquipSlotType)
{
	if (EquipSlotType != ESlotsType::Equipment) return false;

	if (!GetSlots(DraggedItemSlotsType).IsValidIndex(DraggedItemIndex)) return false;
	//	if (!GetSlots(EquipSlotType).IsValidIndex(EquipSlotIndex)) return false;
	FSlotStruct EquipmentSlotData;
	if (!GetSlotInfo(EquipSlotIndex, EquipSlotType, EquipmentSlotData)) return false;
	if (EquipmentSlotData.IsEmpty || !EquipmentSlotData.ItemReference) return false;


	UItem* DraggedItem = GetSlots(DraggedItemSlotsType)[DraggedItemIndex].ItemReference;
	const UItem* L_EquippedItem = EquipmentSlotData.ItemReference;

	if (!IsValid(DraggedItem) || !IsValid(L_EquippedItem)) return false;

	if (DraggedItem == L_EquippedItem) return false;

	bool CanEquip;
	EEquipmentSlotType_Isx EquipmentType;
	EItemRotation ItemRotation;
	DraggedItem->CanEquip(this, GetInventoryOwnerPawn(), CanEquip, EquipmentType, ItemRotation);
	if (!CanEquip) return false;


	if (CanSwapSameSizeItems(DraggedItem, L_EquippedItem)) return true;
	return false;
}

void UInventoryComponent::ExecuteAction_Implementation(const int32 Index, UItem* Item)
{
	if (!IsValid(Item)) return;

	if (!Item->CanExecuteAction(Index, this, GetInventoryOwnerPawn())) return;
	Item->ExecuteAction(Index, this, GetInventoryOwnerPawn());
}

void UInventoryComponent::Server_SwapSameSizeItems_Equipment_Implementation(
	const int32 DraggedItemIndex, const ESlotsType DraggedItemSlotsType, const int32 EquipSlotIndex,
	const ESlotsType EquipSlotType)
{
	if (!CanSwapEquipmentItem(DraggedItemIndex, DraggedItemSlotsType, EquipSlotIndex, EquipSlotType)) return;
	Server_SwapSameSizeItems(DraggedItemIndex, DraggedItemSlotsType, EquipSlotIndex, EquipSlotType);
	Client_OnItemMoved(false, IsSlotsHaveItems(ESlotsType::Temp), EquipSlotIndex, EquipSlotType);
}

void UInventoryComponent::Server_SwapSameSizeItems_Implementation(const int32 SlotIndex_1, const ESlotsType SlotsType_1,
                                                                  const int32 SlotIndex_2, const ESlotsType SlotsType_2)
{
	if (!GetSlots(SlotsType_1).IsValidIndex(SlotIndex_1)) return;
	if (!GetSlots(SlotsType_2).IsValidIndex(SlotIndex_2)) return;

	UItem* ItemOne = GetSlots(SlotsType_1)[SlotIndex_1].ItemReference;
	UItem* ItemTwo = GetSlots(SlotsType_2)[SlotIndex_2].ItemReference;

	if (!CanSwapSameSizeItems(ItemOne, ItemTwo)) return;

	const TArray<int32> OccupiedSlots_1 = ItemOne->OccupiedSlots;
	const TArray<int32> OccupiedSlots_2 = ItemTwo->OccupiedSlots;

	const ESlotsType SlotType_1 = ItemOne->SlotsType;
	const ESlotsType SlotType_2 = ItemTwo->SlotsType;

	const EItemRotation Rotation_1 = ItemOne->Rotation;
	const EItemRotation Rotation_2 = ItemTwo->Rotation;

	Client_OnItemRemoved(OccupiedSlots_1, SlotType_1);
	Client_OnItemRemoved(OccupiedSlots_2, SlotType_2);


	// ItemOne->OccupiedSlots = OccupiedSlots_2;
	// ItemOne->SlotsType = SlotType_2;
	ItemOne->Rotation = Rotation_2;

	// ItemTwo->OccupiedSlots = OccupiedSlots_1;
	// ItemTwo->SlotsType = SlotType_1;
	ItemTwo->Rotation = Rotation_1;

	FillSlots(ItemOne, OccupiedSlots_2, SlotsType_2);
	FillSlots(ItemTwo, OccupiedSlots_1, SlotsType_1);

	GetSlots(SlotsType_1)[SlotIndex_1].ItemReference = ItemTwo;
	GetSlots(SlotsType_2)[SlotIndex_2].ItemReference = ItemOne;

	Client_OnItemAdded(ItemOne->SlotsType, ItemOne->GetClass(), ItemOne->OccupiedSlots, ItemOne->GetAmmoOrAmount(),
	                   ItemOne->Rotation, ItemOne->CanDestroy(this, GetInventoryOwnerPawn()),
	                   FindItemInShortcuts(ItemOne), IsEquippedItem(ItemOne));

	Client_OnItemAdded(ItemTwo->SlotsType, ItemTwo->GetClass(), ItemTwo->OccupiedSlots, ItemTwo->GetAmmoOrAmount(),
	                   ItemTwo->Rotation, ItemTwo->CanDestroy(this, GetInventoryOwnerPawn()),
	                   FindItemInShortcuts(ItemTwo), IsEquippedItem(ItemTwo));
}


void UInventoryComponent::Server_TakeAll_Implementation()
{
	if (!IsValid(StorageComponent)) return;
	for (FSlotStruct const& Slot : GetSlots(ESlotsType::Storage))
	{
		if (Slot.IsEmpty || Slot.IsPartOfItem) continue;
		UItem* Item = Slot.ItemReference;
		if (!IsValid(Item)) return;
		const int32 CanAdd = CanAddItemCount(Item->GetClass(), Item->GetAmount(), ESlotsType::Primary);
		if (CanAdd <= 0) continue;

		const int32 AmountAfterAdd = Item->GetAmount() - CanAdd;
		Server_AddItem(Item->GetClass(), CanAdd, Item->GetAmmo());
		if (!IsValid(Item)) continue;
		SetItemAmount(Item, AmountAfterAdd);
	}
}

bool UInventoryComponent::GetShortcut(const int32 Index, FShortcut& ShortcutData)
{
	if (GetShortcuts().IsValidIndex(Index))
	{
		ShortcutData = GetShortcuts()[Index];
		return true;
	}
	return false;
}

TArray<UItem*> UInventoryComponent::GetAllShortcutItems()
{
	TArray<UItem*> ItemsInShortcuts;
	for (FShortcut const& ShortcutSlot : GetShortcuts())
	{
		if (ShortcutSlot.IsEmpty || !IsValid(ShortcutSlot.Item)) continue;
		ItemsInShortcuts.Add(ShortcutSlot.Item);
	}
	return ItemsInShortcuts;
}

void UInventoryComponent::Server_StackDraggedItem_Implementation(const int32 SelectedSlotIndex,
                                                                 const ESlotsType SelectedSlotType,
                                                                 const int32 DraggedItemSlotIndex,
                                                                 const ESlotsType DraggedItemSlotType,
                                                                 const EItemRotation DraggedItemRotation)
{
	if (!CanStackDraggedItem(SelectedSlotIndex, SelectedSlotType, DraggedItemSlotIndex, DraggedItemSlotType,
	                         DraggedItemRotation))
		return;

	UItem* SelectedItem;
	if (!GetItemInSlot(SelectedSlotIndex, SelectedSlotType, SelectedItem)) return;

	UItem* DraggedItem;
	if (!GetItemInSlot(DraggedItemSlotIndex, DraggedItemSlotType, DraggedItem)) return;

	if (!IsValid(SelectedItem) || !IsValid(DraggedItem)) return;

	if (SelectedItem->GetClass() == DraggedItem->GetClass() && SelectedItem != DraggedItem)
	{
		if (SelectedItem->IsStackable())
		{
			const int32 MaxStack = SelectedItem->MaxStack;
			const int32 CanAdd = MaxStack - SelectedItem->Amount;
			const int32 DraggedItemAmount = DraggedItem->Amount;
			if (DraggedItemAmount >= CanAdd)
			{
				SetItemAmount(SelectedItem, SelectedItem->Amount + CanAdd);
				Server_RemoveItemByRef(DraggedItem, CanAdd);
				Client_OnDraggedItemCombined(DraggedItemAmount - CanAdd);
				return;
			}
			else
			{
				SetItemAmount(SelectedItem, SelectedItem->Amount + DraggedItem->Amount);
				Server_RemoveItemByRef(DraggedItem, 1, true);
				Client_OnDraggedItemCombined(0);
				return;
			}
		};
	}
}


void UInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	OnShortcutsChanged_Server.Clear();
	OnStorageSlotsChanged.Clear();
	OnItemMoved.Clear();
	OnAllItemsRemoved.Clear();
	OnItemRemoved.Clear();
	OnItemChangeAmount.Clear();
	OnItemsAdded.Clear();
	OnItemAdded.Clear();
	OnResizeInventory.Clear();
	OnEquipmentSlotChanged_Server.Clear();
	OnEquip.Clear();
	OnEquipServer.Clear();
	OnDraggedItemCombined.Clear();
	OnDraggedItemSwapped.Clear();
	Server_OnShortcutSelected.Clear();
	OnAmmoChanged.Clear();
	OnUpdateStorageSlots.Clear();
	OnCombineResult.Clear();
	OnLoad.Clear();
	OnChangingAdditionalSlots_Server.Clear();
	OnDiscardItem_Server.Clear();
	OnPrimarySlotsLoaded.Clear();
	OnChangingAdditionalSlots.Clear();
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UInventoryComponent, InventorySlots);
	DOREPLIFETIME(UInventoryComponent, TempInventorySlots);
	DOREPLIFETIME(UInventoryComponent, HiddenSlots);
	DOREPLIFETIME(UInventoryComponent, Shortcuts);
	DOREPLIFETIME(UInventoryComponent, StorageComponent);
	DOREPLIFETIME(UInventoryComponent, SelectedShortcut);
	DOREPLIFETIME(UInventoryComponent, EquippedItem);
	DOREPLIFETIME(UInventoryComponent, HiddenItemSlotsType);
	DOREPLIFETIME(UInventoryComponent, InventorySize);
	DOREPLIFETIME(UInventoryComponent, EquipmentSlots);
}

bool UInventoryComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool RepBool = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (auto const& InventorySlot : InventorySlots)
	{
		RepBool |= Channel->ReplicateSubobject(InventorySlot.ItemReference, *Bunch, *RepFlags);
	}
	for (auto const& InventorySlot : TempInventorySlots)
	{
		RepBool |= Channel->ReplicateSubobject(InventorySlot.ItemReference, *Bunch, *RepFlags);
	}

	for (auto const& InventorySlot : HiddenSlots)
	{
		RepBool |= Channel->ReplicateSubobject(InventorySlot.ItemReference, *Bunch, *RepFlags);
	}

	for (auto const& InventorySlot : EquipmentSlots)
	{
		RepBool |= Channel->ReplicateSubobject(InventorySlot.ItemReference, *Bunch, *RepFlags);
	}

	return RepBool;
}


void UInventoryComponent::UseItem_Implementation(const int32 Index, const ESlotsType SlotsType)
{
	FSlotStruct SlotInfo;
	if (!GetSlotInfo(Index, SlotsType, SlotInfo)) return;

	UItem* Item = SlotInfo.ItemReference;
	if (!IsValid(Item)) return;

	UMaterialInterface* Material;
	FText Text;
	if (!Item->CanUse(this, GetInventoryOwnerPawn(), Text, Material)) return;

	bool Destroy;
	Item->OnUseItem(this, GetInventoryOwnerPawn(), Destroy);
	if (Destroy)
	{
		Server_RemoveItemsInSlot(Index, SlotsType, 1);
	}
}


void UInventoryComponent::SetSlots_Implementation(const ESlotsType SlotsType, const TArray<FSlotStruct>& Slots)
{
	GetSlots(SlotsType) = Slots;
}
