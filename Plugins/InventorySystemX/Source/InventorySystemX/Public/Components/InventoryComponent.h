/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "Objects/Item.h"
#include "InventoryComponent.generated.h"


#pragma region Structs


USTRUCT(BlueprintType)
struct FSlotStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slot")
	int32 Index;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slot")
	bool IsEmpty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slot")
	bool IsPartOfItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slot")
	UItem* ItemReference;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equipment Slot")
	EEquipmentSlotType_Isx EquipmentSlotType;

	FSlotStruct()
		: Index(-1), IsEmpty(true),
		  IsPartOfItem(false),
		  ItemReference(nullptr),
		  EquipmentSlotType(EEquipmentSlotType_Isx::None)
	{
	}
};

USTRUCT(BlueprintType)
struct FSaveData_isx
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Save")
	TSubclassOf<UItem> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Slot")
	ESlotsType SlotsType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Slot")
	EItemRotation Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Slot")
	TArray<int32> OccupiedSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Slot")
	int32 Amount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Slot")
	int32 Ammo;

	FSaveData_isx():
		SlotsType(ESlotsType::Primary),
		Rotation(EItemRotation::Horizontal),
		Amount(1),
		Ammo(0)
	{
	}
};

USTRUCT(BlueprintType)
struct FSaveDataWithShortcuts : public FSaveData_isx
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Save")
	int32 InShortcutSlot;

	FSaveDataWithShortcuts(): InShortcutSlot(0)
	{
	}
};

USTRUCT(BlueprintType)
struct FInventorySaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Save")
	TArray<FSaveDataWithShortcuts> SaveData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Save")
	int32 SelectedShortcut;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Save")
	FString UniqueID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Save")
	int32 EquippedItemSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Save")
	FSize_isx InventorySize;

	FInventorySaveData():
		SelectedShortcut(0),
		UniqueID(""),
		EquippedItemSlot(0)
	{
	}
};


USTRUCT(BlueprintType)
struct FItemDataInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slot")
	UItem* Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slot")
	TArray<int32> Slots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slot")
	EItemRotation Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slot")
	ESlotsType SlotsType;

	FItemDataInfo():
		Item(nullptr),
		Rotation(EItemRotation::Horizontal),
		SlotsType(ESlotsType::Primary)
	{
	}
};

USTRUCT(BlueprintType)
struct FShortcut
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Shortcut")
	bool IsEmpty;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Shortcut")
	int32 Index;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Shortcut")
	UItem* Item;

	FShortcut(): IsEmpty(true), Index(-1), Item(nullptr)
	{
	}
};

USTRUCT(BlueprintType)
struct FStorageData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Storage")
	UItem* Item;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Storage")
	int32 FirstSlot;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Storage")
	int32 Amount;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Storage")
	EItemRotation Rotation;

	FStorageData():
		Item(nullptr),
		FirstSlot(0),
		Amount(1),
		Rotation(EItemRotation::Horizontal)
	{
	}
};

#pragma endregion Structs


#pragma region Delegates

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShortcutsChanged, const TArray<UItem*>&, ItemsInShortcuts);


