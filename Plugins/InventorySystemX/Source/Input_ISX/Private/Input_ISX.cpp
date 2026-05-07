/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#include "Input_ISX.h"

DEFINE_LOG_CATEGORY(Input_ISX);

#define LOCTEXT_NAMESPACE "FInput_ISX"

void FInput_ISX::StartupModule()
{
	UE_LOG(Input_ISX, Warning, TEXT("Input_ISX module has been loaded"));
}

void FInput_ISX::ShutdownModule()
{
	UE_LOG(Input_ISX, Warning, TEXT("Input_ISX module has been unloaded"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FInput_ISX, Input_ISX)