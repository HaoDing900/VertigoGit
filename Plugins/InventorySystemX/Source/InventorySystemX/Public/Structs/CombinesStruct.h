/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once
#include "Engine/DataTable.h"
#include "Objects/Item.h"
#include "CombinesStruct.generated.h"


USTRUCT(BlueprintType)
struct FCombinesStruct : public FTableRowBase	
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combines")
	TSubclassOf<UItem> FirstItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combines")
	TSubclassOf<UItem> SecondItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combines")
	TSubclassOf<UItem> Result;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin = 1), Category="Combines")
	int32 Amount;

	FCombinesStruct():Amount(1){}
};
