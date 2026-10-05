#include "Save/VTGSaveCoordinator.h"
#include "Save/VTGSaveStatics.h"
#include "Save/VTGSaveable.h"
#include "Save/VTGPlayerProgressComponent.h"
#include "VTGGameInstanceBase.h"

#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "Camera/PlayerCameraManager.h"
#include "Loading/VTGMediaLoadingPageSystem.h"

#include "NarrativeComponent.h"

// Checkpoint bookkeeping lives in the persistent flags, so it is saved and restored with them.
const FName UVTGSaveCoordinator::CheckpointIdKey(TEXT("VTG.Checkpoint"));
const FName UVTGSaveCoordinator::CheckpointLevelKey(TEXT("VTG.CheckpointLevel"));
const FName UVTGSaveCoordinator::CheckpointOrderKey(TEXT("VTG.CheckpointOrder"));

namespace
{
	// Seconds of fade on each side of a retry.
	constexpr float RetryFadeTime = 0.4f;
}

// ----------------------------------------------------------------------------------------------
// Slot name helpers - one logical slot maps to several files, all derived from the same index so
// they stay together. Narrative gets its own per-slot file name too.
// ----------------------------------------------------------------------------------------------

FString UVTGSaveCoordinator::SaveSlotName(int32 Slot)
{
	return FString::Printf(TEXT("VTG_Slot_%d"), Slot);
}

FString UVTGSaveCoordinator::ManifestSlotName(int32 Slot)
{
	return FString::Printf(TEXT("VTG_Manifest_%d"), Slot);
}

FString UVTGSaveCoordinator::NarrativeSaveName(int32 Slot)
{
	return FString::Printf(TEXT("VTG_Narrative_%d"), Slot);
}

// ----------------------------------------------------------------------------------------------
// Subsystem lifecycle
// ----------------------------------------------------------------------------------------------

void UVTGSaveCoordinator::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// PostLoadMapWithWorld fires after a level finishes loading - our cue to apply a pending Load.
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UVTGSaveCoordinator::HandlePostLoadMap);
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UVTGSaveCoordinator::HandlePreLoadMap);
}

void UVTGSaveCoordinator::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	Super::Deinitialize();
}

// ----------------------------------------------------------------------------------------------
// Save
// ----------------------------------------------------------------------------------------------

bool UVTGSaveCoordinator::SaveToSlot(int32 Slot, const FString& UserLabel)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	UVTGSaveGame* SaveObj = Cast<UVTGSaveGame>(UGameplayStatics::CreateSaveGameObject(UVTGSaveGame::StaticClass()));
	if (!SaveObj)
	{
		return false;
	}

	// Slot meta (also copied into the standalone manifest file below).
	FVTGSlotMeta& Meta = SaveObj->Meta;
	Meta.SlotIndex   = Slot;
	Meta.DisplayLabel = UserLabel.IsEmpty() ? FString::Printf(TEXT("Slot %d"), Slot) : UserLabel;
	Meta.LevelName   = UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefix=*/true);
	Meta.SaveTimeUtc = FDateTime::UtcNow();
	Meta.SaveVersion = VTG_SAVE_VERSION;
	Meta.bIsValid    = true;
	if (UVTGGameInstanceBase* GI = Cast<UVTGGameInstanceBase>(GetGameInstance()))
	{
		Meta.Stage = GI->GetCurrentStage();
	}

	// Vertigo-owned state: player status + every IVTGSaveable actor in the level.
	GatherWorldState(World, SaveObj);

	// Checkpoint flags (e.g. "L2StreetFightPhase = Combat") ride along in the save.
	SaveObj->PersistentInts = PersistentInts;
	SaveObj->PersistentNames = PersistentNames;

	// Write the main payload + the lightweight manifest.
	bool bOk = UGameplayStatics::SaveGameToSlot(SaveObj, SaveSlotName(Slot), 0);

	if (UVTGSlotManifest* Manifest = Cast<UVTGSlotManifest>(UGameplayStatics::CreateSaveGameObject(UVTGSlotManifest::StaticClass())))
	{
		Manifest->Meta = Meta;
		bOk &= UGameplayStatics::SaveGameToSlot(Manifest, ManifestSlotName(Slot), 0);
	}

	// Narrative keeps its own file (quests + completed-task list) - drive it directly.
	if (UNarrativeComponent* NC = FindNarrativeComponent(World))
	{
		NC->Save(NarrativeSaveName(Slot), 0);
	}

	// ISX inventory and any other Blueprint-only system save to the same slot.
	OnSaveSubsystems.Broadcast(Slot);

	OnSlotSaved.Broadcast(Slot);
	return bOk;
}

