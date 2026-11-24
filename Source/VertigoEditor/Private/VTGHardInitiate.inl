#include "VTGHardInitiateProbe.h"
#include "VTGGameInstanceBase.h"
#include "Save/VTGSaveTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/EngineBaseTypes.h"

/**
 * The hard initiate contract: called exactly once per level entry, on every way in, including the
 * checkpoint resume that skips OnStageBegin entirely.
 *
 * Disk-free and subsystem-free. GI->GetSubsystem returns null in a harness world, which is on
 * purpose here: it exercises the path where no save coordinator answers, and that path must still
 * set the level up rather than quietly lose it. The OnSlotLoaded ordering against a real restore
 * needs the live coordinator and is not covered - see the note in the PASS line.
 */
struct FVTGHardInitiateRegression
{
 static AVTGHardInitiateProbe* Place(UWorld* World,FName Event)
 {
  auto* Probe=World->SpawnActor<AVTGHardInitiateProbe>();
  check(Probe);
  Probe->HardInitiateEvent=Event;
  return Probe;
 }
 /** One tick is all a next-tick timer needs; more of them prove the one-shot guard. */
 static void Pump(UWorld* World,int32 Times=1)
 {
  // FTimerManager::Tick no-ops when it has already run this frame, and a commandlet never advances
  // the frame counter on its own - without this every pump after the first would do nothing and the
  // "fires only once" assertions below would pass without testing anything.
  for(int32 I=0;I<Times;++I){++GFrameCounter;World->GetTimerManager().Tick(.016f);}
 }

 static int32 Run()
 {
  const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
  auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("HardInitiateWorld"),nullptr,true,ERHIFeatureLevel::Num,&Init);
  check(World);
  auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(World);
  auto* GI=NewObject<UVTGGameInstanceBase>();World->SetGameInstance(GI);Context.OwningGameInstance=GI;
  // AActor::ProcessEvent refuses to run script until the world says its actors are initialised, so
  // without this every hard initiate would report "called" and do nothing - exactly the silent
  // failure under test.
  World->InitializeActorsForPlay(FURL());

  // ---- A normal arrival ------------------------------------------------------------------------
  {
   auto* Probe=Place(World,TEXT("ProbeInitiate"));
   Probe->DispatchBeginPlay();
   check(Probe->Initiates==0);        // not during BeginPlay: a restore would still be in flight
   Pump(World);
   check(Probe->Initiates==1);
   check(Probe->SawFromLoad.Num()==1 && Probe->SawFromLoad[0]==false);
   check(!Probe->DidEnterFromLoad());
   // ---- and only once, however long the level runs ---------------------------------------------
   Pump(World,8);
   check(Probe->Initiates==1);
  }

  // ---- A checkpoint resume: OnStageBegin is skipped, setup must happen anyway -------------------
  {
   auto* Probe=Place(World,TEXT("ProbeInitiate"));
   FVTGLevelProgressRecord Record;
   Record.EncounterResumeEvent=TEXT("ProbeResume");   // this is what suppresses OnStageBegin
   Probe->RestoreLevelProgress(Record);
   check(Probe->HasRestoredLevelProgress());
   Probe->DispatchBeginPlay();
   Pump(World,4);
   check(Probe->Initiates==1);                        // the whole point of the mechanism
   check(Probe->SawFromLoad.Num()==1 && Probe->SawFromLoad[0]==true);
   check(Probe->DidEnterFromLoad());
  }

  // ---- Opting out: a BPLM that never made the event is untouched --------------------------------
  {
   auto* Probe=Place(World,NAME_None);
   Probe->DispatchBeginPlay();
   Pump(World,4);
   check(Probe->Initiates==0 && Probe->Resumes==0);
   check(!Probe->RunHardInitiate());
  }