DECLARE_DYNAMIC_MULTICAST_DELEGATE_EightParams(FOnItemAdded, ESlotsType, SlotsType, TSubclassOf<UItem>, ItemClass,
                                               const TArray<int32>&, Slots, const int32, Amount, const EItemRotation,
                                               Rotation, const bool, CanDestroy, const int32, InShortcutSlot,
                                               const bool, IsEquipped);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemsAdded, const UItem*, ItemReference,
                                             const ESlotsType, SlotsType);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemChangeAmount, const int32, SlotIndex,
                                               const ESlotsType, SlotsType, const int32, Amount);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemRemoved, const TArray<int32>&, Slots,
                                             const ESlotsType, SlotsType);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAllItemsRemoved,
                                            const ESlotsType, SlotsType);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChangingAdditionalSlots,
                                             const bool, TempSlotsOccupied, const bool, HiddenSlotsOccupied);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnChangingAdditionalSlots_Server);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnItemMoved, const bool, IsMoved, const bool, TempSlotsHasItems,
                                              const int32, Index, const ESlotsType, SlotsType);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPrimarySlotsLoaded);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStorageSlotsChanged, const TArray<FSlotStruct>&, Slots);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUpdateStorageSlots, const TArray<FSlotStruct>&, Slots);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChanged, UItem*, Item, const int32, NewAmmo);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnShortcutSelected, const bool, IsEmpty, const int32, ShortcutIndex,
                                               UItem*, Item);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombineResult, const bool, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnDiscardItem, TSubclassOf<UItem>, ItemClass, const int32, Amount,
                                              const int32, Ammo, const ESlotsType, SlotsType);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoad, const int32, SelectedShortcut);


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDraggedItemSwapped, const bool, Swapped);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDraggedItemCombined, const int32, NewDraggedItemAmount);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEquipServer, const bool, Equip, UItem*, Item);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEquip, const bool, Equip, UItem*, Item);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResizeInventory, const FSize_isx, NewSize);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEquipmentSlotChanged_Server, const bool, Equipped, const int32,
                                               SlotIndex, UItem*, Item);


#pragma endregion Delegates

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class INVENTORYSYSTEMX_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UInventoryComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	//Unique Inventory ID for Save Load System
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ExposeOnSpawn), Category="Inventory")
	FString UniqueID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, DisplayName="Inventory Slots Size", Category="Inventory")
	FSize_isx InventorySize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, DisplayName="Temporaty Slots Size", Category="Inventory")
	FSize_isx TempInventorySize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	bool UseTempSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combines")
	//,meta = (RequiredAssetDataTags="RowStructure=CombinesStruct"))
	UDataTable* CombinesDataTable;


	UPROPERTY(Replicated, BlueprintReadOnly, Category="Storage")
	class UStorageComponent* StorageComponent;

	UPROPERTY(BlueprintReadOnly, Replicated, Category="Inventory")
	TArray<FSlotStruct> InventorySlots;

	UPROPERTY(BlueprintReadOnly, Replicated, Category="Inventory")
	TArray<FSlotStruct> TempInventorySlots;

	UPROPERTY(BlueprintReadOnly, Replicated, Category="Inventory")
	TArray<FSlotStruct> HiddenSlots;


	UPROPERTY(BlueprintReadWrite, Category="Inventory")
	TArray<FItemDataInfo> SavedPrimaryItemsArray;

	UPROPERTY(BlueprintReadWrite, Replicated, Category="Inventory")
	UItem* EquippedItem;

	UPROPERTY(BlueprintReadOnly, Replicated, Category="Inventory")
	ESlotsType HiddenItemSlotsType;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equipment")
	TArray<EEquipmentSlotType_Isx> DefaultSlots;

	UPROPERTY(BlueprintReadWrite, Replicated, Category="Equipment")
	TArray<FSlotStruct> EquipmentSlots;

#pragma region Options

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Options")
	bool CanAddUnRemovableItemsToStorage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Options")
	bool CanAddUnRemovableItemsToGlobalStorage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Options")
	bool AddSwappedItemFromStorageToInventoryOnClose;

#pragma endregion Options


#pragma region Interface

	UObject* GetPlayerObject();

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	UObject* PlayerObject;

	UFUNCTION()
	void CallOnDiscardItemInterface(TSubclassOf<UItem> ItemClass, const int32 Amount, const int32 Ammo,
	                                const ESlotsType SlotsType);

#pragma endregion Interface

