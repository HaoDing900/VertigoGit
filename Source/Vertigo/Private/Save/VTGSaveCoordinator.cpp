#include "Save/VTGSaveCoordinator.h"
#include "VTGLevelManagerBase.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
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
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "NarrativeUIInputComponent.h"
#include "UI/VTGTerminalWidget.h"
#include "Combat/VTGCombatComponent.h"
#include "UI/VTGSaveIndicator.h"
#include "Camera/PlayerCameraManager.h"
#include "Loading/VTGMediaLoadingPageSystem.h"

#include "NarrativeComponent.h"
#include "UObject/UnrealType.h"

// Checkpoint bookkeeping lives in the persistent flags, so it is saved and restored with them.
const FName UVTGSaveCoordinator::CheckpointIdKey(TEXT("VTG.Checkpoint"));
const FName UVTGSaveCoordinator::StoryAutoSaveStageKey(TEXT("VTG.StoryAutoSaveStage"));
const FName UVTGSaveCoordinator::CombatFlagProperty(TEXT("InCombat"));
const FName UVTGSaveCoordinator::CheckpointLevelKey(TEXT("VTG.CheckpointLevel"));
const FName UVTGSaveCoordinator::CheckpointOrderKey(TEXT("VTG.CheckpointOrder"));

// Off by default: the editor is where level sequences get tuned, and that means long Play and Simulate
// sessions whose BPLM BeginPlay would otherwise autosave into the same files the game reads. See
// UVTGSaveCoordinator::IsStoryAutoSaveSuppressed for why this is the only thing it gates, and why it
// is keyed on the build rather than on the world type.
static TAutoConsoleVariable<int32> CVarAutoSaveInDevRuns(
	TEXT("VTG.AutoSaveInDevRuns"),
	0,
	TEXT("Whether a run from the editor - Play, Simulate or Standalone - writes story autosaves (slots 10/11).\n")
	TEXT("  0 (default): it does not. Manual terminal saves and the slot 0 combat checkpoint still do.\n")
	TEXT("  1: it does, exactly as a packaged build would. Set this while testing the save system itself.\n")
	TEXT("Has no effect in a packaged build, where the check it feeds is compiled out."),
	ECVF_Default);

namespace
{
 const FName MailFields[] = {TEXT("HasNewMail"), TEXT("Mail_CurrentDialogue"), TEXT("Mail_CurrentDialogueID"), TEXT("Mail_Sender_Name"), TEXT("Mail_Sender_Avt")};
 bool IsStoryInteraction(const AActor* Actor)
 {
  if(Actor->ActorHasTag(TEXT("VTG_SmartTriggerInteractionProxy")))return false;
  for (const UClass* C=Actor->GetClass(); C; C=C->GetSuperClass())
   if (C->GetPathName()==TEXT("/Game/Blueprints/BP_0ISXInteractiveCollision.BP_0ISXInteractiveCollision_C")) return true;
  return false;
 }
 void RestoreInteractions(UWorld* World, const UVTGSaveGame* Data, bool Refresh)
 {
  for (TActorIterator<AActor> It(World); It; ++It)
  {
   const bool* Enabled=Data->InteractionEnabled.Find(It->GetFName());
   if (!Enabled || !IsStoryInteraction(*It)) continue;
   if (auto* P=FindFProperty<FBoolProperty>(It->GetClass(), TEXT("CanInteract?")))
   {
    P->SetPropertyValue_InContainer(*It,*Enabled);
    if (Refresh)
     if (auto* Fn=It->FindFunction(*Enabled ? TEXT("EnableInteract") : TEXT("DisableInteract")); Fn && Fn->ParmsSize==0) It->ProcessEvent(Fn,nullptr);
   }
  }
 }

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

 if (PendingLoad && UGameplayStatics::GetCurrentLevelName(World,true)==PendingLoad->Meta.LevelName) RestoreInteractions(World,PendingLoad,false);
 // Before world BeginPlay, including trigger overlaps.
 for (TActorIterator<AVTGLevelManagerBase> It(World); It; ++It) RestoreLevelManager(*It);
 DestroyedActorIds.Reset();
	World->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateUObject(this, &UVTGSaveCoordinator::HandleActorDestroyed));
}

