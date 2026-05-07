/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once

#include "CoreMinimal.h"
#include "ISX_Action.h"
#include "Engine/Texture2D.h"
#include "Structs/ItemStruct.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "Item.generated.h"


class UISX_Action;

UENUM(BlueprintType)
enum class EItemType :uint8
{
	Item,
	Weapon,
	Ammo,
};

USTRUCT(BlueprintType)
struct FActionItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Actions")
	bool Show;

	UPROPERTY(BlueprintReadOnly, Category="Actions")
	FText Name;

	UPROPERTY(BlueprintReadOnly, Category="Actions")
	UTexture2D* Icon;

	FActionItem()
	{
		Show = true;
		Name = FText::FromString("");
		Icon = nullptr;
	}
};

class UInventoryComponent;

UCLASS(Abstract, BlueprintType, Blueprintable)
class INVENTORYSYSTEMX_API UItem : public UObject
{
	GENERATED_BODY()

protected:
	virtual UWorld* GetWorld() const override;

public:
#pragma region InternalProperties

	UPROPERTY(BlueprintReadOnly, Replicated, Category="Item")
	EItemRotation Rotation;

	UPROPERTY(BlueprintReadOnly, Replicated, Category="Item")
	int32 Amount;

	//The variable that is in the weapon is responsible for the number of rounds in the clip.
	UPROPERTY(BlueprintReadWrite, Replicated, Category="Ammo")
	int32 Ammo;

	UPROPERTY(BlueprintReadOnly, Replicated, Category="Item")
	TArray<int32> OccupiedSlots;

	UPROPERTY(BlueprintReadOnly, Replicated, Category="Item")
	UInventoryComponent* InventoryComponentReference;

	UPROPERTY(BlueprintReadOnly, Replicated, Category="Item")
	ESlotsType SlotsType;

#pragma endregion InternalProperties

#pragma region ItemSettings

#pragma region Visual

	//Soft Object.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ShowOnlyInnerProperties), Category="Visual")
	FSize_isx Size;

	//Soft Object.
	//Static Mesh for the pickup and inspect item.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual", AdvancedDisplay)
	float PickupMeshSize = 1.f;


#pragma endregion Visual

#pragma  region Options

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Options")
	EItemType Type = EItemType::Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Options",
		meta=(EditCondition = "Type == EItemType::Weapon", EditConditionHides))
	TSubclassOf<UItem> AmmoClass;

	//Soft Object.
	//The picture that is displayed in the lower right corner of the weapon item in the inventory. To the right of amount.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Options",
		meta=(EditCondition = "Type == EItemType::Ammo", EditConditionHides))
	TSoftObjectPtr<UTexture2D> BulletImage;

	//Image width
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Options",
		meta=(EditCondition = "Type == EItemType::Ammo", EditConditionHides))
	float ImageWidthInWidget = 26.f;

	//Is it possible to put several items in one.
	//Weapons don't stack.
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		meta=(EditCondition ="Type == EItemType::Item || Type == EItemType::Ammo", EditConditionHides),
		Category="Options", DisplayName="Stackable?")
	bool Stackable;

	//The number of items that can fit in one stack.
	UPROPERTY(EditAnywhere, BlueprintReadWrite,
		meta=(EditCondition = "Stackable || Type == EItemType::Weapon", EditConditionHides,
			ClampMin=1, UIMin=2, UIMax=100, NoResetToDefault), DisplayName="Max Stack / Ammo",
		Category="Options") //, EditConditionHides
	int32 MaxStack = 2;

#pragma endregion Options


#pragma region Text

	//Item name.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(NoResetToDefault), Category="Text")
	FText Name;

	//Item description.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(NoResetToDefault, MultiLine="true"), Category="Text")
	FText Description;

#pragma endregion TEXT

#pragma region Shortcut

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shortcut")
	bool CanAddToShortcut;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shortcut")
	TSoftObjectPtr<UTexture2D> ShortcutImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shortcut")
	FSize_isx ShortcutSize;

#pragma endregion Shortcut

