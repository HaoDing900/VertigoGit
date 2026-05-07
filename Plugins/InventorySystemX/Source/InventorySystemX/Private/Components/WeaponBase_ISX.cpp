/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/


#include "Components/WeaponBase_ISX.h"

#include "Net/UnrealNetwork.h"

// Sets default values for this component's properties
UWeaponBase_ISX::UWeaponBase_ISX()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UWeaponBase_ISX::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwnerRole() == ROLE_Authority)
	{
		if (GetInventoryComponent())
		{
			GetInventoryComponent()->Server_OnShortcutSelected.AddDynamic(this, &UWeaponBase_ISX::OnShortcutSelected);

			GetInventoryComponent()->OnShortcutsChanged_Server.AddDynamic(this, &UWeaponBase_ISX::OnShortcutsChanged);

			GetInventoryComponent()->OnEquipServer.AddDynamic(this, &UWeaponBase_ISX::OnEquipWeapon);
		}
	}
}

UInventoryComponent* UWeaponBase_ISX::GetInventoryComponent_Implementation()
{
	if (InventoryComponent) return InventoryComponent;
	if (!GetOwner()) return nullptr;

	InventoryComponent = GetOwner()->FindComponentByClass<UInventoryComponent>();
	if (InventoryComponent) return InventoryComponent;

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return nullptr;
	if (!Pawn->GetController()) return nullptr;
	return InventoryComponent = Pawn->GetController()->FindComponentByClass<UInventoryComponent>();
}

void UWeaponBase_ISX::OnShortcutSelected(const bool IsEmpty, const int32 ShortcutIndex, UItem* Item)
{
	SetEquippedItem(IsEmpty ? nullptr : Item);
}

void UWeaponBase_ISX::OnShortcutsChanged(const TArray<UItem*>& ItemsInShortcuts)
{
	if (!GetInventoryComponent()) return;
	if (GetInventoryComponent()->FindItemInInventory(GetEquippedItem()) == -1)
		SetEquippedItem(nullptr);

	if (GetEquippedItem())
	{
		if (GetInventoryComponent()->GetEquippedItem() == GetEquippedItem()) return;
	}

	if (GetInventoryComponent()->GetSelectedShortcutData().Item == GetEquippedItem()) return;

	if (ItemsInShortcuts.Contains(EquippedItem)) return;

	SetEquippedItem(GetInventoryComponent()->GetSelectedShortcutData().Item);
}

void UWeaponBase_ISX::OnEquipWeapon(const bool Equip, UItem* Item)
{
	if (Equip)
	{
		if (!Item) return;
		SetEquippedItem(Item);
		return;
	}

	if (GetInventoryComponent())
		SetEquippedItem(GetInventoryComponent()->GetSelectedShortcutData().Item);
}

void UWeaponBase_ISX::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UWeaponBase_ISX, EquippedItem)
	DOREPLIFETIME(UWeaponBase_ISX, HidedItem)
}

void UWeaponBase_ISX::SetEquippedItem(UItem* Item)
{
	if (Item == GetEquippedItem()) return;
	EquippedItem = Item;
	HidedItem = nullptr;
	OnEquip.Broadcast(Item ? true : false, Item);
}

bool UWeaponBase_ISX::CanFire_Implementation(const int32 BulletsToShot) const
{
	if (!GetEquippedItem()) return false;
	return GetEquippedItem()->GetAmmo() >= BulletsToShot;
}

void UWeaponBase_ISX::SetAmmo_Implementation(const int32 NewAmmo)
{
	if (!GetInventoryComponent() || !GetEquippedItem()) return;
	GetInventoryComponent()->Server_SetItemAmmo(GetEquippedItem(), NewAmmo);
}

void UWeaponBase_ISX::HideWeapon()
{
	if (!GetEquippedItem()) return;
	UItem* Item = GetEquippedItem();
	SetEquippedItem(nullptr);
	HidedItem = Item;
}

void UWeaponBase_ISX::ShowWeapon()
{
	if (GetEquippedItem()) return;
	SetEquippedItem(HidedItem);
	HidedItem = nullptr;
}

bool UWeaponBase_ISX::CanReload_Implementation()
{
	if (!GetInventoryComponent() || !GetEquippedItem()) return false;
	if (!GetEquippedItem()->GetAmmoClass()) return false;
	const int32 AmmoInInventory = GetInventoryComponent()->GetNumberOfItems(GetEquippedItem()->GetAmmoClass());
	return AmmoInInventory > 0 && GetEquippedItem()->GetMaxAmmo() != GetEquippedItem()->GetAmmo();
}

void UWeaponBase_ISX::HideShowWeapon_Implementation(const EWeaponVisibility Visibility)
{
	if (Visibility == EWeaponVisibility::Hide)
		HideWeapon();
	if (Visibility == EWeaponVisibility::Show)
		ShowWeapon();
	if (Visibility == EWeaponVisibility::ShowHide)
	{
		if (WeaponIsHidden())
		{
			ShowWeapon();
		}
		else
		{
			HideWeapon();
		}
	}
}

void UWeaponBase_ISX::RemoveAmmo_Implementation(const int32 Remove)
{
	if (!GetInventoryComponent() || !GetEquippedItem()) return;
	GetInventoryComponent()->Server_AddItemAmmo(GetEquippedItem(), Remove * -1);
}

void UWeaponBase_ISX::Reload_Implementation()
{
	if (!GetInventoryComponent() || !GetEquippedItem()) return;
	GetInventoryComponent()->Server_ReloadWeapon(GetEquippedItem());
}
