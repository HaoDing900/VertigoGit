/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/PlatformSettings.h"
#include "PlatformSettings_ISX.generated.h"

/**
 * 
 */
UCLASS()
class INPUT_ISX_API UPlatformSettings_ISX : public UPlatformSettings
{
	GENERATED_BODY()

	virtual void InitializePlatformDefaults() override;
	
	UPROPERTY()
	EInputType_ISX DefaultInputType;

public:
	
	EInputType_ISX GetDefaultInputType() const;
};