  // ---- A name that resolves to nothing is loud, not silent --------------------------------------
  // Reflection by name fails quietly, and quietly losing level setup is the bug this whole mechanism
  // exists to prevent - so a typo or a renamed event must refuse rather than pretend.
  // Both cases below are MEANT to log an Error, so mute the category across them: otherwise this
  // regression always exits non-zero and the exit code stops meaning anything.
  const ELogVerbosity::Type WasVerbose=LogTemp.GetVerbosity();
  LogTemp.SetVerbosity(ELogVerbosity::Fatal);
  {
   auto* Probe=Place(World,TEXT("ProbeInitiateTypo"));
   Probe->DispatchBeginPlay();
   Pump(World,4);
   check(Probe->Initiates==0);
   check(!Probe->RunHardInitiate());
  }
  {
   auto* Probe=Place(World,TEXT("ProbeWithParameter"));
   Probe->DispatchBeginPlay();
   Pump(World,4);
   check(Probe->BadCalls==0);                         // wrong shape is refused, not called blind
   check(!Probe->RunHardInitiate());
  }
  LogTemp.SetVerbosity(WasVerbose);

  // ---- Driving it by hand is allowed, and is the only way to run it twice ------------------------
  {
   auto* Probe=Place(World,TEXT("ProbeInitiate"));
   Probe->DispatchBeginPlay();
   Pump(World);
   check(Probe->Initiates==1);
   check(Probe->RunHardInitiate());
   check(Probe->Initiates==2);                        // explicit, so the one-shot guard does not apply
   Pump(World,4);
   check(Probe->Initiates==2);                        // and the base class still does not fire again
  }

  // ---- Sub-stages -------------------------------------------------------------------------------
  // The sub-stage is the "rebuild from here" marker. Two things have to hold or it is worthless:
  // it must survive the save, and it must already be in place when the hard initiate reads it.
  {
   auto* Probe=Place(World,TEXT("ProbeInitiate"));
   check(Probe->GetSubStage().IsNone());              // nothing authored = the stage's normal setup

   // Recording and saving are separate: there is no coordinator in this world, so no autosave can be
   // scheduled - and the sub-stage must be recorded all the same, or a checkpoint taken while saving
   // is unavailable would rebuild from the wrong place.
   check(!Probe->StoryCheckpoint(TEXT("L5.Corridor")));
   check(Probe->GetSubStage()==TEXT("L5.Corridor"));

   // An empty pin is a mistake, and must not quietly wipe the marker that is already there.
   const ELogVerbosity::Type Was=LogTemp.GetVerbosity();LogTemp.SetVerbosity(ELogVerbosity::Fatal);
   check(!Probe->StoryCheckpoint(NAME_None));
   LogTemp.SetVerbosity(Was);
   check(Probe->GetSubStage()==TEXT("L5.Corridor"));

   FVTGLevelProgressRecord Record;
   Probe->CaptureLevelProgress(Record);
   check(Record.SubStage==TEXT("L5.Corridor"));

   Probe->ClearSubStage();
   check(Probe->GetSubStage().IsNone());

   // ---- and it is there before the hard initiate looks ------------------------------------------
   auto* Loaded=Place(World,TEXT("ProbeInitiate"));
   Loaded->RestoreLevelProgress(Record);
   check(Loaded->GetSubStage()==TEXT("L5.Corridor"));
   Loaded->DispatchBeginPlay();
   Pump(World,4);
   check(Loaded->Initiates==1);
   check(Loaded->SawSubStage.Num()==1 && Loaded->SawSubStage[0]==TEXT("L5.Corridor"));
  }
  {
   // A save written before sub-stages existed has no field at all; that reads as None, which is the
   // stage's normal setup - so old saves keep behaving exactly as they did.
   auto* Probe=Place(World,TEXT("ProbeInitiate"));
   Probe->RestoreLevelProgress(FVTGLevelProgressRecord());
   check(Probe->GetSubStage().IsNone());
   Probe->DispatchBeginPlay();
   Pump(World,4);
   check(Probe->SawSubStage.Num()==1 && Probe->SawSubStage[0].IsNone());
  }

  World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
  UE_LOG(LogTemp,Display,TEXT("HARD INITIATE PASS: fires exactly once per level entry and not during BeginPlay; fires on a checkpoint resume that skips OnStageBegin, reporting DidEnterFromLoad; an unset name is a clean opt-out; a missing or parameterised name is refused rather than called; RunHardInitiate drives it by hand without disarming the one-shot; a sub-stage is recorded even when no autosave can be scheduled, rides the progress record, is readable by the time the hard initiate runs, and reads as None out of a save written before it existed. Not covered here: ordering against a live save restore's OnSlotLoaded, which needs the real coordinator."));
  return 0;
 }
};
