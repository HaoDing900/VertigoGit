/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/


#include "Objects/Item.h"
#include "Engine/NetDriver.h"
#include "Components/InventoryComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "Inventory"

UWorld* UItem::GetWorld() const
{
	if (GIsEditor && !GIsPlayInEditorWorld) return nullptr;
	else if (GetOuter()) return GetOuter()->GetWorld();
	else return nullptr;
}


void UItem::EquipOrUnequipItem(UInventoryComponent* InventoryComponent)
{
	if (SlotsType != ESlotsType::Primary) return;
	UInventoryComponent* LocalInventoryComponent =
		InventoryComponent ? InventoryComponent : InventoryComponentReference;
	if (!IsValid(LocalInventoryComponent)) return;

	if (!LocalInventoryComponent->IsEquippedItem(this))
	{
		LocalInventoryComponent->Server_EquipItem(this);
		return;
	}
	LocalInventoryComponent->Server_UnequipItem();
}

void UItem::CanEquipOrUnequip(UInventoryComponent* InventoryComponent, FText& UseText, bool& CanUse)
{
	const FText Equip = LOCTEXT("Equip", "Equip");
	const FText Unequip = LOCTEXT("Unequip", "Unequip");
	CanUse = SlotsType == ESlotsType::Primary;
	const UInventoryComponent* LocalInventoryComponent =
		InventoryComponent ? InventoryComponent : InventoryComponentReference;
	if (!IsValid(LocalInventoryComponent))
	{
		UseText = FText::FromString("");
		CanUse = false;
		return;
	}
	UseText = LocalInventoryComponent->IsEquippedItem(this) ? Unequip : Equip;
}

void UItem::CanEquip_Implementation(UInventoryComponent* InventoryComponent, APawn* PlayerPawn, bool& CanEquip,
                                    EEquipmentSlotType_Isx& EquipmentType, EItemRotation& ItemRotation) const
{
	CanEquip = false;
	EquipmentType = EEquipmentSlotType_Isx::None;
	ItemRotation = EItemRotation::Horizontal;
}


bool UItem:: HasTag(const FName Tag)
{
	for (FName const& TagName : Tags)
	{
		if (Tag == TagName)
			return true;
	}
	return false;
}

TArray<FActionItem> UItem::GetActionsList(UInventoryComponent* InventoryComponent, APawn* Character)
{
	TArray<FActionItem> ActionsList;
	for (TSubclassOf<UISX_Action> const& Action : Actions)
	{
		FActionItem ActionItem;
		if (UISX_Action* DefaultAction = Action.GetDefaultObject())
		{
			ActionItem.Show = DefaultAction->CanExecute(this, InventoryComponent, Character);
			ActionItem.Name = DefaultAction->Name;
			ActionItem.Icon = DefaultAction->Icon;
			ActionsList.Add(ActionItem);
		}
		else
		{
			ActionItem.Show = false;
			ActionsList.Add(ActionItem);
		}
	}
	return ActionsList;
}

bool UItem::CanExecuteAction(const int32 Index, UInventoryComponent* InventoryComponent, APawn* Character)
{
	if (!Actions.IsValidIndex(Index)) return false;
	if (!Actions[Index]) return false;

	if (UISX_Action* DefaultAction = Actions[Index].GetDefaultObject())
	{
		return DefaultAction->CanExecute(this, InventoryComponent, Character);
	}
	return false;
}

void UItem::ExecuteAction(const int32 Index, UInventoryComponent* InventoryComponent, APawn* Character)
{
	if (!Actions.IsValidIndex(Index)) return;
	if (!Actions[Index]) return;

	if (UISX_Action* DefaultAction = Actions[Index].GetDefaultObject())
	{
		DefaultAction->OnExecute(this, InventoryComponent, Character);
	}
}

bool UItem::GetProperty(const FName PropertyName, float& Value)
{
	const float* Ref = Properties.Find(PropertyName);
	if (Ref == nullptr)
	{
		Value = 0;
		return false;
	}
	Value = *Ref;
	return true;
}

void UItem::OnUseItem_Implementation(UInventoryComponent* InventoryComponent, APawn* PlayerPawn, bool& Destroy)
{
	Destroy = false;
}

bool UItem::CanUse_Implementation(UInventoryComponent* InventoryComponent, APawn* PlayerPawn, FText& UseText,
                                  UMaterialInterface*& IconMaterial)
{
	if (UseText.IsEmpty())
	{
		UseText = NSLOCTEXT("Inventory", "Item", "Use");
	}
	IconMaterial = nullptr;
	return false;
}

bool UItem::CanDestroy_Implementation(UInventoryComponent* InventoryComponent, APawn* PlayerPawn)
{
	return true;
}

bool UItem::CanCombine(UInventoryComponent* InventoryComponent) const
{
	const UInventoryComponent* LocalInventoryComponent = InventoryComponent
		                                                     ? InventoryComponent
		                                                     : InventoryComponentReference
		                                                     ? InventoryComponentReference
		                                                     : nullptr;
	if (!IsValid(LocalInventoryComponent)) return false;
	return LocalInventoryComponent->CanCombineItem(GetClass());
}


void UItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UObject::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UItem, Amount);
	DOREPLIFETIME(UItem, Rotation);
	DOREPLIFETIME(UItem, OccupiedSlots);
	DOREPLIFETIME(UItem, SlotsType);
	DOREPLIFETIME(UItem, InventoryComponentReference);
	DOREPLIFETIME(UItem, Ammo);

	if (const UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(GetClass()))
	{
		BPClass->GetLifetimeBlueprintReplicationList(OutLifetimeProps);
	}
}

bool UItem::CallRemoteFunction(UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack)
{
	if (!GetOuter()) return false;
	if (UActorComponent* ActorComponent = Cast<UActorComponent>(GetOuter()))
	{
		if (AActor* Actor = Cast<AActor>(ActorComponent))
		{
			UNetDriver* NetDriver = Actor->GetNetDriver();
			if (!NetDriver) return false;
			NetDriver->ProcessRemoteFunction(Actor, Function, Parms, OutParms, Stack, this);
			return true;
		}
	}
	return false;
}

int32 UItem::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
	return (GetOuter() ? GetOuter()->GetFunctionCallspace(Function, Stack) : FunctionCallspace::Local);
}
