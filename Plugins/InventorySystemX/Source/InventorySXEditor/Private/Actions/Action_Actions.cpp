/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/


#include "Action_Actions.h"
#include "Objects/ISX_Action.h"


FAction_Actions::FAction_Actions( uint32 InAssetCategory)
{
	InventoryCategory = InAssetCategory;
}

FText FAction_Actions::GetName() const
{
	return FText::FromString("Inventory System X | Action");
}

UClass* FAction_Actions::GetSupportedClass() const
{
	return UISX_Action::StaticClass();
}


uint32 FAction_Actions::GetCategories()
{
	return InventoryCategory;
}