void UVTGSaveCoordinator::HandleActorDestroyed(AActor* Actor)
{
	// Only actors that came with the map: anything spawned at runtime will simply not be re-spawned.
	//
	// This deliberately does NOT require IVTGSaveable. Opting in makes sense for capturing an actor's
	// state - you only serialise what you marked - but it is the wrong default for destruction: a
	// map-placed actor that the game destroyed should stay destroyed, always. Requiring the interface
	// meant the opposite, and since only BP_Player_Sa and BP_Hammer implement it, every ISX collision,
	// smart trigger, restrict volume, blocking volume, removed NPC and enemy that 8 of the BPLMs
	// destroy came straight back on load - while the BPLM variable saying "already done" came back as
	// true. That is the "values restored but the world did not change" bug, at its source.
	if (!Actor || !Actor->HasAnyFlags(RF_WasLoaded) || !ShouldTrackDestruction(Actor))
	{
		return;
	}
	DestroyedActorIds.Add(ResolveSaveId(Actor));
}

bool UVTGSaveCoordinator::ShouldTrackDestruction(const AActor* Actor) const
{
	// The player is never "a thing that stayed destroyed" - it is respawned by the game mode, and
	// recording it would make a load delete the pawn it just restored. IsPlayerControlled() alone is
	// not enough to recognise it: that answers via PlayerState, which does not exist without a game
	// mode, so check the controller and player 0's pawn as well.
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (Pawn->IsPlayerControlled()
			|| Cast<APlayerController>(Pawn->GetController()) != nullptr
			|| Pawn == UGameplayStatics::GetPlayerPawn(Actor->GetWorld(), 0))
		{
			return false;
		}
	}
	// Nor is a level manager: it is the level's brain, its BeginPlay is what restores progress in the
	// first place, and one of them (L2StreetBarFight) destroys itself as part of normal flow.
	if (Actor->IsA<AVTGLevelManagerBase>())
	{
		return false;
	}
	for (const TSubclassOf<AActor>& Excluded : DestructionUntrackedClasses)
	{
		if (Excluded && Actor->IsA(Excluded))
		{
			return false;
		}
	}
	return true;
}

// ----------------------------------------------------------------------------------------------
// Save
// ----------------------------------------------------------------------------------------------

bool UVTGSaveCoordinator::SaveTerminalSlot(int32 Slot,FText& Result)
{
 UWorld* World=GetGameInstance()?GetGameInstance()->GetWorld():nullptr;
 if(!IsManualSlot(Slot)){
  Result=NSLOCTEXT("VTGTerminal","NotYourSlot","This slot belongs to the game, not to you.");return false;
 }
 // One gate for the terminal and the autosave alike, and it hands back the reason to show.
 if(!CanSaveNow(World,Result)) return false;
 const bool Success=SaveToSlot(Slot,FString::Printf(TEXT("Manual Save %02d"),Slot));
 Result=Success?FText::Format(NSLOCTEXT("VTGTerminal","Success","Saved to slot {0}."),FText::AsNumber(Slot)):NSLOCTEXT("VTGTerminal","WriteFailed","The save could not be written. Please try again.");
 return Success;
}

bool UVTGSaveCoordinator::SaveFromTerminal(FText& Result)
{
 UWorld* World=GetGameInstance()?GetGameInstance()->GetWorld():nullptr;
 if(!CanSaveNow(World,Result)) return false;
 TArray<FVTGSlotMeta> Metas;GetAllSlotMetas(Metas);
 int32 Slot=INDEX_NONE;
 for(const auto& Meta:Metas)
  if(IsManualSlot(Meta.SlotIndex) && Meta.bIsValid && Meta.DisplayLabel==TEXT("Terminal Save")){Slot=Meta.SlotIndex;break;}
 if(Slot==INDEX_NONE)for(const auto& Meta:Metas)
  if(IsManualSlot(Meta.SlotIndex) && !DoesSlotExist(Meta.SlotIndex)){Slot=Meta.SlotIndex;break;}
 if(Slot==INDEX_NONE){Result=NSLOCTEXT("VTGTerminal","Full","No empty manual save slot is available.");return false;}
 const bool Success=SaveToSlot(Slot,TEXT("Terminal Save"));
 Result=Success?FText::Format(NSLOCTEXT("VTGTerminal","Success","Saved to slot {0}."),FText::AsNumber(Slot)):NSLOCTEXT("VTGTerminal","WriteFailed","The save could not be written. Please try again.");
 return Success;
}

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

	// Keep the maps the player has already left behind. Pure bookkeeping: nothing reads them yet.
	AccumulateMapProgress(SaveObj);

	// "Leashed?" and the rest of the GameInstance's own Blueprint variables.
	CaptureGameInstanceValues(SaveObj);

	// One logical slot is several files, and the manifest is what marks the slot valid - so it must be
	// written LAST. Written first (as it was), a failure in any later file left a slot that advertised
	// itself as good while holding, say, the previous session's quests.
	bool bOk = UGameplayStatics::SaveGameToSlot(SaveObj, SaveSlotName(Slot), 0);

	// Narrative keeps its own file (quests + completed-task list) - drive it directly.
	if (UNarrativeComponent* NC = FindNarrativeComponent(World))
	{
		bOk &= NC->Save(NarrativeSaveName(Slot), 0);
	}

	// ISX inventory and any other Blueprint-only system save to the same slot.
	OnSaveSubsystems.Broadcast(Slot);

	if (bOk)
	{
		if (UVTGSlotManifest* Manifest = Cast<UVTGSlotManifest>(UGameplayStatics::CreateSaveGameObject(UVTGSlotManifest::StaticClass())))
		{
			Manifest->Meta = Meta;
			bOk &= UGameplayStatics::SaveGameToSlot(Manifest, ManifestSlotName(Slot), 0);
		}
		else
		{
			bOk = false;
		}
	}
	else
	{
		// Leave the old manifest alone: an earlier good save in this slot stays loadable, and a slot
		// that never had one stays invisible rather than half-there.
		UE_LOG(LogTemp, Warning, TEXT("Save to slot %d failed before the manifest; the slot keeps whatever it had"), Slot);
	}

	if (bOk) OnSlotSaved.Broadcast(Slot);
	return bOk;
}

