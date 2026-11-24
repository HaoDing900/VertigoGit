#include "Save/VTGSaveCoordinator.h"
#include "Save/VTGSaveTypes.h"
#include "VTGGameInstanceBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

/**
 * Story autosave guards. Deliberately disk-free: the write itself is deferred to a ticker that this
 * test never pumps, so nothing touches the player's real slot files. What is asserted here is the
 * part that decides WHETHER to write - which is the part that can quietly ruin a playthrough by
 * overwriting a combat checkpoint or a manual save, or by never firing at all.
 */
struct FVTGStoryAutoSaveRegression
{
 /** The ticker is never pumped here, so stamp the beat by hand to simulate "that one was written". */
 static void MarkWritten(UVTGSaveCoordinator* Save,UWorld* World,FName Stage)
 {
  Save->SetPersistentName(UVTGSaveCoordinator::StoryAutoSaveStageKey,Save->StoryAutoSaveBeat(World,Stage));
 }
 /** Accepting only schedules a write; drop the pending ticker so the next case starts clean. */
 static void Accepted(UVTGSaveCoordinator* Save)
 {
  check(Save->StoryAutoSaveTicker.IsValid());
  Save->CancelPendingRestore();
  check(!Save->StoryAutoSaveTicker.IsValid());
 }

 static int32 Run()
 {
  // Every run of this commandlet is itself a dev run (WITH_EDITOR), so story autosaves are suppressed
  // by default - that IS the gate, but it would make every acceptance case below fail for the wrong
  // reason. Opt in for the duration and restore it at the end; the gate gets its own block.
  IConsoleVariable* DevRuns=IConsoleManager::Get().FindConsoleVariable(TEXT("VTG.AutoSaveInDevRuns"));
  check(DevRuns && DevRuns->GetInt()==0); // the shipped default, asserted rather than assumed
  DevRuns->Set(1);

  const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
  auto* MapA=UWorld::CreateWorld(EWorldType::Game,false,TEXT("StoryAutoSaveMapA"),nullptr,true,ERHIFeatureLevel::Num,&Init);
  auto* MapB=UWorld::CreateWorld(EWorldType::Game,false,TEXT("StoryAutoSaveMapB"),nullptr,true,ERHIFeatureLevel::Num,&Init);
  check(MapA && MapB);
  auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(MapA);
  auto* GI=NewObject<UVTGGameInstanceBase>();MapA->SetGameInstance(GI);MapB->SetGameInstance(GI);Context.OwningGameInstance=GI;
  auto* Save=NewObject<UVTGSaveCoordinator>(GI);
  // GI->GetWorld() needs a world context this harness does not build; the level manager passes its
  // own world in production, and so does this test.

  // ---- The reserved slots ---------------------------------------------------------------------
  const int32 Story=Save->GetStoryAutoSaveSlot();
  check(Save->GetAutoSaveSlot()==0 && Story!=0);
  // Two rotating story slots, so one bad write never destroys the last good beat.
  check(Save->IsStoryAutoSaveSlot(Story) && Save->IsStoryAutoSaveSlot(Story+1));
  check(!Save->IsStoryAutoSaveSlot(Story-1) && !Save->IsStoryAutoSaveSlot(Story+2));
  check(!Save->IsManualSlot(0) && !Save->IsManualSlot(-1));
  check(!Save->IsManualSlot(Story) && !Save->IsManualSlot(Story+1));
  for(int32 Slot=1;Slot<Story;++Slot)check(Save->IsManualSlot(Slot));
  FText Result;
  check(!Save->SaveTerminalSlot(0,Result));        // combat checkpoint is the game's, not the player's
  check(!Save->SaveTerminalSlot(Story,Result));    // so are both story autosave slots
  check(!Save->SaveTerminalSlot(Story+1,Result));
  TArray<FVTGSlotMeta> Metas;Save->GetAllSlotMetas(Metas);
  for(int32 Slot : {Story,Story+1})
   check(Metas.ContainsByPredicate([Slot](const FVTGSlotMeta& M){return M.SlotIndex==Slot;})); // loadable

  // ---- The save gate --------------------------------------------------------------------------
  // No world and no pawn is the one case this harness can exercise; combat, cutscenes and dialogue
  // are asserted in the terminal regression, which has a real pawn and a real player controller.
  FText Reason;
  check(!Save->CanSaveNow(nullptr,Reason) && !Reason.IsEmpty());
  check(!Save->IsPlayerInCombat(nullptr));

  // ---- A beat is map + stage ------------------------------------------------------------------
  check(Save->StoryAutoSaveBeat(nullptr,TEXT("Stage_A")).IsNone());
  const FName BeatA=Save->StoryAutoSaveBeat(MapA,TEXT("Stage_A"));
  check(!BeatA.IsNone());
  check(Save->StoryAutoSaveBeat(MapA,TEXT("Stage_B"))!=BeatA);   // stage change is a new beat
  Context.SetCurrentWorld(MapB);
  check(Save->StoryAutoSaveBeat(MapB,TEXT("Stage_A"))!=BeatA);   // map change is a new beat too
  check(!Save->StoryAutoSaveBeat(MapB,NAME_None).IsNone());      // a map with no stage still counts
  Context.SetCurrentWorld(MapA);

  // ---- First arrival saves; the same beat again does not --------------------------------------
  check(Save->StoryAutoSave(Context.World(),TEXT("Stage_A")));Accepted(Save);
  MarkWritten(Save,MapA,TEXT("Stage_A"));
  check(!Save->StoryAutoSave(Context.World(),TEXT("Stage_A")));  // second BPLM in this map, or walked back in

  // ---- Stage advance in the same map saves ----------------------------------------------------
  check(Save->StoryAutoSave(Context.World(),TEXT("Stage_B")));Accepted(Save);
  MarkWritten(Save,MapA,TEXT("Stage_B"));
  check(!Save->StoryAutoSave(Context.World(),TEXT("Stage_B")));

  // ---- Walking into another map saves, even with the stage unchanged --------------------------
  Context.SetCurrentWorld(MapB);
  check(Save->StoryAutoSave(Context.World(),TEXT("Stage_B")));Accepted(Save);
  MarkWritten(Save,MapB,TEXT("Stage_B"));
  check(!Save->StoryAutoSave(Context.World(),TEXT("Stage_B")));
  // ...and a stage-less map is a beat of its own, not a reason to skip.
  check(Save->StoryAutoSave(Context.World(),NAME_None));Accepted(Save);
  MarkWritten(Save,MapB,NAME_None);
  check(!Save->StoryAutoSave(Context.World(),NAME_None));

  // ---- Walking BACK into a map DOES re-save: the autosave is "where you are now" ---------------
  // Only one beat is remembered, the last one written - not a history of visits. So A -> B -> A
  // autosaves three times, each overwriting the last, which is what a safety net should do.
  Context.SetCurrentWorld(MapA);
  check(Save->StoryAutoSave(Context.World(),TEXT("Stage_B")));Accepted(Save);
  MarkWritten(Save,MapA,TEXT("Stage_B"));
  check(!Save->StoryAutoSave(Context.World(),TEXT("Stage_B")));  // but not twice for the same arrival

  // ---- Refusals -------------------------------------------------------------------------------
  Context.SetCurrentWorld(MapB);
  Save->PendingLoad=NewObject<UVTGSaveGame>();
  check(!Save->StoryAutoSave(Context.World(),TEXT("Stage_Z")));  // a load in flight must not be overwritten
  Save->PendingLoad=nullptr;
  Save->bRetrying=true;
  check(!Save->StoryAutoSave(Context.World(),TEXT("Stage_Z")));  // nor a combat retry
  Save->bRetrying=false;
  check(Save->StoryAutoSave(Context.World(),TEXT("Stage_Z")));   // and it recovers afterwards
  check(!Save->StoryAutoSave(Context.World(),TEXT("Stage_Y")));  // one queued write at a time
  Accepted(Save);
  Context.SetCurrentWorld(nullptr);
  check(!Save->StoryAutoSave(Context.World(),TEXT("Stage_Z")));  // no world, no beat

  // ---- The editor opt-out (VTG.AutoSaveInDevRuns) ----------------------------------------------
  // Tuning level sequences means long Simulate sessions, and without this every BPLM BeginPlay in the
  // editor wrote into the same Saved/SaveGames files the game reads. Two things are asserted here.
  // First that the gate is keyed on the BUILD, not the world type: a world of type Game is suppressed
  // too, which is what covers Standalone - a separate UnrealEditor.exe -game process, and the one mode
  // that must be used to test anything the PIE viewport cannot show. Second, and more important, that
  // suppression is STATELESS: no stamp, no queued write, so turning it back on is not a transition.
  Context.SetCurrentWorld(MapB);
  const FName StampBefore=Save->GetPersistentName(UVTGSaveCoordinator::StoryAutoSaveStageKey);
  check(!Save->IsStoryAutoSaveSuppressed(MapB));      // opted in at the top of this run
  DevRuns->Set(0);
  check(MapB->WorldType==EWorldType::Game);
  check(Save->IsStoryAutoSaveSuppressed(MapB));       // Standalone's world type, and still suppressed
  MapB->WorldType=EWorldType::PIE;
  check(Save->IsStoryAutoSaveSuppressed(MapB));       // Play and Simulate share this one
  MapB->WorldType=EWorldType::Game;
  check(!Save->IsStoryAutoSaveSuppressed(nullptr));   // no world is "no beat", not "suppressed"
  check(!Save->StoryAutoSave(MapB,TEXT("Stage_Z")));  // an unwritten beat still does not write
  check(Save->GetPersistentName(UVTGSaveCoordinator::StoryAutoSaveStageKey)==StampBefore); // no stamp
  check(!Save->StoryAutoSaveTicker.IsValid());                                             // no write
  // Turning it on is enough by itself: that beat was never stamped, so it is still owed.
  DevRuns->Set(1);
  check(!Save->IsStoryAutoSaveSuppressed(MapB));
  check(Save->StoryAutoSave(MapB,TEXT("Stage_Z")));Accepted(Save);
  check(Save->GetPersistentName(UVTGSaveCoordinator::StoryAutoSaveStageKey)==StampBefore);
  // The gate is narrow on purpose: it never touches the slot 0 combat checkpoint, which is what makes
  // retry-after-death work and is exactly what gets tested from the editor.
  check(Save->GetAutoSaveSlot()==0 && !Save->IsStoryAutoSaveSlot(Save->GetAutoSaveSlot()));

  // ---- New Game forgets the previous run ------------------------------------------------------
  // Nothing used to clear this, and the coordinator outlives a map change - so a fresh run inherited
  // the last one's PersistentInts, and BPLM_L2StreetBarFight read "fight already done".
  Context.SetCurrentWorld(MapA); // the previous case left the context without a world
  Save->SetPersistentInt(TEXT("LevelPhaseForL2StreetFight"),3);
  MarkWritten(Save,MapA,TEXT("Stage_A"));
  check(Save->GetPersistentInt(TEXT("LevelPhaseForL2StreetFight"))==3);
  Save->BeginNewGame();
  check(Save->GetPersistentInt(TEXT("LevelPhaseForL2StreetFight"),-1)==-1);
  check(Save->GetPersistentName(UVTGSaveCoordinator::StoryAutoSaveStageKey).IsNone());
  check(Save->ProgressByMap.Num()==0);
  check(!Save->StoryAutoSaveTicker.IsValid());
  // ...so the first map of the new run autosaves again.
  check(Save->StoryAutoSave(Context.World(),TEXT("Stage_A")));Accepted(Save);

  // ---- A new game forgets the stamps, so the first map of the next run saves again ------------
  Context.SetCurrentWorld(MapA);
  Save->ClearPersistentFlags();
  check(Save->GetPersistentName(UVTGSaveCoordinator::StoryAutoSaveStageKey).IsNone());
  check(Save->StoryAutoSave(Context.World(),TEXT("Stage_A")));Accepted(Save);

  DevRuns->Set(0); // back to the shipped default, so nothing after this run inherits the opt-in
  MapA->DestroyWorld(false);MapB->DestroyWorld(false);GEngine->DestroyWorldContext(MapA);
  UE_LOG(LogTemp,Display,TEXT("STORY AUTOSAVE PASS: beat = map + stage; fires on first arrival, on a stage advance, on walking into another map and on walking back; refuses a repeated beat, mid-load, mid-retry, a second queued write and no world; stage-less maps count; reserved slots 0 and %d are loadable but not player-writable; new game clears the stamp, the persistent ints and the per-map progress; VTG.AutoSaveInDevRuns=0 suppresses every run launched from the editor - Play, Simulate and Standalone, keyed on the build rather than the world type - statelessly, leaving no stamp and no queued write, and flipping it to 1 is enough on its own. No user save files touched."),Story);
  return 0;
 }
};