#pragma region Delegates

	UPROPERTY(BlueprintAssignable, Category="Srver Events")
	FOnShortcutsChanged OnShortcutsChanged_Server;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category="UI Events")
	FOnItemAdded OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnItemsAdded OnItemsAdded;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnItemChangeAmount OnItemChangeAmount;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnItemRemoved OnItemRemoved;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnAllItemsRemoved OnAllItemsRemoved;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnItemMoved OnItemMoved;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnStorageSlotsChanged OnStorageSlotsChanged;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnChangingAdditionalSlots OnChangingAdditionalSlots;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnPrimarySlotsLoaded OnPrimarySlotsLoaded;

	UPROPERTY(BlueprintAssignable, Category="Srver Events")
	FOnDiscardItem OnDiscardItem_Server;

	UPROPERTY(BlueprintAssignable, Category="Srver Events")
	FOnChangingAdditionalSlots_Server OnChangingAdditionalSlots_Server;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnLoad OnLoad;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnCombineResult OnCombineResult;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnUpdateStorageSlots OnUpdateStorageSlots;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnAmmoChanged OnAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category="Srver Events")
	FOnShortcutSelected Server_OnShortcutSelected;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnDraggedItemSwapped OnDraggedItemSwapped;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnDraggedItemCombined OnDraggedItemCombined;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnEquipServer OnEquipServer;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnEquip OnEquip;

	UPROPERTY(BlueprintAssignable, Category="UI Events")
	FOnResizeInventory OnResizeInventory;

	UPROPERTY(BlueprintAssignable, Category="Srver Events")
	FOnEquipmentSlotChanged_Server OnEquipmentSlotChanged_Server;

#pragma endregion Delegates

#pragma region Client Function Delegates

	UFUNCTION(Client, Reliable)
	void Client_OnResizeInventory(const FSize_isx NewSize);

	void Client_OnResizeInventory_Implementation(const FSize_isx NewSize)
	{
		OnResizeInventory.Broadcast(NewSize);
	};


	UFUNCTION(Client, Reliable)
	void Client_OnPrimarySlotsLoaded();

	UFUNCTION(Client, Reliable)
	void Client_OnLoad(const int32 InSelectedShortcut);

	UFUNCTION(Client, Reliable)
	void Client_OnItemAdded(const ESlotsType SlotsType, TSubclassOf<UItem> ItemClass, const TArray<int32>& Slots,
	                        const int32 Amount, const EItemRotation Rotation, const bool CanDestroy,
	                        const int32 ShortcutSlot, const bool IsEquipped);

	UFUNCTION(Client, Reliable)
	void Client_OnItemRemoved(const TArray<int32>& Slots, const ESlotsType SlotsType);

	UFUNCTION(Client, Reliable)
	void Client_OnAllItemsRemoved(const ESlotsType SlotsType);

	UFUNCTION(Client, Reliable)
	void Client_OnItemChangeAmount(const int32 Index, const ESlotsType SlotsType, const int32 Amount);

	UFUNCTION(Client, Reliable)
	void Client_OnItemMoved(const bool IsMoved, const bool TempSlotsHasItems, const int32 Index,
	                        const ESlotsType SlotsType);


	UFUNCTION(Client, Reliable)
	void Client_OnChangingAdditionalSlots(const bool TempSlotsOccupied, const bool HiddenSlotsOccupied);

	UFUNCTION(Client, Unreliable)
	void Client_OnCombineResult(const bool Result);

	void Client_OnCombineResult_Implementation(const bool Result)
	{
		OnCombineResult.Broadcast(Result);
	}


	UFUNCTION(Client, Unreliable)
	void Client_OnAmmoChanged(UItem* Item, const int32 NewAmmo);

	void Client_OnAmmoChanged_Implementation(UItem* Item, const int32 NewAmmo)
	{
		OnAmmoChanged.Broadcast(Item, NewAmmo);
	}

	UFUNCTION(Client, Reliable)
	void Client_OnDraggedItemSwapped(const bool Swapped);

	void Client_OnDraggedItemSwapped_Implementation(const bool Swapped)
	{
		OnDraggedItemSwapped.Broadcast(Swapped);
	}

	UFUNCTION(Client, Reliable)
	void Client_OnDraggedItemCombined(const int32 NewDraggedItemAmount);

	void Client_OnDraggedItemCombined_Implementation(const int32 NewDraggedItemAmount)
	{
		OnDraggedItemCombined.Broadcast(NewDraggedItemAmount);
	}

	UFUNCTION(Client, Reliable)
	void Client_OnEquip(const bool Equip, UItem* Item);

	void Client_OnEquip_Implementation(const bool Equip, UItem* Item)
	{
		OnEquip.Broadcast(Equip, Item);
	}


