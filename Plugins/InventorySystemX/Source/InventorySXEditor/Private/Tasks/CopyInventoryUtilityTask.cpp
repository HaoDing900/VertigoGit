/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/


#include "CopyInventoryUtilityTask.h"
#include "Modules/ModuleManager.h"


void UCopyInventoryUtilityTask::BeginExecution()
{
	Super::BeginExecution();
	SetTaskNotificationText(FText::FromString(TEXT("Copy Inventory Plugin Folder To Project Dir...")));
	if (CopyFolderFromEngineToProject())
	{
		SetTaskNotificationText(FText::FromString(TEXT("The folder was copied successfully.")));
	}
	else
	{
		SetTaskNotificationText(FText::FromString(TEXT("Error copying folder.")));
	}
	FinishExecutingTask();
}


bool UCopyInventoryUtilityTask::CopyFolderFromEngineToProject()
{
	const FString SourcePath = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("Marketplace/InventorySystemX"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if(!PlatformFile.DirectoryExists(*FPaths::ProjectPluginsDir()))
	{
		PlatformFile.CreateDirectory(*FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins")));
	}
	const FString DestinationPath = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("InventorySystemX"));

	
	if (PlatformFile.CopyDirectoryTree(*DestinationPath, *SourcePath, false))
	{
		UE_LOG(LogTemp, Warning, TEXT("The Inventory folder was copied successfully."))
		return true;
	}
	UE_LOG(LogTemp, Error, TEXT("Error copying folder."))
	return false;
}

bool UCopyInventoryUtilityTask::RemoveProjectFolder()
{
	const FString DestinationPath = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("InventorySystemX"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	if (PlatformFile.DeleteDirectory(*DestinationPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("The Inventory folder was removed successfully."))
		return true;
	}
	UE_LOG(LogTemp, Error, TEXT("Error removing folder."))
	return false;
}

bool UCopyInventoryUtilityTask::MovePluginContendDirToProjectContentDir()
{

	const FString SourcePath = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("InventorySystemX/Content"));
	const FString DestinationPath = FPaths::ProjectContentDir();
	
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	
	if (PlatformFile.CopyDirectoryTree(*DestinationPath, *SourcePath, false))
	{
		PlatformFile.DeleteDirectoryRecursively(*SourcePath);
		UE_LOG(LogTemp, Warning, TEXT("The content folder was copied successfully."))
		
		return true;
	}
	UE_LOG(LogTemp, Error, TEXT("Error copying folder."))
	return false;
}
