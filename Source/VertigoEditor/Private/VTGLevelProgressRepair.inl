#include "VTGLevelManagerBase.h"

struct FVTGLevelProgressRegression
{
 static void VerifyCoordinator(UWorld* World, AVTGLevelManagerBase* Manager, const FVTGLevelProgressRecord& Record)
 {
  auto* GI=NewObject<UVTGGameInstanceBase>();World->SetGameInstance(GI);
  auto* Save=NewObject<UVTGSaveCoordinator>(GI);
  Save->PendingLoad=NewObject<UVTGSaveGame>();
  Save->PendingLoad->Meta.LevelName=TEXT("DifferentMap");
  Save->PendingLoad->LevelProgress.Add(Manager->GetFName(),Record);
  Save->RestoreLevelManager(Manager);check(!Manager->HasRestoredLevelProgress());
  Save->PendingLoad->Meta.LevelName=UGameplayStatics::GetCurrentLevelName(World,true);
  Save->RestoreLevelManager(Manager);check(Manager->HasRestoredLevelProgress());
  check(!Manager->EnterSavedStoryEvent(TEXT("BlackMarket.Intro")));
  check(Manager->EnterSavedStoryEvent(TEXT("LaterEvent")));
  Save->RestoreLevelManager(Manager);
  FVTGLevelProgressRecord Again;Manager->CaptureLevelProgress(Again);
  check(Again.StartedEvents.Contains(TEXT("LaterEvent")));
  Save->PendingLoad=nullptr;
  UE_LOG(LogTemp,Display,TEXT("EARLY RESTORE PASS: map/actor lookup before BeginPlay; repeated restore does not erase new progress."));
 }
};

int32 RepairLevelProgress(bool VerifyOnly)
{
 using namespace AlleyFix;
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/L4BlackMarketEntrance/BPLM_BlackMarketEntrance"));
 check(BP && BP->GeneratedClass->IsChildOf(AVTGLevelManagerBase::StaticClass()));
 UEdGraph* G=BP->UbergraphPages[0];
 const TCHAR* Events[]={TEXT("K2Node_CustomEvent_0"),TEXT("K2Node_CustomEvent_2"),TEXT("K2Node_CustomEvent_3"),TEXT("K2Node_CustomEvent_4")};
 const TCHAR* Ids[]={TEXT("BlackMarket.Intro"),TEXT("BlackMarket.MailNotice"),TEXT("BlackMarket.DistantArgue"),TEXT("BlackMarket.MeetSequence")};
 if(!VerifyOnly)
 {
  const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
  const FString Backup=FPaths::ProjectSavedDir()/TEXT("QA/LevelProgressBefore/BPLM_BlackMarketEntrance.uasset");
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
  if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
 }
 for(int32 I=0;I<4;++I)
 {
  auto* Entry=Node(G,Events[I]);auto* Out=Pin(Entry,TEXT("then"));
  const FString Mark=FString(TEXT("VTG saved story: "))+Ids[I];
  if(Out->LinkedTo.Num()==1 && Out->LinkedTo[0]->GetOwningNode()->NodeComment==Mark) continue;
  check(!VerifyOnly && Out->LinkedTo.Num()==1);
  auto* Next=Out->LinkedTo[0];
  auto* Gate=Call(G,AVTGLevelManagerBase::StaticClass()->FindFunctionByName(TEXT("EnterSavedStoryEvent")),Entry->NodePosX+240,Entry->NodePosY-160);
  Gate->NodeComment=Mark;Pin(Gate,TEXT("EventId"))->DefaultValue=Ids[I];
  auto* Branch=Add<UK2Node_IfThenElse>(G,Entry->NodePosX+500,Entry->NodePosY-160);Branch->AllocateDefaultPins();Branch->NodeComment=Mark;
  Out->BreakAllPinLinks();Link(Out,Pin(Gate,TEXT("execute")));
  Link(Pin(Gate,TEXT("then")),Pin(Branch,TEXT("execute")));
  Link(Pin(Gate,TEXT("ReturnValue")),Pin(Branch,TEXT("Condition")));
  Link(Pin(Branch,TEXT("then")),Next);
 }
 if(!VerifyOnly && !Save(BP))return 1;
 FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);if(Log.NumErrors)return 2;
 // Real BPLM class, fresh actor instances and real SaveGame serialization. No user's disk slots.
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("LevelProgressTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);
 auto* First=World->SpawnActor<AVTGLevelManagerBase>(BP->GeneratedClass);
 auto* Fresh=World->SpawnActor<AVTGLevelManagerBase>(BP->GeneratedClass);
 check(First && Fresh);
 auto* Read=FindFProperty<FBoolProperty>(First->GetClass(),TEXT("L4_HasReadMissionBriefMail?"));check(Read);
 Read->SetPropertyValue_InContainer(First,true);
 check(First->EnterSavedStoryEvent(TEXT("BlackMarket.Intro")));
 auto* Data=NewObject<UVTGSaveGame>();First->CaptureLevelProgress(Data->LevelProgress.FindOrAdd(TEXT("Manager")));
 TArray<uint8> Bytes;check(UGameplayStatics::SaveGameToMemory(Data,Bytes));
 auto* Loaded=CastChecked<UVTGSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
 const auto& Record=Loaded->LevelProgress.FindChecked(TEXT("Manager"));
 check(Record.Values.Contains(TEXT("L4_HasInitializedMail")) && !Record.Values.Contains(TEXT("Player Pawn")) && !Record.Values.Contains(TEXT("bBusy")));
 Fresh->RestoreLevelProgress(Record);
 check(Read->GetPropertyValue_InContainer(Fresh));
 check(!Fresh->EnterSavedStoryEvent(TEXT("BlackMarket.Intro")));
 check(Fresh->EnterSavedStoryEvent(TEXT("BlackMarket.MeetSequence")));
 auto* NewGame=World->SpawnActor<AVTGLevelManagerBase>(BP->GeneratedClass);
 check(NewGame->EnterSavedStoryEvent(TEXT("BlackMarket.Intro")));
 check(!Read->GetPropertyValue_InContainer(NewGame));
 FVTGLevelProgressRecord Again;Fresh->CaptureLevelProgress(Again);
 check(Again.StartedEvents.Contains(TEXT("BlackMarket.Intro")) && Again.StartedEvents.Contains(TEXT("BlackMarket.MeetSequence")));
 auto* PendingActor=World->SpawnActor<AVTGLevelManagerBase>(BP->GeneratedClass);
 FVTGLevelProgressRegression::VerifyCoordinator(World,PendingActor,Record);
 World->DestroyWorld(false);
 UE_LOG(LogTemp,Display,TEXT("LEVEL PROGRESS PASS: actual BPLM bool snapshot roundtrip; saved intro suppressed; future event and new-game intro allowed; runtime locks excluded; Blueprint compile clean."));
 return 0;
}
