/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityTask.h"
#include "CopyInventoryUtilityTask.generated.h"

/**
 * 
 */
UCLASS()
class UCopyInventoryUtilityTask : public UEditorUtilityTask
{
	GENERATED_BODY()

protected:
	virtual void BeginExecution() override;

public:
	static bool CopyFolderFromEngineToProject();
	static bool RemoveProjectFolder();
	static bool MovePluginContendDirToProjectContentDir();
};