#pragma region Equip

	UFUNCTION(BlueprintCallable, Category="Equip")
	void EquipOrUnequipItem(UInventoryComponent* InventoryComponent);

	UFUNCTION(BlueprintCallable, Category="Equip")
	void CanEquipOrUnequip(UInventoryComponent* InventoryComponent, FText& UseText, bool& CanUse);


#pragma endregion Equip

#pragma region Examine

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Examine")
	bool CanExamine;

	//SoftObject Static Mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Examine")
	TSoftObjectPtr<UStaticMesh> OverrideMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Examine")
	float ExamineMeshSize = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Examine")
	FRotator ExamineStartRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Examine")
	FVector PivotPointOffset;

#pragma endregion Examine

#pragma region  Properties

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Properties")
	FName Socket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Properties")
	float MainProperty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Properties")
	TMap<FName, float> Properties;

#pragma endregion Properties

#pragma region Actions

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Actions")
	TArray<TSubclassOf<UISX_Action>> Actions;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Actions")
	TArray<FActionItem> GetActionsList(UInventoryComponent* InventoryComponent, APawn* Character);

	bool CanExecuteAction(const int32 Index, UInventoryComponent* InventoryComponent, APawn* Character);

	UFUNCTION(BlueprintCallable, Category="Actions")
	void ExecuteAction(const int32 Index, UInventoryComponent* InventoryComponent, APawn* Character);


#pragma endregion Actions

#pragma region Tags

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tags")
	TArray<FName> Tags;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Tags")
	FORCEINLINE TArray<FName> GetTags() { return Tags; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Tags")
	bool HasTag(const FName Tag);

#pragma endregion Tags

#pragma endregion ItemSettings

#pragma region OverridableFunctions

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Overridable Functions")
	bool CanUse(UInventoryComponent* InventoryComponent, APawn* PlayerPawn, FText& UseText,
	            UMaterialInterface*& IconMaterial);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Overridable Functions")
	void OnUseItem(UInventoryComponent* InventoryComponent, APawn* PlayerPawn, bool& Destroy);

	UFUNCTION(BlueprintCallable, BlueprintPure, BlueprintNativeEvent, Category="Overridable Functions")
	bool CanDestroy(UInventoryComponent* InventoryComponent, APawn* PlayerPawn);

	UFUNCTION(BlueprintCallable, BlueprintPure, BlueprintNativeEvent, Category="Overridable Functions")
	void CanEquip(UInventoryComponent* InventoryComponent, APawn* PlayerPawn, bool& CanEquip,
	              EEquipmentSlotType_Isx& EquipmentType, EItemRotation& ItemRotation) const;


#pragma endregion OverridableFunctions


#pragma region Getters

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Getters")
	bool GetProperty(const FName PropertyName, float& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Getters")
	FORCEINLINE int32 GetAmmo() const { return Ammo; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Getters")
	FORCEINLINE EItemType GetItemType() const { return Type; };

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Getters")
	bool IsStackable() const { return Type == EItemType::Weapon ? false : Stackable; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Getters")
	bool CanCombine(UInventoryComponent* InventoryComponent) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Getters")
	FORCEINLINE int32 GetAmount() const { return Amount; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Getters")
	int32 GetAmmoOrAmount() const { return IsWeapon() ? Ammo : Amount; }

	//-1 if None
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Getters")
	int32 GetSlotIndex() { return OccupiedSlots.IsValidIndex(0) ? OccupiedSlots[0] : -1; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Getters")
	bool IsWeapon() const
	{
		return Type == EItemType::Weapon ? true : false;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Getters")
	bool IsAmmo() const
	{
		return Type == EItemType::Ammo ? true : false;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Getters")
	int32 GetMaxAmmo() const
	{
		return MaxStack;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Getters")
	TSubclassOf<UItem> GetAmmoClass()
	{
		return AmmoClass;
	}


#pragma endregion Getters

#pragma region Setters

	void SetAmmo(const int32 InAmmo) { Ammo = FMath::Clamp(InAmmo, 0, MaxStack); }

#pragma endregion Setters

private:
	//Replication
	virtual bool IsSupportedForNetworking() const override { return true; };
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//RPC Support
	virtual bool CallRemoteFunction(UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack) override;
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
};

#undef LOCTEXT_NAMESPACE
