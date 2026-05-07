/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/


#include "Components/StorageComponent.h"

#include "Actors/GlobalStorage.h"
#include "Engine/ActorChannel.h"
#include "Functions/InventoryStaticFunctions.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"


// Sets default values for this component's properties
UStorageComponent::UStorageComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	// ...
}


// Called when the game starts
void UStorageComponent::BeginPlay()
{
	//Find Global Storage
	if (IsGlobalStorage && GetOwnerRole() == ROLE_Authority)
	{
		GetGlobalStorage();
	}

	//Initialize Storage
	if (!IsGlobalStorage && GetOwnerRole() == ROLE_Authority)
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
		for (auto const& ToAdd : ItemsToAdd)
			AddItem(ToAdd.ItemClass, ToAdd.Amount, ToAdd.Ammo);
	}
	Super::BeginPlay();
}


void UStorageComponent::AddItem(TSubclassOf<UItem> ItemClass, int32 Amount, int32 Ammo)
{
	if (!ItemClass) return; //false;
	if (Amount <= 0) return; //false;

	const UItem* ItemObject = ItemClass.GetDefaultObject();
	if (!ItemObject) return; // false;

	const int32 MaxStack = ItemObject->MaxStack;

	TArray<int32> EmptySlots;
	EItemRotation Rotation;


	while (UInventoryStaticFunctions::FindEmptySlots(Slots, ItemObject->Size, Size, Rotation,
	                                                 EmptySlots, TArray<int32>()))
	{
		if (ItemObject->IsStackable())
		{
			if (Amount <= MaxStack)
			{
				UInventoryStaticFunctions::AddItemToSlots(this, nullptr, Slots, ItemClass, Amount, Ammo, Rotation,
				                                          EmptySlots, ESlotsType::Storage);
				return; // true;
			}
			UInventoryStaticFunctions::AddItemToSlots(this, nullptr, Slots, ItemClass, MaxStack, Ammo, Rotation,
			                                          EmptySlots, ESlotsType::Storage);
			Amount -= MaxStack;
		}
		else
		{
			UInventoryStaticFunctions::AddItemToSlots(this, nullptr, Slots, ItemClass, 1, Ammo, Rotation,
			                                          EmptySlots, ESlotsType::Storage);
			Amount -= 1;
		}
		if (Amount <= 0)
		{
			return; // true;
		}
	}
}

bool UStorageComponent::CanOpen_Implementation()
{
	return true;
}

AGlobalStorage* UStorageComponent::GetGlobalStorage()
{
	if (IsValid(GlobalStorage)) return GlobalStorage;
	if (AActor* ActorStorage = UGameplayStatics::GetActorOfClass(this, AGlobalStorage::StaticClass()))
	{
		GlobalStorage = Cast<AGlobalStorage>(ActorStorage);
		if (IsValid(GlobalStorage))
			return GlobalStorage;
	}
	return nullptr;
}

void UStorageComponent::Server_CloneInventory_2_Implementation(const TArray<FSlotStruct>& InSlots, const FSize_isx InSize)
{
	Slots = InSlots;
	for (FSlotStruct const & Slot : Slots)
	{
		if(Slot.IsEmpty && Slot.IsPartOfItem) continue;
		if(!IsValid(Slot.ItemReference)) continue;

		Slot.ItemReference->SlotsType = ESlotsType::Storage;
	}
	Size = InSize;
}


void UStorageComponent::Server_CloneInventory_Implementation(UInventoryComponent* InventoryComponent)
{
	if (!IsValid(InventoryComponent)) return;
	Server_CloneInventory_2(InventoryComponent->GetSlots(ESlotsType::Primary),
	                        InventoryComponent->GetInventorySize(ESlotsType::Primary));
}

FStorageSaveData UStorageComponent::GetSaveData()
{
	TArray<FSaveData_isx> SlotsSaveData;

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

		SlotsSaveData.Add(SaveData);
	}

	FStorageSaveData StorageSaveData;
	StorageSaveData.SaveData = SlotsSaveData;
	StorageSaveData.GUID = GUID;

	return StorageSaveData;
}

void UStorageComponent::Server_LoadStorageFromSave_Implementation(const TArray<FSaveData_isx>& SaveData)
{
	//Clear Slots;
	for (FSlotStruct& Slot : Slots)
	{
		Slot.IsEmpty = true;
		Slot.IsPartOfItem = false;
		Slot.ItemReference = nullptr;
	}

	//Add Items From Data
	for (FSaveData_isx const& Data : SaveData)
	{
		UInventoryStaticFunctions::AddItemToSlots(this, nullptr, Slots, Data.ItemClass, Data.Amount, Data.Amount,
		                                          Data.Rotation,
		                                          Data.OccupiedSlots, Data.SlotsType);
	}
}


void UStorageComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UStorageComponent, Slots);
	DOREPLIFETIME(UStorageComponent, Size);
	DOREPLIFETIME(UStorageComponent, IsGlobalStorage);
	DOREPLIFETIME(UStorageComponent, GlobalStorage);
}

bool UStorageComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool RepBool = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (auto const& InventorySlot : Slots)
	{
		RepBool |= Channel->ReplicateSubobject(InventorySlot.ItemReference, *Bunch, *RepFlags);
	}
	return RepBool;
}