FName UVTGSaveCoordinator::ResolveSaveId(AActor* Actor)
{
	// The player pawn is spawned at runtime, so its name is not a stable key; give it a fixed one.
	if (const APawn* Pawn = Cast<APawn>(Actor); Pawn && Pawn->IsPlayerControlled())
	{
		return TEXT("VTG.Player");
	}
	// Otherwise the actor's override if it gave one, else its (level-stable) name. Destruction is
	// tracked for every map-placed actor, not only the ones implementing the interface, so the
	// interface call has to be guarded rather than assumed.
	const FName SaveId = Actor && Actor->Implements<UVTGSaveable>() ? IVTGSaveable::Execute_GetSaveId(Actor) : NAME_None;
	return SaveId.IsNone() ? Actor->GetFName() : SaveId;
}


void UVTGSaveCoordinator::CaptureGameInstanceValues(UVTGSaveGame* SaveObj)
{
	if (!SaveObj)
	{
		return;
	}
	UVTGSaveStatics::CaptureValueProperties(GetGameInstance(), SaveObj->GameInstanceValues, SaveObj->GameInstanceTypes, ExcludedGameInstanceVariables);
}

void UVTGSaveCoordinator::RestoreGameInstanceValues(const UVTGSaveGame* SaveObj)
{
	if (!SaveObj)
	{
		return;
	}
	// The GameInstance outlives the travel, so writing it here - before OpenLevel - means the next
	// world's BeginPlay and any dialogue condition already see the loaded values. Doing it on the
	// other side of the map change (where PendingLoad is applied) would be a frame too late.
	UVTGSaveStatics::ApplyValueProperties(GetGameInstance(), SaveObj->GameInstanceValues, SaveObj->GameInstanceTypes, ExcludedGameInstanceVariables);
}

void UVTGSaveCoordinator::AccumulateMapProgress(UVTGSaveGame* SaveObj)
{
	if (!SaveObj || SaveObj->Meta.LevelName.IsEmpty())
	{
		return;
	}

	// The current map's entry is rewritten from what GatherWorldState just collected; every other
	// entry is whatever the slate carried in from the slot we loaded (or from an earlier save in
	// this session), so walking A -> B -> save no longer drops A.
	FVTGMapProgress& Current = ProgressByMap.FindOrAdd(SaveObj->Meta.LevelName);
	Current.Managers = SaveObj->LevelProgress;
	Current.InteractionEnabled = SaveObj->InteractionEnabled;
	Current.DestroyedActors = SaveObj->DestroyedActors;

	SaveObj->ProgressByMap = ProgressByMap;
}

