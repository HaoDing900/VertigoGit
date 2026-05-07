/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once

#include "CoreMinimal.h"
#include "Actors/GlobalStorage.h"
#include "Components/ActorComponent.h"
#include "Structs/ItemStruct.h"
#include "StorageComponent.generated.h"

USTRUCT(BlueprintType)
struct FStorageSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Save")
	TArray<FSaveData_isx> SaveData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Save")
	FGuid GUID;
};

USTRUCT(BlueprintType)
struct FItemAndAmount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storage")
	TSubclassOf<UItem> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storage")
	int32 Amount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storage")
	int32 Ammo;

	FItemAndAmount():
		ItemClass(nullptr),
		Amount(1),
		Ammo(0)
	{
	}
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class INVENTORYSYSTEMX_API UStorageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UStorageComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ExposeOnSpawn), Category="Storage")
	FGuid GUID;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Storage")
	TArray<FSlotStruct> Slots;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, meta=(ExposeOnSpawn), Category="Storage")
	FSize_isx Size;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Storage")
	FSize_isx& GetSize() { return IsGlobalStorage ? IsValid(GetGlobalStorage()) ? GlobalStorage->Size : Size : Size; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Storage")
	TArray<FSlotStruct>& GetSlots()
	{
		return IsGlobalStorage
			       ? IsValid(GetGlobalStorage())
				         ? GlobalStorage->Slots
				         : Slots
			       : Slots;
	};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ExposeOnSpawn), Category="Storage")
	TArray<FItemAndAmount> ItemsToAdd;


	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, meta=(ExposeOnSpawn), Category="Storage")
	bool IsGlobalStorage = false;

	UPROPERTY(Replicated)
	AGlobalStorage* GlobalStorage;

	UFUNCTION(BlueprintCallable, Category="Storage")
	void AddItem(TSubclassOf<UItem> ItemClass, int32 Amount, int32 Ammo);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, BlueprintPure, Category="Storage")
	bool CanOpen();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Storage")
	AGlobalStorage* GetGlobalStorage();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Storage")
	void Server_CloneInventory(UInventoryComponent* InventoryComponent);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Storage")
	void Server_CloneInventory_2(const TArray<FSlotStruct>& InSlots, const FSize_isx InSize);


#pragma region Saves

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Saves")
	FStorageSaveData GetSaveData();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Saves")
	void Server_LoadStorageFromSave(const TArray<FSaveData_isx>& SaveData);


#pragma endregion Saves

private:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
};
