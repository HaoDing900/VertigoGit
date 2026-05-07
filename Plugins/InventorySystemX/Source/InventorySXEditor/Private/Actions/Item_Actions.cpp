/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/


#include "Item_Actions.h"
#include "Objects/Item.h"


FItem_Actions::FItem_Actions( uint32 InAssetCategory)
{
	InventoryCategory = InAssetCategory;
}

FText FItem_Actions::GetName() const
{
	return FText::FromString("Inventory System X | Item");
}

UClass* FItem_Actions::GetSupportedClass() const
{
	return UItem::StaticClass();
}


uint32 FItem_Actions::GetCategories()
{
	return InventoryCategory;
}