#pragma endregion Client Function Delegates

#pragma region InventoryFunctions

	UFUNCTION(Server, Reliable)
	void InitializeSlots();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Functions")
	FSize_isx& GetInventorySize(const ESlotsType SlotsType = ESlotsType::Primary);

	UFUNCTION(Server, Reliable)
	void SetInventorySize(const ESlotsType SlotsType, const FSize_isx NewSize);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Functions")
	TArray<FSlotStruct>& GetSlots(const ESlotsType Type = ESlotsType::Primary);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void Server_AddItem_2(TSubclassOf<UItem> ItemClass, int32 Amount, const int32 Ammo, const ESlotsType SlotType,
	                      const bool AddRemainingToTempSlots = false);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Functions")
	int32 CanAddItemCount(TSubclassOf<UItem> ItemClass, int32 Amount = 1,
	                      const ESlotsType SlotsType = ESlotsType::Primary);

	bool CheckSlots(const int32 Index, const int32 Width, const int32 Height, TArray<int32>& EmptySlots,
	                const ESlotsType SlotType = ESlotsType::Primary,
	                const TArray<int32>& ExcludeSlots = TArray<int32>());

	bool CheckSize(const int32 Index, const int32 Width, const int32 Height, const ESlotsType SlotsType);


	UFUNCTION(BlueprintCallable, Category="Inventory Functions")
	bool FindEmptySlotsAdvantage(const TArray<FSlotStruct>& Slots, const ESlotsType SlotsType, const int32 Width,
	                             const int32 Height, EItemRotation& Rotation,
	                             TArray<int32>& EmptySlots, const TArray<int32>& ExcludeSlots);


	UFUNCTION(BlueprintCallable, Category="Inventory Functions")
	bool FindEmptySlots(const ESlotsType SlotsType, const int32 Width, const int32 Height, EItemRotation& Rotation,
	                    TArray<int32>& EmptySlots)
	{
		return FindEmptySlotsAdvantage(GetSlots(SlotsType), SlotsType, Width, Height, Rotation, EmptySlots,
		                               TArray<int32>());
	}


	UFUNCTION(Server, Reliable)
	void AddItemToSlots(TSubclassOf<UItem> ItemClass, const int32 Amount, const EItemRotation Rotation,
	                    const TArray<int32>& Slots, const ESlotsType SlotsType , const int32 Ammo = 1);
	

	void AddItemToSlotsLocal(TSubclassOf<UItem> ItemClass, const int32 Amount, const EItemRotation Rotation,
	                         const TArray<int32>& Slots, UPARAM(ref) TArray<FSlotStruct>& SlotsStruct);


	UFUNCTION(BlueprintCallable, Category="Inventory Functions")
	bool FindItemToStack(const TSubclassOf<UItem> ItemClass, const ESlotsType SlotsType, int32& CanAdd, UItem*& Item)
	{
		return FindItemToStack(ItemClass, GetSlots(SlotsType), CanAdd, Item);
	};

	static bool FindItemToStack(const TSubclassOf<UItem> ItemClass, const TArray<FSlotStruct>& Slots, int32& CanAdd,
	                            UItem*& Item);

	bool GetItemInSlot(const int32 SlotIndex, const ESlotsType SlotType, UItem*& Item);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory")
	void Server_RemoveItemsInSlot(const int32 SlotIndex, const ESlotsType SlotType, const int32 Amount = 1,
	                              const bool RemoveAll = false);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void Server_DiscardItem(const int32 SlotIndex, const ESlotsType SlotType, const int32 Amount = 1,
	                        const bool RemoveAll = false);


	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void Server_RemoveItemByRef(const UItem* Item, const int32 Amount = 1,
	                            const bool RemoveAll = false);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void Server_DiscardAllItems(const ESlotsType SlotsType);

	UFUNCTION(Server, Reliable, Category="Inventory Functions")
	void ClearSlots(const ESlotsType SlotsType, const TArray<int32>& Slots);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void SetItemAmount(UItem* Item, const int32 Amount);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Functions")
	bool GetSlotInfo(const int32 Index, const ESlotsType SlotsType, FSlotStruct& SlotInfo);


	UFUNCTION(BlueprintCallable, Category="Inventory Functions")
	bool CanItemAddedToSlots(const int32 SlotOfItem, const ESlotsType ItemSlotType, const EItemRotation Rotation,
	                         const int32 SlotToAdd, const ESlotsType SlotsTypeToAdd, TArray<int32>& EmptySlots);

	bool IsEmptySlotsForItem(const int32 Index, const int32 Width, const int32 Height, const ESlotsType SlotType,
	                         const EItemRotation Rotation,
	                         TArray<int32>& EmptySlots, const TArray<int32>& ExcludeSlots = TArray<int32>());

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void MoveItem(const int32 SlotOfItem, const ESlotsType ItemSlotType, const EItemRotation Rotation,
	              const int32 SlotToAdd, const ESlotsType SlotsTypeToAdd);

	UFUNCTION()
	void AddExistingItemToSlots(UItem* Item, const ESlotsType SlotsType, const EItemRotation Rotation,
	                            const TArray<int32>& EmptySlots);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Functions")
	bool IsSlotsHaveItems(const ESlotsType SlotsType);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void SavePrimarySlotsInArray();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void LoadPrimarySlotsFromArray();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void Server_ClearAllSlots(const ESlotsType SlotsType);

	UFUNCTION(Server, Reliable)
	void FillSlots(UItem* Item, const TArray<int32>& Slots, const ESlotsType SlotsType);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void UseItem(const int32 Index, const ESlotsType SlotsType);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Functions")
	bool CanCombineItem(TSubclassOf<UItem> ItemClass) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Functions")
	bool CanCombineItems(TSubclassOf<UItem> FirstItemClass, TSubclassOf<UItem> SecondItemClass);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void Server_CombineItems(UItem* ItemOne, UItem* ItemTwo);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Inventory Functions")
	void SplitItem(const int32 Index, ESlotsType SlotType);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Inventory Functions")
	void Server_SplitItem_2(const int32 Index, ESlotsType SlotType, const int32 Split, bool ReverseSplit);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void ResizeSlots(const ESlotsType SlotType, const FSize_isx NewSize);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void InitializeStorage(UStorageComponent* InStorageComponent);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void DeInitializeStorage();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Inventory Functions")
	void SetSlots(const ESlotsType SlotsType, const TArray<FSlotStruct>& Slots);

	UFUNCTION(Client, Reliable)
	void CheckStorageSlotsTimer(const TArray<FStorageData>& StorageData);

	UFUNCTION()
	void CheckStorageSlots(const TArray<FStorageData>& StorageData);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Inventory Functions")
	void Server_AutoSort();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category= "Inventory Functions")
	void Server_StackDraggedItem(const int32 SelectedSlotIndex, const ESlotsType SelectedSlotType,
	                             const int32 DraggedItemSlotIndex, const ESlotsType DraggedItemSlotType,
	                             const EItemRotation DraggedItemRotation);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category= "Inventory Functions")
	int32 CalculateItemAmountAfterStuck(const int32 SelectedSlotIndex, const ESlotsType SelectedSlotType,
	                                    const int32 DraggedItemSlotIndex, const ESlotsType DraggedItemSlotType,
	                                    int32& SelectedItemAmount);


	UFUNCTION(BlueprintCallable, Category= "Inventory Functions")
	TArray<UItem*> GetAllItemsInSlots(TArray<int32> Slots, ESlotsType SlotsType);

	UFUNCTION(BlueprintCallable, Category="Inventory Functions")
	TArray<int32> GetSlotsByItemSize(const int32 FistSlot, const ESlotsType SlotsType, const FSize_isx ItemSize,
	                                 const EItemRotation Rotation);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category= "Inventory Functions")
	bool CanSwapDraggedItem(const int32 ItemToIgnoreSlotIndex, const ESlotsType ItemToIgnoreSlotsType,
	                        const int32 SelectedIndex, const ESlotsType SelectedSlotsType,
	                        TSubclassOf<UItem> DraggedItemClass,
	                        const EItemRotation DraggedItemRotation);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category= "Inventory Functions")
	void Server_SwapDraggedItem(const int32 ItemIndex, const ESlotsType SlotsType, TSubclassOf<UItem> ItemClass,
	                            const int32 SelectedIndex, const ESlotsType SelectedSlotsType,
	                            const EItemRotation DraggedItemRotation);


	UFUNCTION(BlueprintCallable, BlueprintPure, Category= "Inventory Functions")
	bool IsValidSwappedItem() const;

	UFUNCTION()
	void OnAdditionSlotsChanged();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category= "Inventory Functions")
	bool CanStackDraggedItem(const int32 SelectedSlotIndex, const ESlotsType SelectedSlotType,
	                         const int32 DraggedItemSlotIndex, const ESlotsType DraggedItemSlotType,
	                         const EItemRotation DraggedItemRotation);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category= "Inventory Functions")
	void Server_TakeAll();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category= "Inventory Functions")
	void Server_SwapSameSizeItems(const int32 SlotIndex_1, const ESlotsType SlotsType_1, const int32 SlotIndex_2,
	                              const ESlotsType SlotsType_2);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category= "Inventory Functions")
	bool CanSwapSameSizeItems(const UItem* Item_One, const UItem* Item_Two) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category= "Inventory Functions")
	bool CanSwapSameSizeItems_2(const int32 SlotIndex_1, const ESlotsType SlotsType_1, const int32 SlotIndex_2,
	                            const ESlotsType SlotsType_2);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category= "Inventory Functions")
	bool CanSwapEquipmentItem(const int32 DraggedItemIndex, const ESlotsType DraggedItemSlotsType,
	                          const int32 EquipSlotIndex,
	                          const ESlotsType EquipSlotType);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category= "Inventory Functions")
	void Server_SwapSameSizeItems_Equipment(const int32 DraggedItemIndex, const ESlotsType DraggedItemSlotsType,
	                                        const int32 EquipSlotIndex,
	                                        const ESlotsType EquipSlotType);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category= "Inventory Functions")
	void ExecuteAction(const int32 Index, UItem*Item);


