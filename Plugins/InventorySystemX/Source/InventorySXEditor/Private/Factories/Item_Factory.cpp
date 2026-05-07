/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/


#include "Item_Factory.h"
#include "InventorySXEditor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Objects/Item.h"

UItem_Factory::UItem_Factory(const FObjectInitializer& ObjectInitializer): Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UItem::StaticClass();
}


FText UItem_Factory::GetDisplayName() const
{
	return FText::FromString("Item");
}

uint32 UItem_Factory::GetMenuCategories() const
{
	return FInventorySXEditor::InventoryCategory;
}

FString UItem_Factory::GetDefaultNewAssetName() const
{
	return TEXT("Item_");
}

UObject* UItem_Factory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
                                         UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return FKismetEditorUtilities::CreateBlueprint(SupportedClass, InParent, InName, BPTYPE_Normal,
	                                               UBlueprint::StaticClass(),
	                                               UBlueprintGeneratedClass::StaticClass(), CallingContext);
}
