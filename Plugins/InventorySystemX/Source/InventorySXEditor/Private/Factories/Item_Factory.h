/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once

#include "CoreMinimal.h"
#include "Item_Factory.generated.h"

/**
 * 
 */
UCLASS()
class INVENTORYSXEDITOR_API UItem_Factory : public UFactory
{
	GENERATED_BODY()

	UItem_Factory(const FObjectInitializer& ObjectInitializer);
	
	
	//virtual bool ConfigureProperties() override;
	
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
	virtual FString GetDefaultNewAssetName() const override;
	
	
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
};
