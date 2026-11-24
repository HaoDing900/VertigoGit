#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "EdGraphSchema_K2.h"
#include "VTGGameInstanceBase.h"
#include "Save/VTGSaveCoordinator.h"
#include "Save/VTGSaveTypes.h"
#include "Engine/GameInstance.h"

/**
 * The GameInstance snapshot is what makes "which did Sa pick today, Leashed or Raw" survive a save.
 * This exercises it on the REAL BP_VTG_GameInstance class through REAL SaveGame serialization, so a
 * variable that stops round-tripping (renamed, retyped, turned transient) fails here and not in a
 * playtest three maps later.
 */
struct FVTGGameInstanceSnapshotRegression
{
 static UGameInstance* Spawn(UClass* Class) { return NewObject<UGameInstance>(GEngine, Class); }


 /**
  * The snapshot restores the GameInstance's "Leashed?" before the map opens; what then carries it to
  * the pawn is BPLM's own BeginPlay calling "Detect LeashedOrRaw?", which copies it into
  * BP_Player_Sa's "IsMedicated?" - the variable Cond_Sa_Leashed actually reads. That link is content,
  * not code, so assert it here: unwire it and requirement 4 silently stops working after a load.
  */
 static void CheckPawnSyncStillWired()
 {
  auto* BPLM=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/BPLM"));check(BPLM);
  TArray<UEdGraph*> Graphs;BPLM->GetAllGraphs(Graphs);
  bool bFound=false;
  for(UEdGraph* G : Graphs)
   for(UEdGraphNode* N : G->Nodes)
   {
    auto* Call=Cast<UK2Node_CallFunction>(N);
    if(!Call || Call->FunctionReference.GetMemberName()!=FName(TEXT("Detect LeashedOrRaw?"))) continue;
    // Walk the exec chain back; it has to originate at BeginPlay.
    UEdGraphNode* Up=N;
    for(int32 Hop=0;Hop<30 && Up;++Hop)
    {
     UEdGraphPin* In=nullptr;
     for(UEdGraphPin* P:Up->Pins)
      if(P->Direction==EGPD_Input && P->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec){In=P;break;}
     if(!In || In->LinkedTo.Num()==0) break;
     Up=In->LinkedTo[0]->GetOwningNode();
    }
    auto* Event=Cast<UK2Node_Event>(Up);
    if(Event && Event->EventReference.GetMemberName()==FName(TEXT("ReceiveBeginPlay"))) bFound=true;
   }
  check(bFound); // BPLM's BeginPlay no longer reaches "Detect LeashedOrRaw?"
 }

 static int32 Run()
 {
  auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/GameInstance/BP_VTG_GameInstance"));
  check(BP && BP->GeneratedClass && BP->GeneratedClass->IsChildOf(UVTGGameInstanceBase::StaticClass()));
  auto* Leashed=FindFProperty<FBoolProperty>(BP->GeneratedClass,TEXT("Leashed?"));
  check(Leashed); // the whole point of requirement 4; if this is gone the snapshot is pointless

  // Capture from a GameInstance that has made the choice.
  auto* Source=Spawn(BP->GeneratedClass);check(Source);
  Leashed->SetPropertyValue_InContainer(Source,true);
  auto* Data=NewObject<UVTGSaveGame>();
  NewObject<UVTGSaveCoordinator>(Source)->CaptureGameInstanceValues(Data);
  check(Data->GameInstanceValues.Contains(TEXT("Leashed?")));
  check(!Data->GameInstanceValues.Contains(TEXT("SaveData")));    // ISX owns its own save blob
  check(!Data->GameInstanceValues.Contains(TEXT("CurrentStage"))); // native, already in Meta.Stage

  // Through the real save file format, not just a struct copy.
  TArray<uint8> Bytes;check(UGameplayStatics::SaveGameToMemory(Data,Bytes));
  auto* Loaded=CastChecked<UVTGSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));

  // A fresh GameInstance starts at the asset default and ends up at the saved value.
  auto* Fresh=Spawn(BP->GeneratedClass);check(Fresh);
  check(!Leashed->GetPropertyValue_InContainer(Fresh));
  NewObject<UVTGSaveCoordinator>(Fresh)->RestoreGameInstanceValues(Loaded);
  check(Leashed->GetPropertyValue_InContainer(Fresh));

  // A variable whose type changed since the save was written is skipped, not misread.
  auto* Retyped=Spawn(BP->GeneratedClass);check(Retyped);
  Loaded->GameInstanceTypes.Add(TEXT("Leashed?"),TEXT("int32"));
  NewObject<UVTGSaveCoordinator>(Retyped)->RestoreGameInstanceValues(Loaded);
  check(!Leashed->GetPropertyValue_InContainer(Retyped));

  // An excluded name is not written AND not applied even if an older save carries it.
  auto* Excluded=Spawn(BP->GeneratedClass);check(Excluded);
  auto* Stale=NewObject<UVTGSaveGame>();
  Stale->GameInstanceValues.Add(TEXT("Leashed?"),TEXT("True"));
  Stale->GameInstanceTypes.Add(TEXT("Leashed?"),Leashed->GetCPPType(nullptr,0));
  auto* Co=NewObject<UVTGSaveCoordinator>(Excluded);
  Co->ExcludedGameInstanceVariables.Add(TEXT("Leashed?"));
  Co->RestoreGameInstanceValues(Stale);
  check(!Leashed->GetPropertyValue_InContainer(Excluded));

  CheckPawnSyncStillWired();
  UE_LOG(LogTemp,Display,TEXT("GAMEINSTANCE SNAPSHOT PASS: real BP_VTG_GameInstance roundtrip through SaveGame; Leashed? restored on a fresh instance; ISX SaveData and native CurrentStage excluded; retyped and excluded variables skipped; BPLM BeginPlay still syncs it to the pawn."));
  return 0;
 }
};