void UVTGSaveCoordinator::GatherWorldState(UWorld* World, UVTGSaveGame* SaveObj)
{
	if (!World || !SaveObj)
	{
		return;
	}

 if (APawn* Pawn=UGameplayStatics::GetPlayerPawn(World,0))
  for (FName Name : MailFields)
   if (FProperty* P=FindFProperty<FProperty>(Pawn->GetClass(),Name))
   {
    FString Value;P->ExportText_InContainer(0,Value,Pawn,Pawn,Pawn,PPF_None);
    SaveObj->PlayerMail.Add(Name,Value);
   }
 for (TActorIterator<AActor> It(World); It; ++It)
  if (IsStoryInteraction(*It))
   if (auto* P=FindFProperty<FBoolProperty>(It->GetClass(),TEXT("CanInteract?")))
    SaveObj->InteractionEnabled.Add(It->GetFName(),P->GetPropertyValue_InContainer(*It));

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
			if (SaveObj->Meta.SlotIndex != AutoSaveSlot)
   {
    if (auto* Health=FindFProperty<FNumericProperty>(Pawn->GetClass(),TEXT("Health")); Health && (Health->IsFloatingPoint() || Health->IsInteger()))
    {
     SaveObj->bHasTerminalHealth=true;
     SaveObj->TerminalHealth=Health->IsFloatingPoint()?Health->GetFloatingPointPropertyValue(Health->ContainerPtrToValuePtr<void>(Pawn)):double(Health->GetSignedIntPropertyValue(Health->ContainerPtrToValuePtr<void>(Pawn)));
    }
   }
   SaveObj->bHasPlayerTransform = true;
			SaveObj->PlayerTransform = RespawnOverride.Get(Pawn->GetActorTransform());
		}
	}

 for (TActorIterator<AVTGLevelManagerBase> It(World); It; ++It)
  It->CaptureLevelProgress(SaveObj->LevelProgress.FindOrAdd(It->GetProgressKey()));

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

	// Dead is not destroyed. UVTGCombatComponent::HandleDeath only sets CombatState and lets Blueprint
	// ragdoll the body, so a killed enemy is still a live actor in the world and a plain reload stood it
	// back up unhurt. Record a dead body as removed instead: the fact worth keeping is "this one is
	// dead", and the body itself is not worth a per-enemy save record.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->HasAnyFlags(RF_WasLoaded) || !ShouldTrackDestruction(Actor)) continue;
		const UVTGCombatComponent* Combat = Actor->FindComponentByClass<UVTGCombatComponent>();
		if (Combat && Combat->GetCombatState() == EVTGCombatState::Dead)
		{
			SaveObj->DestroyedActors.AddUnique(ResolveSaveId(Actor));
		}
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

	// Format migration. v2 and older only ever kept the save's own map, in the flat LevelProgress /
	// InteractionEnabled / DestroyedActors fields; fold that into the per-map container so there is
	// one shape to read from. Those flat fields stay authoritative for the restore path below.
	if (SaveObj->Meta.SaveVersion < 3)
	{
		FVTGMapProgress& Migrated = SaveObj->ProgressByMap.FindOrAdd(SaveObj->Meta.LevelName);
		Migrated.Managers = SaveObj->LevelProgress;
		Migrated.InteractionEnabled = SaveObj->InteractionEnabled;
		Migrated.DestroyedActors = SaveObj->DestroyedActors;
		SaveObj->Meta.SaveVersion = VTG_SAVE_VERSION;
	}
	ProgressByMap = SaveObj->ProgressByMap;

	// GameInstance variables go back NOW, before the map opens (see RestoreGameInstanceValues).
	RestoreGameInstanceValues(SaveObj);

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

	UVTGMediaLoadingPageSystem::OpenLevelWithLoadingScreenByName(this, FName(*SaveObj->Meta.LevelName));
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
	FTSTicker::GetCoreTicker().RemoveTicker(StoryAutoSaveTicker);
	RestoreTicker.Reset();
	RetryRevealTicker.Reset();
	StoryAutoSaveTicker.Reset();
	// A cancelled autosave must not leave the spinner on screen forever.
	bStoryAutoSaveAnnounced = false;
	FinishSaveIndicator();
}

