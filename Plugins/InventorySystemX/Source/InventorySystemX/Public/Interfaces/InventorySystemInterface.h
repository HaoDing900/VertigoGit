/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Objects/Item.h"
#include "InventorySystemInterface.generated.h"

class UInventoryComponent;
// This class does not need to be modified.
UINTERFACE(BlueprintType, Blueprintable)
class UInventorySystemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class INVENTORYSYSTEMX_API IInventorySystemInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="Inventory System X Interface - Server")
	void OnInventoryComponentInitialized(UInventoryComponent* InventoryComponent);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="Inventory System X Interface - Server")
	void OnDiscardItem(TSubclassOf<UItem> Item, const int32 Amount, const int32 Ammo, const ESlotsType SlotsType);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="Inventory System X Interface - Equipment")
	void PreEquip(UInventoryComponent* InventoryComponent, const TArray<UItem*>& ItemsInSlots,
					UItem*& ReplaceItem);
};