void UVTGSaveCoordinator::GatherWorldState(UWorld* World, UVTGSaveGame* SaveObj)
{
	if (!World || !SaveObj)
	{
		return;
	}

	// Player progress component + pawn transform.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (UVTGPlayerProgressComponent* Prog = Pawn->FindComponentByClass<UVTGPlayerProgressComponent>())
			{
				UVTGSaveStatics::SerializeSaveGameProperties(Prog, SaveObj->PlayerData);
			}
			SaveObj->bHasPlayerTransform = true;
			SaveObj->PlayerTransform = RespawnOverride.Get(Pawn->GetActorTransform());
		}
	}

	// Every actor that opts into saving.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->Implements<UVTGSaveable>())
		{
			continue;
		}

		// Stable key: the actor's override if it gave one, else its (level-stable) name.
		FName SaveId = IVTGSaveable::Execute_GetSaveId(Actor);
		if (SaveId == NAME_None)
		{
			SaveId = Actor->GetFName();
		}

		FVTGActorRecord Rec;
		Rec.SaveId = SaveId;
		UVTGSaveStatics::SerializeSaveGameProperties(Actor, Rec.Data);
		if (IVTGSaveable::Execute_ShouldSaveTransform(Actor))
		{
			Rec.bHasTransform = true;
			Rec.Transform = Actor->GetActorTransform();
		}

		SaveObj->ActorRecords.Add(SaveId, Rec);
	}
}

// ----------------------------------------------------------------------------------------------
// Load (deferred until the saved map exists)
// ----------------------------------------------------------------------------------------------

bool UVTGSaveCoordinator::LoadFromSlot(int32 Slot)
{
	if (!DoesSlotExist(Slot))
	{
		return false;
	}

	UVTGSaveGame* SaveObj = Cast<UVTGSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName(Slot), 0));
	if (!SaveObj)
	{
		return false;
	}

	// (Migration hook: if SaveObj->Meta.SaveVersion < VTG_SAVE_VERSION, upgrade it here.)

	// Restore checkpoint flags NOW - before the map opens - so actors can read them in BeginPlay.
	PersistentInts = SaveObj->PersistentInts;
	PersistentNames = SaveObj->PersistentNames;
	ResumedCheckpoint = GetPersistentName(CheckpointLevelKey) == FName(*SaveObj->Meta.LevelName) ? GetPersistentName(CheckpointIdKey) : NAME_None;

	// Hold the rest of the data across the upcoming map change; HandlePostLoadMap applies it.
	PendingLoad = SaveObj;
	PendingLoadSlot = Slot;

	// Carry the stage across, exactly like UVTGLevelStatics::OpenLevelWithStage does.
	if (UVTGGameInstanceBase* GI = Cast<UVTGGameInstanceBase>(GetGameInstance()))
	{
		GI->SetCurrentStage(SaveObj->Meta.Stage);
	}

	UGameplayStatics::OpenLevel(this, FName(*SaveObj->Meta.LevelName));
	return true;
}

void UVTGSaveCoordinator::HandlePreLoadMap(const FString& MapName)
{
	if (!PendingLoad)
	{
		// Plain level change (or a checkpoint-less retry): fresh start, no checkpoint in this level yet.
		ResumedCheckpoint = NAME_None;
		PersistentNames.Remove(CheckpointIdKey);
		PersistentNames.Remove(CheckpointLevelKey);
		PersistentInts.Remove(CheckpointOrderKey);
	}
}