void UVTGSaveCoordinator::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	CancelPendingRestore();

	// Arriving at the main menu with nothing being loaded means no run is in progress, so this is the
	// moment to forget the last one. Doing it here rather than on a New Game button means it needs no
	// Blueprint wiring and cannot be forgotten: Continue and the load menu both set PendingLoad before
	// opening their map, so they are never mistaken for leaving a run.
	if (!PendingLoad && !MainMenuLevelName.IsEmpty()
		&& UGameplayStatics::GetCurrentLevelName(LoadedWorld, /*bRemovePrefix=*/true) == MainMenuLevelName)
	{
		BeginNewGame();
	}

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
 // By now every level manager in this map has had its BeginPlay and claimed its record. Anything left
 // over belongs to a key no manager answers to any more - almost always a renamed BPLM instance. That
 // used to be completely silent, which is how a playthrough quietly loses its progress; say it.
 {
  TSet<FName> Present;
  for (TActorIterator<AVTGLevelManagerBase> It(World); It; ++It) Present.Add(It->GetProgressKey());
  for (const TPair<FName, FVTGLevelProgressRecord>& Record : SaveObj->LevelProgress)
   if (!Present.Contains(Record.Key))
    UE_LOG(LogTemp, Warning, TEXT("Save holds level-manager progress under \"%s\" (class %s) but no level manager in %s answers to that key - it was probably renamed. Set its LevelManagerSaveId to \"%s\" to recover existing saves."),
     *Record.Key.ToString(), *Record.Value.ClassPath, *UGameplayStatics::GetCurrentLevelName(World, true), *Record.Key.ToString());

  // Same story for destroyed actors, and now it covers 16 classes rather than the two that implement
  // the interface - so a renamed ISX collision or trigger quietly comes back. Only the ids that belong
  // to this map can be checked, so an id from another map is not a complaint: match against what is
  // actually here, and only warn for ones nothing in this world has ever answered to.
  TSet<FName> Here;
  for (TActorIterator<AActor> It(World); It; ++It)
   if (It->HasAnyFlags(RF_WasLoaded)) Here.Add(ResolveSaveId(*It));
  const FVTGMapProgress* MapEntry = SaveObj->ProgressByMap.Find(UGameplayStatics::GetCurrentLevelName(World, true));
  if (MapEntry)
   for (const FName& Id : MapEntry->DestroyedActors)
    if (!Here.Contains(Id) && !DestroyedActorIds.Contains(Id))
     UE_LOG(LogTemp, Warning, TEXT("Save says \"%s\" was destroyed in %s, but no actor there answers to that name - it was probably renamed, so it will come back."),
      *Id.ToString(), *UGameplayStatics::GetCurrentLevelName(World, true));
 }
 RestoreInteractions(World,SaveObj,true);
 if (APawn* Pawn=UGameplayStatics::GetPlayerPawn(World,0))
  for (FName Name : MailFields)
   if (const FString* Value=SaveObj->PlayerMail.Find(Name))
    if (FProperty* P=FindFProperty<FProperty>(Pawn->GetClass(),Name))
     P->ImportText_Direct(**Value,P->ContainerPtrToValuePtr<void>(Pawn),Pawn,PPF_None);


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
		if (!Actor)
		{
			continue;
		}

		// Gone when the save was made (an ISX collision the BPLM removed, a trigger it consumed, an
		// NPC it took out of the scene): remove the fresh copy the map just spawned. Checked for every
		// map-placed actor, not only interface implementers - see HandleActorDestroyed.
		if (Actor->HasAnyFlags(RF_WasLoaded) && Destroyed.Contains(ResolveSaveId(Actor)))
		{
			Actor->Destroy();
			continue;
		}

		// Per-actor state, on the other hand, is opt-in: only what asked to be saved is restored.
		if (!Actor->Implements<UVTGSaveable>())
		{
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
    if (SaveObj->bHasTerminalHealth)
    {
        if (APawn* Pawn=UGameplayStatics::GetPlayerPawn(World,0))
        {
            if (auto* Health=FindFProperty<FNumericProperty>(Pawn->GetClass(),TEXT("Health")); Health && (Health->IsFloatingPoint() || Health->IsInteger()))
                            {
                if(Health->IsFloatingPoint()) Health->SetFloatingPointPropertyValue(Health->ContainerPtrToValuePtr<void>(Pawn),SaveObj->TerminalHealth);
                else Health->SetIntPropertyValue(Health->ContainerPtrToValuePtr<void>(Pawn),int64(SaveObj->TerminalHealth));
            }
        }
    }
}

// ----------------------------------------------------------------------------------------------
// Misc / queries
// ----------------------------------------------------------------------------------------------


FName UVTGSaveCoordinator::StoryAutoSaveBeat(UWorld* World, FName Stage) const
{
 // A "beat" is the map you are in plus the stage you are in. Either one changing is a new beat, and
 // nothing else is. Keying on the map matters because only 4 of 14 maps currently set a stage at all,
 // so a stage-only key almost never fires; keying on the stage as well covers a stage advance that
 // reopens the same map. Stage may legitimately be None - a map with no stage still deserves a save.
 if (!World) return NAME_None;
 const FString Level = UGameplayStatics::GetCurrentLevelName(World, /*bRemovePrefix=*/true);
 if (Level.IsEmpty()) return NAME_None;
 return FName(*FString::Printf(TEXT("%s|%s"), *Level, Stage.IsNone() ? TEXT("-") : *Stage.ToString()));
}

bool UVTGSaveCoordinator::StoryAutoSave(FName Stage)
{
 return StoryAutoSave(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr, Stage);
}

