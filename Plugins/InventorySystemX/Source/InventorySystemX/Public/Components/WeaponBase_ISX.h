/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once

#include "CoreMinimal.h"
#include "InventoryComponent.h"
#include "Components/ActorComponent.h"
#include "WeaponBase_ISX.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEquipWeapon, bool, Equip, UItem*, Item);

UENUM(BlueprintType)
enum class EWeaponVisibility :uint8
{
	ShowHide UMETA(DisplayName = "Hide/Show"),
	Show,
	Hide
};

class UItem;


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class INVENTORYSYSTEMX_API UWeaponBase_ISX : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UWeaponBase_ISX();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, BlueprintPure,
	Category="Weapon Base Overridable Functions")
	UInventoryComponent* GetInventoryComponent();


	
	UFUNCTION()
	void OnShortcutSelected(const bool IsEmpty, const int32 ShortcutIndex,
	                        UItem* Item);
	
	UFUNCTION()
	void OnShortcutsChanged(const TArray<UItem*>& ItemsInShortcuts);

	UFUNCTION()
	void OnEquipWeapon(const bool Equip, UItem* Item);

	

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:

	//Delegate
	UPROPERTY(BlueprintAssignable, VisibleAnywhere, Category="Weapon Base")
	FOnEquipWeapon OnEquip;

	//References
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon Base")
	UInventoryComponent* InventoryComponent;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category="Weapon Base")
	UItem* EquippedItem;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category="Weapon Base")
	UItem* HidedItem;

	//Functions
	void SetEquippedItem(UItem* Item);


	UFUNCTION(BlueprintCallable, Category="Weapon Base")
	FORCEINLINE UItem* GetEquippedItem() const { return EquippedItem; }


	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Weapon Base")
	void Reload();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Weapon Base")
	void RemoveAmmo(const int32 Remove = 1);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Weapon Base")
	void SetAmmo(const int32 NewAmmo);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, meta=(ExpandEnumAsExecs=Visibility), Category="Equipment Data")
	void HideShowWeapon(const EWeaponVisibility Visibility);

	void HideWeapon();
	void ShowWeapon();

	UFUNCTION(BlueprintCallable, Category="Weapon Base")
	FORCEINLINE bool WeaponIsHidden() const { return HidedItem ? true : false; }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, BlueprintPure, DisplayName="Can Reload?",
	Category="Weapon Base Overridable Functions")
	bool CanReload();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, BlueprintPure, DisplayName="Can Fire?",
	Category="Weapon Base Overridable Functions")
	bool CanFire(int32 BulletsToShot) const;
};