void UVTGSaveCoordinator::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld)
	{
		return;
	}

	// Retry: stay black until the restored state is in place - otherwise the player is seen at the
	// PlayerStart for a frame before being moved to the checkpoint.
	APlayerController* RetryPC = bRetrying ? UGameplayStatics::GetPlayerController(LoadedWorld, 0) : nullptr;
	if (RetryPC && RetryPC->PlayerCameraManager)
	{
		RetryPC->PlayerCameraManager->SetManualCameraFade(1.f, FLinearColor::Black, false);
	}

	// Wait one tick so every actor's BeginPlay has run before we overwrite their state.
	TWeakObjectPtr<UVTGSaveCoordinator> WeakThis(this);
	TWeakObjectPtr<UWorld> WeakWorld(LoadedWorld);
	LoadedWorld->GetTimerManager().SetTimerForNextTick([WeakThis, WeakWorld]()
	{
		UVTGSaveCoordinator* Self = WeakThis.Get();
		UWorld* World = WeakWorld.Get();
		if (!Self || !World)
		{
			return;
		}

		if (Self->bRetrying)
		{
			Self->bRetrying = false;
			if (UVTGMediaLoadingPageSystem* LoadingPage = Self->GetGameInstance()->GetSubsystem<UVTGMediaLoadingPageSystem>())
			{
				LoadingPage->SetLoadingPageEnabled(true);
			}
			APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
			if (PC && PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, RetryFadeTime, FLinearColor::Black, false, false);
			}
		}

		if (!Self->PendingLoad)
		{
			return;
		}

		const int32 Slot = Self->PendingLoadSlot;

		// 1. Vertigo-owned state (player + level actors).
		Self->ApplyWorldState(World, Self->PendingLoad);

		// 2. Narrative quest/task state.
		if (UNarrativeComponent* NC = Self->FindNarrativeComponent(World))
		{
			NC->Load(NarrativeSaveName(Slot), 0);
		}

		// 3. ISX inventory + any Blueprint-only system.
		Self->OnLoadSubsystems.Broadcast(Slot);
		Self->OnSlotLoaded.Broadcast(Slot);

		Self->PendingLoad = nullptr;
		Self->PendingLoadSlot = INDEX_NONE;
	});
}

void UVTGSaveCoordinator::ApplyWorldState(UWorld* World, UVTGSaveGame* SaveObj)
{
	if (!World || !SaveObj)
	{
		return;
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (UVTGPlayerProgressComponent* Prog = Pawn->FindComponentByClass<UVTGPlayerProgressComponent>())
			{
				UVTGSaveStatics::DeserializeSaveGameProperties(Prog, SaveObj->PlayerData);
			}
			if (SaveObj->bHasPlayerTransform)
			{
				Pawn->SetActorTransform(SaveObj->PlayerTransform, false, nullptr, ETeleportType::TeleportPhysics);
				// The camera follows the controller, which still faces the PlayerStart's way.
				PC->SetControlRotation(SaveObj->PlayerTransform.Rotator());
			}
		}
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->Implements<UVTGSaveable>())
		{
			continue;
		}

		FName SaveId = IVTGSaveable::Execute_GetSaveId(Actor);
		if (SaveId == NAME_None)
		{
			SaveId = Actor->GetFName();
		}

		if (const FVTGActorRecord* Rec = SaveObj->ActorRecords.Find(SaveId))
		{
			UVTGSaveStatics::DeserializeSaveGameProperties(Actor, Rec->Data);
			if (Rec->bHasTransform)
			{
				Actor->SetActorTransform(Rec->Transform, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------
// Misc / queries
// ----------------------------------------------------------------------------------------------

bool UVTGSaveCoordinator::AutoSave()
{
	return SaveToSlot(AutoSaveSlot, TEXT("Autosave"));
}

bool UVTGSaveCoordinator::SaveCheckpoint(FName CheckpointId, const FTransform& RespawnTransform, int32 Order)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || CheckpointId.IsNone())
	{
		return false;
	}

	const FName Level(*UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefix=*/true));
	if (GetPersistentName(CheckpointLevelKey) == Level && Order < GetPersistentInt(CheckpointOrderKey, 0))
	{
		return false; // walked back over an older checkpoint
	}

	SetPersistentName(CheckpointIdKey, CheckpointId);
	SetPersistentName(CheckpointLevelKey, Level);
	SetPersistentInt(CheckpointOrderKey, Order);

	RespawnOverride = RespawnTransform;
	const bool bOk = AutoSave();
	RespawnOverride.Reset();
	return bOk;
}

