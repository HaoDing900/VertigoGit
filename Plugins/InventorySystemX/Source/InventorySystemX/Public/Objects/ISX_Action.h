/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/Texture2D.h"
#include "ISX_Action.generated.h"

#define  LOCTEXT_NAMESPACE "Action"
class UItem;
/**
 * 
 */
class UInventoryComponent;

UCLASS(Blueprintable, BlueprintType, meta=(ShowWorldContextPin))
class INVENTORYSYSTEMX_API UISX_Action : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Action")
	FText Name;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Action")
	UTexture2D* Icon;

	UFUNCTION(BlueprintNativeEvent, Category="Action")
	bool CanExecute(UItem* Item, UInventoryComponent* InventoryComponent, APawn* Character);

	bool CanExecute_Implementation(UItem* Item, UInventoryComponent* InventoryComponent, APawn* Character)
	{
		return true;
	}


	UFUNCTION(BlueprintNativeEvent, Category="Action")
	void OnExecute(UItem* Item, UInventoryComponent* InventoryComponent, APawn* Character);

	void OnExecute_Implementation(UItem* Item, UInventoryComponent* InventoryComponent, APawn* Character)
	{
	}
};


#undef LOCTEXT_NAMESPACE
