// Explicit scene rebuild entries layered over the existing value-only BPLM snapshots.
namespace StreetSewerCheckpoints
{
using namespace MarketFollowup;
const FString Output=TEXT("Scripts/EncounterCheckpoints/");
const TCHAR* L2Path=TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight");
const TCHAR* SewerPath=TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt");
void Regression(UBlueprint* L2,UBlueprint* Sewer);
void Persist(UBlueprint* BP)
{
 const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 const FString Backup=FPaths::ProjectDir()/Output/TEXT("Backup")/(BP->GetName()+TEXT(".uasset"));
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
 if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
 auto* P=BP->GetOutermost();P->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
 check(UPackage::SavePackage(P,BP,*File,Args));
}
UK2Node_CallFunction* ComponentCall(UEdGraph* G,UClass* C,FName Fn,FName Component,int X,int Y)
{
 auto* N=Call(G,C->FindFunctionByName(Fn),X,Y);auto* Ref=Get(G,Component,X,Y+180);Link(Ref,*Component.ToString(),N,TEXT("self"));return N;
}
void PatchL2(UBlueprint* BP)
{
 if(BP->GeneratedClass->FindFunctionByName(TEXT("VTG_ResumeL2StreetFight")))return;
 auto* G=Graph(BP);
 auto* Fight=Event(G,TEXT("VTG_ResumeL2StreetFight"),12500,10000);
 auto* AfterEvent=Event(G,TEXT("VTG_ResumeAfterL2StreetFight"),12500,10600);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);
 auto Entry=[&](UEdGraphNode* E,int Y)
 {
  // Resolve the authored function by its existing call, since Blueprint display names contain spaces.
  auto* Light=Call(G,CastChecked<UK2Node_CallFunction>(Node(G,TEXT("K2Node_CallFunction_109")))->GetTargetFunction(),12800,Y);
  Link(E,TEXT("then"),Light,TEXT("execute"));
  auto* Init=Invoke(G,BP,TEXT("L2_InitializeNPC"),13100,Y);Link(Light,TEXT("then"),Init,TEXT("execute"));return Init;
 };
 // Join the existing combat setup AFTER its one-time opening bark. No intro sequences are played.
 auto* Init=Entry(Fight,10000);Link(Init,TEXT("then"),Node(G,TEXT("K2Node_CallFunction_27")),TEXT("execute"));
 auto* AfterInit=Entry(AfterEvent,10600);
 auto* Resume=ComponentCall(G,UVTGL2FightStateComponent::StaticClass(),TEXT("ResumeAfterFight"),TEXT("L2FightState"),13400,10600);
 auto* Client=Get(G,TEXT("L2_FemaleClient"),13400,10880);Link(Client,TEXT("L2_FemaleClient"),Resume,TEXT("FemaleClient"));Link(AfterInit,TEXT("then"),Resume,TEXT("execute"));
 auto* Old=Node(G,TEXT("K2Node_CallFunction_91"));auto Inputs=Old->FindPinChecked(TEXT("execute"))->LinkedTo;auto Next=Old->FindPinChecked(TEXT("then"))->LinkedTo;
 auto* Save=Call(G,AVTGLevelManagerBase::StaticClass()->FindFunctionByName(TEXT("SaveEncounterCheckpoint")),12500,11400);Save->NodeComment=TEXT("L2 fight boundary: save BPLM progress before enemy activation");
 Save->FindPinChecked(TEXT("CheckpointId"))->DefaultValue=TEXT("L2StreetFightStart");Save->FindPinChecked(TEXT("ResumeEvent"))->DefaultValue=TEXT("VTG_ResumeL2StreetFight");Save->FindPinChecked(TEXT("Order"))->DefaultValue=TEXT("100");
 auto* Player=Get(G,TEXT("Player Pawn"),11900,11600);auto* Transform=Call(G,AActor::StaticClass()->FindFunctionByName(TEXT("GetTransform")),12200,11600);Link(Player,TEXT("Player Pawn"),Transform,TEXT("self"));Link(Transform,TEXT("ReturnValue"),Save,TEXT("RespawnTransform"));
 Old->BreakAllNodeLinks();for(auto* P:Inputs)Link(P,Save->FindPinChecked(TEXT("execute")));for(auto* P:Next)Link(Save->FindPinChecked(TEXT("then")),P);
 FBlueprintEditorUtils::RemoveNode(BP,Old,true);
 auto* Entered=Set(G,TEXT("L2_Substage_HadEnteredFight?"),TEXT("true"),11900,11400);Before(Save->FindPinChecked(TEXT("execute")),Entered);
 auto* SequenceDone=Set(G,TEXT("L2_EnmSeqPlaying"),TEXT("false"),12200,11400);After(Entered->FindPinChecked(TEXT("then")),SequenceDone);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);
}
void PatchSewer(UBlueprint* BP)
{
 if(BP->GeneratedClass->FindFunctionByName(TEXT("VTG_ResumeSewerBossFight")))return;
 FEdGraphPinType BoolType;BoolType.PinCategory=UEdGraphSchema_K2::PC_Boolean;
 for(FName Name:{FName(TEXT("L3_CheckpointDoorOpened")),FName(TEXT("L3_CheckpointPassedDoor"))})check(FBlueprintEditorUtils::AddMemberVariable(BP,Name,BoolType,TEXT("false")));
 auto* G=Graph(BP);auto* Fight=Event(G,TEXT("VTG_ResumeSewerBossFight"),5000,14000);auto* AfterEvent=Event(G,TEXT("VTG_ResumeAfterSewerBossFight"),5000,14500);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);
 for(auto* E:{Fight,AfterEvent})
 {
  auto* Restore=ComponentCall(G,UVTGSewerEncounterComponent::StaticClass(),TEXT("RestoreEncounterCheckpointScene"),TEXT("SewerEncounter"),5300,E->NodePosY);Link(E,TEXT("then"),Restore,TEXT("execute"));
 }
 auto* Opened=Set(G,TEXT("L3_CheckpointDoorOpened"),TEXT("true"),5000,15100);Before(Node(G,TEXT("K2Node_CallFunction_22"))->FindPinChecked(TEXT("execute")),Opened);
 auto* Passed=Set(G,TEXT("L3_CheckpointPassedDoor"),TEXT("true"),5000,15500);After(Node(G,TEXT("K2Node_CustomEvent_8"))->FindPinChecked(TEXT("then")),Passed);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);
}
void PatchL2Arena(UBlueprint* BP)
{
 auto* G=Graph(BP);for(UEdGraphNode* N:G->Nodes)if(N->GetName()==TEXT("VTG_CheckpointActivateArena"))return;
 UK2Node_CallFunction* Init=nullptr;
 for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N);C && C->NodePosX==13100 && C->NodePosY==10000)Init=C;
 check(Init);
 auto* Activate=Call(G,CastChecked<UK2Node_CallFunction>(Node(G,TEXT("K2Node_CallFunction_71")))->GetTargetFunction(),13400,10000);Activate->Rename(TEXT("VTG_CheckpointActivateArena"),G);
 auto* Arena=Get(G,TEXT("FightAreaCollision"),13400,10200);Link(Arena,TEXT("FightAreaCollision"),Activate,TEXT("self"));After(Init->FindPinChecked(TEXT("then")),Activate);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Compile(BP);
}
int32 Run(bool VerifyOnly)
{
 auto* L2=LoadObject<UBlueprint>(nullptr,L2Path);auto* Sewer=LoadObject<UBlueprint>(nullptr,SewerPath);check(L2 && Sewer);
 if(!VerifyOnly){PatchL2(L2);PatchL2Arena(L2);PatchSewer(Sewer);}
 Compile(L2);Compile(Sewer);
 for(auto* BP:{L2,Sewer})check(BP->GeneratedClass->IsChildOf(AVTGLevelManagerBase::StaticClass()));
 check(L2->GeneratedClass->FindFunctionByName(TEXT("VTG_ResumeL2StreetFight")) && L2->GeneratedClass->FindFunctionByName(TEXT("VTG_ResumeAfterL2StreetFight")));
 check(Sewer->GeneratedClass->FindFunctionByName(TEXT("VTG_ResumeSewerBossFight")) && Sewer->GeneratedClass->FindFunctionByName(TEXT("VTG_ResumeAfterSewerBossFight")));
 Regression(L2,Sewer);
 if(!VerifyOnly){Persist(L2);Persist(Sewer);}
 UE_LOG(LogTemp,Display,TEXT("STREET_SEWER_CHECKPOINT_PATCH_SUCCESS"));return 0;
}
}