bool UVTGSaveCoordinator::IsStoryAutoSaveSuppressed(const UWorld* World) const
{
#if WITH_EDITOR
 // Keyed on the build, not the world type. Checking IsPlayInEditor() covered Play and Simulate (both
 // run as EWorldType::PIE) but missed Standalone, which is a separate UnrealEditor.exe -game process
 // whose world type is Game - so the one mode that has to be used for testing anything the PIE
 // viewport cannot show, like the tunnel loading movie, went on writing into the real save files.
 // WITH_EDITOR is true for every binary launched out of the editor, Standalone included, and false for
 // a packaged game, so one condition covers all three dev modes.
 return World != nullptr && CVarAutoSaveInDevRuns.GetValueOnAnyThread() == 0;
#else
 // Compiled out of a packaged build entirely: there is no console variable to get wrong and no branch
 // to take, which is a stronger guarantee than a runtime check that the shipped game autosaves.
 return false;
#endif
}

bool UVTGSaveCoordinator::StoryAutoSave(UWorld* World, FName Stage)
{
 if (PendingLoad || bRetrying) return false;
 const FName Beat = StoryAutoSaveBeat(World, Stage);
 if (Beat.IsNone()) return false;
 // The editor opt-out, checked in exactly one place: this function is the only funnel every automatic
 // story autosave goes through (BPLM BeginPlay for a map, OnStageBegin for a stage advance), so one
 // early-out covers all of them and there is no second copy of the rule to drift. Deliberately BEFORE
 // the beat stamp and the ticker, so a suppressed save leaves nothing behind - no stamp claiming this
 // beat was written, no pending write - and turning the variable back on needs no reset. Logged rather
 // than silent: "I played it and nothing saved" has to be answerable from the Output Log.
 if (IsStoryAutoSaveSuppressed(World))
 {
  UE_LOG(LogTemp, Display, TEXT("Story autosave for beat %s skipped: VTG.AutoSaveInDevRuns is 0, so a run launched from the editor does not write story autosaves. Set it to 1 to exercise them here; manual terminal saves and the slot %d combat checkpoint are unaffected."), *Beat.ToString(), AutoSaveSlot);
  return false;
 }
 // Only the LAST beat is remembered, not a history of visits: several BPLMs in one map all report
 // stage begin, and BeginPlay can run again without the map or the stage having changed. Walking back
 // into a map you left does re-save, deliberately - the autosave tracks where the player is now.
 if (GetPersistentName(StoryAutoSaveStageKey) == Beat) return false;
 if (StoryAutoSaveTicker.IsValid()) return false;

 // Do NOT write now, and do not write on a fixed delay either. Stage begin runs during BeginPlay, and
 // most maps open straight into an auto-playing entry sequence, so any fixed delay lands inside the
 // cutscene. Poll instead, and take the first quiet moment. The world is captured weakly rather than
 // looked up again: if the map changed in between, the world this beat was scheduled for is gone and
 // the save would describe somewhere the player has already left.
 StoryAutoSaveGiveUpTime = FPlatformTime::Seconds() + double(StoryAutoSaveTimeout);
 bStoryAutoSaveAnnounced = false;
 StoryAutoSaveTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
  [this, Stage, Beat, WeakWorld = TWeakObjectPtr<UWorld>(World)](float) -> bool
  {
   UWorld* World = WeakWorld.Get();
   if (!World || PendingLoad || bRetrying)
   {
    // A load or a map change has taken over; this beat is no longer the one to save.
    StoryAutoSaveTicker.Reset();
    return false;
   }
   // Recomputed: if the stage advanced while we waited, the beat we queued is stale.
   if (StoryAutoSaveBeat(World, Stage) != Beat || GetPersistentName(StoryAutoSaveStageKey) == Beat)
   {
    StoryAutoSaveTicker.Reset();
    return false;
   }

   FText Reason;
   if (!CanSaveNow(World, Reason))
   {
    if (FPlatformTime::Seconds() < StoryAutoSaveGiveUpTime) return true; // keep waiting
    StoryAutoSaveTicker.Reset();
    UE_LOG(LogTemp, Warning, TEXT("Story autosave for beat %s gave up after %.0fs: %s"), *Beat.ToString(), StoryAutoSaveTimeout, *Reason.ToString());
    return false;
   }

   // Put the spinner up and come back next tick. SaveToSlot blocks the game thread, so announcing and
   // writing in the same tick would mean the player's first sight of the indicator is already frozen.
   if (!bStoryAutoSaveAnnounced)
   {
    bStoryAutoSaveAnnounced = true;
    ShowSaveIndicator(World);
    return true;
   }

   StoryAutoSaveTicker.Reset();
   const int32 Slot = PickStoryAutoSaveSlot();
   // Stamp the beat BEFORE writing, so the file itself records which beat it is. Loading it back
   // therefore does not immediately autosave over it again.
   const FName Previous = GetPersistentName(StoryAutoSaveStageKey);
   SetPersistentName(StoryAutoSaveStageKey, Beat);
   // Read the manifest back afterwards: a half-written slot that still claims to be valid is exactly
   // what the second rotating slot exists to survive, and it should not be mistaken for a good save.
   const bool bWritten = SaveToSlot(Slot, FString::Printf(TEXT("Story Autosave - %s"), *Beat.ToString())) && DoesSlotExist(Slot);
   FinishSaveIndicator();
   if (!bWritten)
   {
    if (Previous.IsNone()) PersistentNames.Remove(StoryAutoSaveStageKey);
    else SetPersistentName(StoryAutoSaveStageKey, Previous);
    UE_LOG(LogTemp, Warning, TEXT("Story autosave for beat %s could not be written to slot %d; the other rotating slot still holds the previous beat"), *Beat.ToString(), Slot);
    return false;
   }
   UE_LOG(LogTemp, Display, TEXT("Story autosave written to slot %d for beat %s"), Slot, *Beat.ToString());
   return false;
  }), StoryAutoSavePollInterval); // polled, not every frame: this may have to wait out a whole cutscene
 return true;
}


