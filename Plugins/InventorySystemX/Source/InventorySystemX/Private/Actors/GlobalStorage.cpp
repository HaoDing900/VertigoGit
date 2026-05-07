/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/


#include "Actors/GlobalStorage.h"
#include "Engine/ActorChannel.h"
#include "Functions/InventoryStaticFunctions.h"
#include "Net/UnrealNetwork.h"


// Sets default values for this component's properties
AGlobalStorage::AGlobalStorage()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	// ...
}


// Called when the game starts
void AGlobalStorage::BeginPlay()
{
	Super::BeginPlay();
	
	//Initialize Slots
	if (GetLocalRole() == ROLE_Authority)
	{
		int32 const StorageSize = Size.Height * Size.Width;
		for (int32 x = 0; x < StorageSize; x++)
		{
			FSlotStruct Slot;
			Slot.Index = x;
			Slot.IsEmpty = true;
			Slot.IsPartOfItem = false;
			Slot.ItemReference = nullptr;

			Slots.Add(Slot);
		}
	}
}

TArray<FSaveData_isx> AGlobalStorage::GetSaveData()
{
	TArray<FSaveData_isx> SaveDataArray;
	for (FSlotStruct const& Slot : Slots)
	{
		if (Slot.IsEmpty || Slot.IsPartOfItem) continue;
		if (!IsValid(Slot.ItemReference)) continue;

		const UItem* Item = Slot.ItemReference;
		FSaveData_isx SaveData;
		SaveData.ItemClass = Item->GetClass();
		SaveData.OccupiedSlots = Item->OccupiedSlots;
		SaveData.SlotsType = Item->SlotsType;
		SaveData.Rotation = Item->Rotation;
		SaveData.Amount = Item->Type == EItemType::Weapon ? Item->GetAmmo() : Item->GetAmount();

		SaveDataArray.Add(SaveData);
	}
	return SaveDataArray;
}


void AGlobalStorage::Server_LoadStorageFromSave_Implementation(const TArray<FSaveData_isx>& SaveData)
{
	//Clear Slots
	for (FSlotStruct& Slot : Slots)
	{
		Slot.IsEmpty = true;
		Slot.IsPartOfItem = false;
		Slot.ItemReference = nullptr;
	}

	//Add Items
	for (auto const& Data : SaveData)
	{
		UInventoryStaticFunctions::AddItemToSlots(this, nullptr, Slots, Data.ItemClass, Data.Amount, Data.Amount,
		                                          Data.Rotation,
		                                          Data.OccupiedSlots, Data.SlotsType);
	}
}

void AGlobalStorage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGlobalStorage, Slots);
	
}

bool AGlobalStorage::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool RepBool = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	for (auto const& InventorySlot : Slots)
	{
		RepBool |= Channel->ReplicateSubobject(InventorySlot.ItemReference, *Bunch, *RepFlags);
	}
	
	return RepBool;
}