bool UVTGSaveCoordinator::RetryFromCheckpoint()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || bRetrying)
	{
		return false;
	}

	bRetrying = true;
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, RetryFadeTime, FLinearColor::Black, false, /*bHoldWhenFinished=*/true);
	}
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &UVTGSaveCoordinator::DoRetry), RetryFadeTime, false);
	return true;
}

void UVTGSaveCoordinator::DoRetry()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		bRetrying = false;
		return;
	}

	// A retry is the same level again: no tunnel video, just the black fade.
	if (UVTGMediaLoadingPageSystem* LoadingPage = GetGameInstance()->GetSubsystem<UVTGMediaLoadingPageSystem>())
	{
		LoadingPage->SetLoadingPageEnabled(false);
	}

	// Only an autosave of THIS level counts - also covers BPLMs that call Auto Save directly.
	const FString Level = UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefix=*/true);
	if (UVTGSlotManifest* Manifest = Cast<UVTGSlotManifest>(UGameplayStatics::LoadGameFromSlot(ManifestSlotName(AutoSaveSlot), 0)))
	{
		if (Manifest->Meta.LevelName == Level && LoadFromSlot(AutoSaveSlot))
		{
			return;
		}
	}

	// No checkpoint here yet: restart the level from the top (the stage stays on the GameInstance).
	UGameplayStatics::OpenLevel(World, FName(*Level));
}

void UVTGSaveCoordinator::ClearCheckpoint()
{
	DeleteSlot(AutoSaveSlot);
	PersistentNames.Remove(CheckpointIdKey);
	PersistentNames.Remove(CheckpointLevelKey);
	PersistentInts.Remove(CheckpointOrderKey);
	ResumedCheckpoint = NAME_None;
}

bool UVTGSaveCoordinator::DeleteSlot(int32 Slot)
{
	const bool bOk = UGameplayStatics::DeleteGameInSlot(SaveSlotName(Slot), 0);
	UGameplayStatics::DeleteGameInSlot(ManifestSlotName(Slot), 0);
	UGameplayStatics::DeleteGameInSlot(NarrativeSaveName(Slot), 0);
	return bOk;
}

bool UVTGSaveCoordinator::DoesSlotExist(int32 Slot) const
{
	return UGameplayStatics::DoesSaveGameExist(SaveSlotName(Slot), 0);
}

void UVTGSaveCoordinator::GetAllSlotMetas(TArray<FVTGSlotMeta>& OutMetas) const
{
	OutMetas.Reset();
	for (int32 i = 0; i < MaxSlots; ++i)
	{
		FVTGSlotMeta Meta;
		Meta.SlotIndex = i;

		if (UGameplayStatics::DoesSaveGameExist(ManifestSlotName(i), 0))
		{
			if (UVTGSlotManifest* M = Cast<UVTGSlotManifest>(UGameplayStatics::LoadGameFromSlot(ManifestSlotName(i), 0)))
			{
				Meta = M->Meta;
			}
		}

		OutMetas.Add(Meta);
	}
}

UNarrativeComponent* UVTGSaveCoordinator::FindNarrativeComponent(UWorld* World) const
{
	if (!World)
	{
		return nullptr;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return nullptr;
	}

	// Narrative's component usually lives on the PlayerController; fall back to pawn / game state.
	if (UNarrativeComponent* NC = PC->FindComponentByClass<UNarrativeComponent>())
	{
		return NC;
	}
	if (APawn* Pawn = PC->GetPawn())
	{
		if (UNarrativeComponent* NC = Pawn->FindComponentByClass<UNarrativeComponent>())
		{
			return NC;
		}
	}
	if (AGameStateBase* GS = World->GetGameState())
	{
		if (UNarrativeComponent* NC = GS->FindComponentByClass<UNarrativeComponent>())
		{
			return NC;
		}
	}
	return nullptr;
}