int32 UVTGSaveCoordinator::PickStoryAutoSaveSlot() const
{
 // Prefer an empty slot; otherwise overwrite the OLDER one, so the newer beat always survives a bad
 // write. Derived from the manifests rather than remembered, so it is still correct next session.
 int32 Best = StoryAutoSaveSlot;
 FDateTime Oldest = FDateTime::MaxValue();
 for (int32 Slot = StoryAutoSaveSlot; Slot < StoryAutoSaveSlot + StoryAutoSaveSlotCount; ++Slot)
 {
  const UVTGSlotManifest* Manifest = Cast<UVTGSlotManifest>(UGameplayStatics::LoadGameFromSlot(ManifestSlotName(Slot), 0));
  if (!Manifest || !Manifest->Meta.bIsValid) return Slot;
  if (Manifest->Meta.SaveTimeUtc < Oldest) { Oldest = Manifest->Meta.SaveTimeUtc; Best = Slot; }
 }
 return Best;
}

bool UVTGSaveCoordinator::IsPlayerInCombat(UWorld* World) const
{
 APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
 if (!Pawn) return false;
 // FindFProperty walks the class's property list, and the save page asks this every time it refreshes
 // while open, so remember the answer for as long as the pawn class stays the same.
 if (CombatFlagClass.Get() != Pawn->GetClass())
 {
  CombatFlagClass = Pawn->GetClass();
  CombatFlagCached = FindFProperty<FBoolProperty>(Pawn->GetClass(), CombatFlagProperty);
 }
 return CombatFlagCached && CombatFlagCached->GetPropertyValue_InContainer(Pawn);
}

bool UVTGSaveCoordinator::CanSaveNow(UWorld* World, FText& OutReason) const
{
 OutReason = FText::GetEmpty();
 if (!World || !UGameplayStatics::GetPlayerPawn(World, 0))
 {
  OutReason = NSLOCTEXT("VTGTerminal", "NoWorld", "Cannot save right now.");
  return false;
 }
 if (PendingLoad || bRetrying)
 {
  OutReason = NSLOCTEXT("VTGTerminal", "Busy", "Cannot save during a level transition.");
  return false;
 }
 // Combat. The pawn's own flag, the same one the combat presentation code reads, so the answer here
 // and what the player sees on screen cannot disagree.
 if (IsPlayerInCombat(World))
 {
  OutReason = NSLOCTEXT("VTGTerminal", "InConflict", "Not available within conflict.");
  return false;
 }
 // A level manager holding a story section closed - see AVTGLevelManagerBase::SetManualSaveBlocked.
 // Asked here rather than kept as a flag on the coordinator so it cannot outlive the map it belongs
 // to: the managers are gone the moment the level is, and with them the block.
 for (TActorIterator<AVTGLevelManagerBase> It(World); It; ++It)
 {
  if (It->IsManualSaveBlocked())
  {
   OutReason = NSLOCTEXT("VTGTerminal", "InStorySection", "Not available during this sequence.");
   return false;
  }
 }
 // A playing level sequence. This is the one that matters most for the story autosave: saving mid
 // cutscene captures a staging transform and story gates that have started but not finished.
 for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
 {
  const ULevelSequencePlayer* Player = It->GetSequencePlayer();
  if (Player && Player->IsPlaying())
  {
   OutReason = NSLOCTEXT("VTGTerminal", "InCutscene", "Not available during a cutscene.");
   return false;
  }
 }
 // Dialogue or a Note owning the UI input. The terminal owns it too while it is open, and that is not
 // a reason to refuse - it is the thing asking.
 if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
  if (const UNarrativeUIInputComponent* Input = UNarrativeUIInputComponent::Find(PC))
   if (const UObject* Owner = Input->GetActiveOwner(); Owner && !Owner->IsA<UVTGTerminalWidget>())
   {
    OutReason = NSLOCTEXT("VTGTerminal", "InDialogue", "Not available during a conversation.");
    return false;
   }
 return true;
}



