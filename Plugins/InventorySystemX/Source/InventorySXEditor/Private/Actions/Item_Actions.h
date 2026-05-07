/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_Blueprint.h"


/**
 * 
 */


class INVENTORYSXEDITOR_API FItem_Actions : public FAssetTypeActions_Base
{
public:
	FItem_Actions(uint32 InAssetCategory);

	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override { return FColor(63, 126, 255); }
	virtual uint32 GetCategories() override;

private:
	uint32 InventoryCategory;
};
