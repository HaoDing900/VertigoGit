/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once


#include "GameFramework/Info.h"
#include "Components/InventoryComponent.h"
#include "Structs/ItemStruct.h"
#include "GlobalStorage.generated.h"


class UInventoryComponent;

UCLASS(ClassGroup=(Custom), Blueprintable)
class INVENTORYSYSTEMX_API AGlobalStorage : public AInfo
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	AGlobalStorage();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	UPROPERTY(BlueprintReadOnly, Replicated, Category= "Storage")
	TArray<FSlotStruct> Slots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ExposeOnSpawn), Category= "Storage")
	FSize_isx Size;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Saves")
	TArray<FSaveData_isx> GetSaveData();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Saves")
	void Server_LoadStorageFromSave(const TArray<FSaveData_isx>& SaveData);

private:
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