void UVTGSaveCoordinator::ShowSaveIndicator(UWorld* World)
{
 APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
 if (!PC) return;
 if (!SaveIndicator || SaveIndicator->GetWorld() != World)
 {
  SaveIndicator = CreateWidget<UVTGSaveIndicator>(PC, UVTGSaveIndicator::StaticClass(), TEXT("VTGSaveIndicator"));
 }
 if (!SaveIndicator) return;
 if (!SaveIndicator->IsInViewport()) SaveIndicator->AddToViewport(100);
 SaveIndicator->Begin();
}

void UVTGSaveCoordinator::FinishSaveIndicator()
{
 if (SaveIndicator) SaveIndicator->Finish();
}

void UVTGSaveCoordinator::BeginNewGame()
{
	// Everything that outlives a map change and describes a run in progress. The coordinator is a
	// GameInstance subsystem, so none of it goes away by itself - and nothing used to clear it, which
	// meant a new game inherited the previous run's PersistentInts. BPLM_L2StreetBarFight keeps its
	// level phase there, so a fresh run walking into the bar street read "fight already done".
	ClearPersistentFlags();
	ResumedCheckpoint = NAME_None;
	RespawnOverride.Reset();
	PendingLoad = nullptr;
	PendingLoadSlot = INDEX_NONE;
	bRetrying = false;
	CancelPendingRestore();
	DestroyedActorIds.Reset();
	if (UVTGGameInstanceBase* GI = Cast<UVTGGameInstanceBase>(GetGameInstance()))
	{
		GI->SetCurrentStage(NAME_None);
	}
	UE_LOG(LogTemp, Display, TEXT("Save coordinator reset for a new game"));
}

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

	const TMap<FName, FName> PreviousNames = PersistentNames;
	const TMap<FName, int32> PreviousInts = PersistentInts;
	SetPersistentName(CheckpointIdKey, CheckpointId);
	SetPersistentName(CheckpointLevelKey, Level);
	SetPersistentInt(CheckpointOrderKey, Order);

	RespawnOverride = RespawnTransform;
	const bool bOk = AutoSave();
	RespawnOverride.Reset();
	if (!bOk)
	{
		PersistentNames = PreviousNames;
		PersistentInts = PreviousInts;
	}
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





void UVTGSaveCoordinator::RestoreLevelManager(AVTGLevelManagerBase* Manager) const
{
 if (!Manager || Manager->HasRestoredLevelProgress() || !PendingLoad || Manager->GetGameInstance() != GetGameInstance()) return;
 if (UGameplayStatics::GetCurrentLevelName(Manager, true) != PendingLoad->Meta.LevelName) return;
 if (const auto* Record = PendingLoad->LevelProgress.Find(Manager->GetProgressKey()))
  Manager->RestoreLevelProgress(*Record);
}

int32 UVTGSaveCoordinator::GetLatestValidSlot() const
{
 int32 Best=INDEX_NONE;
 FDateTime Latest=FDateTime::MinValue();
 for (int32 Slot=0; Slot<MaxSlots; ++Slot)
 {
  if (!DoesSlotExist(Slot)) continue;
  const auto* Data=Cast<UVTGSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName(Slot),0));
  if (!Data || !Data->Meta.bIsValid || Data->Meta.LevelName.IsEmpty() || Data->Meta.SaveVersion>VTG_SAVE_VERSION) continue;
  // Cooked IoStore packages cannot be discovered by scanning loose disk files.
  TArray<FAssetData> Maps;
  FAssetRegistryModule::GetRegistry().GetAssetsByClass(UWorld::StaticClass()->GetClassPathName(),Maps);
  const bool MapExists=Maps.ContainsByPredicate([Data](const FAssetData& Asset)
  {
   return Asset.PackageName.ToString()==Data->Meta.LevelName || Asset.AssetName.ToString()==Data->Meta.LevelName;
  });
  if (!MapExists) continue;
  if (Best==INDEX_NONE || Data->Meta.SaveTimeUtc>Latest)
  {Best=Slot;Latest=Data->Meta.SaveTimeUtc;}
 }
 return Best;
}

bool UVTGSaveCoordinator::ContinueLatestSave()
{
 if (PendingLoad) return false;
 const int32 Slot=GetLatestValidSlot();
 return Slot!=INDEX_NONE && LoadFromSlot(Slot);
}
