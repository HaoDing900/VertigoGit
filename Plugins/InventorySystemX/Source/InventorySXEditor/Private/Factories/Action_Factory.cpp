/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/


#include "Action_Factory.h"
#include "InventorySXEditor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Objects/ISX_Action.h"

UAction_Factory::UAction_Factory(const FObjectInitializer& ObjectInitializer): Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UISX_Action::StaticClass();
}


FText UAction_Factory::GetDisplayName() const
{
	return FText::FromString("Action");
}

uint32 UAction_Factory::GetMenuCategories() const
{
	return FInventorySXEditor::InventoryCategory;
}

FString UAction_Factory::GetDefaultNewAssetName() const
{
	return TEXT("Action_");
}

UObject* UAction_Factory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
                                         UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return FKismetEditorUtilities::CreateBlueprint(SupportedClass, InParent, InName, BPTYPE_Normal,
	                                               UBlueprint::StaticClass(),
	                                               UBlueprintGeneratedClass::StaticClass(), CallingContext);
}
