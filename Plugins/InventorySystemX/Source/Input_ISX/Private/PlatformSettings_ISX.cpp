/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#include "PlatformSettings_ISX.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "Input_ISX_Subsystem.h"






void UPlatformSettings_ISX::InitializePlatformDefaults()
{
	const FName PlatformName = GetPlatformIniName();
	const FDataDrivenPlatformInfo& PlatformInfo = FDataDrivenPlatformInfoRegistry::GetPlatformInfo(PlatformName);
	DefaultInputType = EInputType_ISX::Gamepad;
	if (PlatformInfo.DefaultInputType == "Gamepad")
	{
		DefaultInputType = EInputType_ISX::Gamepad;
	}
	else if (PlatformInfo.DefaultInputType == "Touch")
	{
		DefaultInputType = EInputType_ISX::Touch;
	}
	else if (PlatformInfo.DefaultInputType == "MouseAndKeyboard")
	{
		DefaultInputType = EInputType_ISX::MouseAndKeyboard;
	}
}

EInputType_ISX UPlatformSettings_ISX::GetDefaultInputType() const
{
	return DefaultInputType;
}