#pragma endregion InventoryFunctions

#pragma region Shortcuts

	UPROPERTY(Replicated)
	TArray<FShortcut> Shortcuts;

	UPROPERTY(Replicated)
	int32 SelectedShortcut;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Shortcuts")
	FORCEINLINE TArray<FShortcut>& GetShortcuts() { return Shortcuts; }

	UFUNCTION(BlueprintCallable, Category="Shortcuts")
	bool GetShortcut(const int32 Index, FShortcut& ShortcutData);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Shortcuts")
	void Server_AddItemToShortcut(const int32 Index, UItem* Item);

	TArray<UItem*> GetAllShortcutItems();

	UFUNCTION(Server, Reliable)
	void Server_CheckAndRemoveItemFromShortcuts(UItem* Item);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Shortcuts")
	void Server_RemoveItemFromShortcut(const int32 Index);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Shortcuts")
	int32 FindItemInShortcuts(UItem* Item);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Shortcuts")
	void Server_SelectShortcut(const int32 Index);

	//-1 if there are no free slots
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Shortcuts")
	int32 GetFreeShortcut();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Shortcuts")
	int32 GetSelectedShortcutIndex() const { return SelectedShortcut; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Shortcuts")
	FShortcut GetSelectedShortcutData();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Shortcuts")
	void Server_AddSelectedShortcutAmount(const int32 AddAmount);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Shortcuts")
	void Server_SetSelectedShortcutAmount(const int32 NewAmount);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Shortcuts")
	bool IsShortcutItem(UItem* Item, int32& ShortcutIndex);

#pragma endregion Shortcuts

#pragma region Equipped_Weapon

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Equip")
	void Server_EquipItem(UItem* Item);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Equip")
	void Server_UnequipItem();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Equip")
	void Server_UnequipItem_2(UItem* Item);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Equip")
	FORCEINLINE UItem* GetEquippedItem() const { return EquippedItem; };

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Equip")
	bool IsEquippedItem(UItem* Item) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Equip")
	bool HasEquippedItem() const;


#pragma endregion  Equipped_Weapon


#pragma region Equiped Slots

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Equip")
	void AddItemToEquipmentSlot(const int32 EquipmentSlotIndex, const int32 Index, const ESlotsType SlotsType);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Equip")
	void AddItemToEquipmentSlot_2(const int32 EquipmentSlotIndex, UItem* Item);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Equip")
	int32 FindEquipmentSlotToEquip(const int32 SlotIndex, const ESlotsType SlotType);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Equip")
	void AddNewItemToEquipmentSlot(TSubclassOf<UItem> ItemClass, int32 Amount, const int32 Ammo);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Equip")
	void AddItemToEquipment(TSubclassOf<UItem> ItemClass, int32 Amount, const int32 Ammo);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Equip")
	int32 FindFreeEquipmentSlot(TSubclassOf<UItem> ItemClass, EItemRotation& Rotation);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Equip")
	void EquipEquipmentItem(const int32 Index, const ESlotsType SlotsType);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Equip")
	int32 FindItemInEquipmentSlots(UItem* Item);

	


#pragma endregion Equiped Slots

#pragma region Ammo

	UFUNCTION
	(BlueprintCallable, Server, Reliable, Category="Ammo")
	void Server_SetItemAmmo(UItem* Item, const int32 Ammo);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Ammo")
	void Server_AddItemAmmo(UItem* Item, const int32 AddAmmo);

#pragma endregion Ammo

#pragma region FunctionsForTheUser


	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Extra Inventory Functions")
	APawn* GetInventoryOwnerPawn();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Extra Inventory Functions")
	int32 FindItemInInventory(UItem* Item);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Extra Inventory Functions")
	TArray<UItem*> GetAllItems(const ESlotsType SlotsType);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Extra Inventory Functions")
	TArray<UItem*> GetAllItemsOfClass(TSubclassOf<UItem> ItemClass);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Extra Inventory Functions")
	int32 GetNumberOfItems(const TSubclassOf<UItem> ItemClass);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Extra Inventory Functions")
	void Server_RemoveItemsOfClass(TSubclassOf<UItem> ItemClass, int32 Amount);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Extra Inventory Functions")
	void Server_AddItem(TSubclassOf<UItem> ItemClass, const int32 Amount, const int32 Ammo);

	void Server_AddItem_Implementation(TSubclassOf<UItem> ItemClass, const int32 Amount, const int32 Ammo)
	{
		Server_AddItem_2(ItemClass, Amount, Ammo, ESlotsType::Primary);
	}

	UFUNCTION(Server, Reliable, Category="Extra Inventory Functions")
	void Server_AddExistingItem(UItem* Item, const ESlotsType SlotType);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Extra Inventory Functions")
	void Server_ReloadWeapon(UItem* Weapon);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Extra Inventory Functions")
	void Server_ResizeInventory(const FSize_isx NewSize);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Extra Inventory Functions")
	void Server_ResizeInventoryFromItem(const FSize_isx NewSize, UItem* Resizer);

	UFUNCTION(Server, Reliable)
	void Server_ResizeSlots(const ESlotsType SlotsType, const FSize_isx NewSize);


#pragma endregion FunctionsForTheUser

#pragma region Saves

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Save / Load")
	FInventorySaveData GetSaveData();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Save / Load")
	void Server_LoadInventoryFromSave(const FInventorySaveData& SaveData);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Save / Load")
	void Server_SetUniqueID(const bool GenerateRandom, const FString& InUniqueID);

#pragma endregion Saves

private:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
};
