/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#include "InventorySXEditor.h"

#include "Action_Actions.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Actions/Item_Actions.h"
#include "Tasks/CopyInventoryUtilityTask.h"
#include "ThumbnailRender/UItemThumbnailRenderer.h"


DEFINE_LOG_CATEGORY(InventorySXEditor);

#define LOCTEXT_NAMESPACE "FInventorySXEditor"

uint32 FInventorySXEditor::InventoryCategory;

void FInventorySXEditor::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	//InventoryCategory
	InventoryCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("InventorySystemX")),
	                                                             LOCTEXT("InventorySXCategory", "Inventory System X"));
	
	//Actions
	const TSharedRef<IAssetTypeActions> ActionAction = MakeShareable(new FAction_Actions(InventoryCategory));
	const TSharedRef<IAssetTypeActions> ItemAction = MakeShareable(new FItem_Actions(InventoryCategory));


	AssetTools.RegisterAssetTypeActions(ItemAction);
	AssetTools.RegisterAssetTypeActions(ActionAction);
	
	CreatedAssetTypeActions.Add(ItemAction);
	CreatedAssetTypeActions.Add(ActionAction);

	//Thumbnail
	UThumbnailManager::Get().UnregisterCustomRenderer(UBlueprint::StaticClass());
	UThumbnailManager::Get().RegisterCustomRenderer(UBlueprint::StaticClass(),
	                                                UUItemThumbnailRenderer::StaticClass());
	//Console Command
	const FString Command = TEXT("isx.CopyInventoryToProjectFolder");
	const FString Description = TEXT("Copy Inventory Plugin From Engine Folder To Project Directory");
	IConsoleManager::Get().RegisterConsoleCommand(
		*Command,
		*Description,
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UCopyInventoryUtilityTask::CopyFolderFromEngineToProject();
		}),
		ECVF_Default
	);
}

void FInventorySXEditor::ShutdownModule()
{
	if (UObjectInitialized())
	{
		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
			for (TSharedPtr<IAssetTypeActions> TypeAction : CreatedAssetTypeActions)
			{
				AssetTools.UnregisterAssetTypeActions(TypeAction.ToSharedRef());
			}
			CreatedAssetTypeActions.Empty();
		}

		UThumbnailManager::Get().UnregisterCustomRenderer(UBlueprint::StaticClass());

		IConsoleObject* ConsoleObject = IConsoleManager::Get().FindConsoleObject(
			TEXT("isx.CopyInventoryToProjectFolder"));
		if (ConsoleObject != nullptr)
		{
			IConsoleManager::Get().UnregisterConsoleObject(ConsoleObject);
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FInventorySXEditor, InventorySXEditor)
