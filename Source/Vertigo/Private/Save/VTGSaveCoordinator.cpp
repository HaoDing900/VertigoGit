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
#include "Containers/Ticker.h"
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
	WorldActorsInitializedHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UVTGSaveCoordinator::HandleWorldActorsInitialized);
}

void UVTGSaveCoordinator::Deinitialize()
{
	CancelPendingRestore();
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldActorsInitializedHandle);
	Super::Deinitialize();
}

void UVTGSaveCoordinator::HandleWorldActorsInitialized(const FActorsInitializedParams& Params)
{
	UWorld* World = Params.World;
	if (!World || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	DestroyedActorIds.Reset();
	World->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateUObject(this, &UVTGSaveCoordinator::HandleActorDestroyed));
}

void UVTGSaveCoordinator::HandleActorDestroyed(AActor* Actor)
{
	// Only actors that came with the map: anything spawned at runtime will simply not be re-spawned.
	if (Actor && Actor->HasAnyFlags(RF_WasLoaded) && Actor->Implements<UVTGSaveable>())
	{
		DestroyedActorIds.Add(ResolveSaveId(Actor));
	}
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

FName UVTGSaveCoordinator::ResolveSaveId(AActor* Actor)
{
	// The player pawn is spawned at runtime, so its name is not a stable key; give it a fixed one.
	if (const APawn* Pawn = Cast<APawn>(Actor); Pawn && Pawn->IsPlayerControlled())
	{
		return TEXT("VTG.Player");
	}
	// Otherwise the actor's override if it gave one, else its (level-stable) name.
	const FName SaveId = IVTGSaveable::Execute_GetSaveId(Actor);
	return SaveId.IsNone() ? Actor->GetFName() : SaveId;
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
			if (UInventoryComponent* Inventory = Pawn->FindComponentByClass<UInventoryComponent>())
			{
				SaveObj->bHasPlayerInventory = true;
				SaveObj->PlayerInventory = Inventory->GetSaveData();
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

		const FName SaveId = ResolveSaveId(Actor);

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

	SaveObj->DestroyedActors = DestroyedActorIds.Array();
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
	CancelPendingRestore();
	if (!PendingLoad)
	{
		// Plain level change (or a checkpoint-less retry): fresh start, no checkpoint in this level yet.
		ResumedCheckpoint = NAME_None;
		PersistentNames.Remove(CheckpointIdKey);
		PersistentNames.Remove(CheckpointLevelKey);
		PersistentInts.Remove(CheckpointOrderKey);
	}
}

void UVTGSaveCoordinator::CancelPendingRestore()
{
	FTSTicker::GetCoreTicker().RemoveTicker(RestoreTicker);
	FTSTicker::GetCoreTicker().RemoveTicker(RetryRevealTicker);
	RestoreTicker.Reset();
	RetryRevealTicker.Reset();
}

void UVTGSaveCoordinator::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	CancelPendingRestore();

	// Keep the existing checkpoint flow hidden until its saved state is restored.
	APlayerController* RetryPC = bRetrying ? UGameplayStatics::GetPlayerController(LoadedWorld, 0) : nullptr;
	if (RetryPC && RetryPC->PlayerCameraManager)
	{
		RetryPC->PlayerCameraManager->SetManualCameraFade(1.f, FLinearColor::Black, false);
	}

	// A BeginPlay tutorial may already have paused the world. A world timer would
	// leave both the checkpoint restore and its black camera fade blocked forever.
	TWeakObjectPtr<UWorld> WeakWorld(LoadedWorld);
	RestoreTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
		[this, WeakWorld](float)
	{
		RestoreTicker.Reset();
		UWorld* World = WeakWorld.Get();
		if (!World || GetGameInstance()->GetWorld() != World)
		{
			return false;
		}

		if (PendingLoad)
		{
			const int32 Slot = PendingLoadSlot;
			// Preserve the existing player, actor, quest and inventory restoration.
			ApplyWorldState(World, PendingLoad);
			if (UNarrativeComponent* NC = FindNarrativeComponent(World))
			{
				NC->Load(NarrativeSaveName(Slot), 0);
			}
			OnLoadSubsystems.Broadcast(Slot);
			OnSlotLoaded.Broadcast(Slot);
			PendingLoad = nullptr;
			PendingLoadSlot = INDEX_NONE;
		}

		if (bRetrying)
		{
			bRetrying = false;
			if (auto* LoadingPage = GetGameInstance()->GetSubsystem<UVTGMediaLoadingPageSystem>())
			{
				LoadingPage->SetLoadingPageEnabled(true);
			}
			APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
			if (PC && PC->PlayerCameraManager)
			{
				TWeakObjectPtr<APlayerCameraManager> Camera(PC->PlayerCameraManager);
				const double Start = FPlatformTime::Seconds();
				// Reveal the restored scene even while the tutorial keeps gameplay paused.
				RetryRevealTicker = FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateWeakLambda(this, [this, WeakWorld, Camera, Start](float)
				{
					if (!Camera.IsValid() || !WeakWorld.IsValid() || GetGameInstance()->GetWorld() != WeakWorld.Get())
					{
						RetryRevealTicker.Reset();
						return false;
					}
					const float Alpha = FMath::Clamp(float((FPlatformTime::Seconds() - Start) / RetryFadeTime), 0.f, 1.f);
					Camera->SetManualCameraFade(1.f - Alpha, FLinearColor::Black, false);
					if (Alpha >= 1.f)
					{
						Camera->StopCameraFade();
						RetryRevealTicker.Reset();
						return false;
					}
					return true;
				}));
			}
		}
		return false;
	}));
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
			UInventoryComponent* Inventory = Pawn->FindComponentByClass<UInventoryComponent>();
			if (Inventory && SaveObj->bHasPlayerInventory)
			{
				Inventory->Server_LoadInventoryFromSave(SaveObj->PlayerInventory);
			}
			if (SaveObj->bHasPlayerTransform)
			{
				Pawn->SetActorTransform(SaveObj->PlayerTransform, false, nullptr, ETeleportType::TeleportPhysics);
				// The camera follows the controller, which still faces the PlayerStart's way.
				PC->SetControlRotation(SaveObj->PlayerTransform.Rotator());
			}
		}
	}

	const TSet<FName> Destroyed(SaveObj->DestroyedActors);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->Implements<UVTGSaveable>())
		{
			continue;
		}

		// Gone when the save was made (e.g. a picked-up item): remove the fresh copy the map spawned.
		if (Actor->HasAnyFlags(RF_WasLoaded) && Destroyed.Contains(ResolveSaveId(Actor)))
		{
			Actor->Destroy();
			continue;
		}

		if (const FVTGActorRecord* Rec = SaveObj->ActorRecords.Find(ResolveSaveId(Actor)))
		{
			UVTGSaveStatics::DeserializeSaveGameProperties(Actor, Rec->Data);
			if (Rec->bHasTransform)
			{
				Actor->SetActorTransform(Rec->Transform, false, nullptr, ETeleportType::TeleportPhysics);
			}
			IVTGSaveable::Execute_OnSaveRestored(Actor);
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
