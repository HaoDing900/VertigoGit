#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "NarrativeComponent.h"
#include "UI/VTGSlideshow.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace SoapExample
{
 template<class T> T* Add(UEdGraph* G,int X,int Y) {auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;return N;}
 UK2Node_CallFunction* Call(UEdGraph* G,UClass* C,const TCHAR* Name,int X,int Y)
 {auto* N=Add<UK2Node_CallFunction>(G,X,Y);N->SetFromFunction(C->FindFunctionByName(Name));N->AllocateDefaultPins();return N;}
 void Link(UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BP)
 {check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A->FindPinChecked(AP),B->FindPinChecked(BP)));}
 bool Compile(UBlueprint* BP)
 {FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Results);return Results.NumErrors==0;}
 bool Save(UObject* Object)
 {auto* P=Object->GetOutermost();FString Filename=FPackageName::LongPackageNameToFilename(P->GetName(),Object->IsA<UWorld>()?FPackageName::GetMapPackageExtension():FPackageName::GetAssetPackageExtension());FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;return UPackage::SavePackage(P,Object,*Filename,Args);}
 void Backup(const TCHAR* Relative)
 {FString Source=FPaths::ProjectDir()/Relative;FString Target=FPaths::ProjectSavedDir()/"SlideshowExampleBackup"/Relative;IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target),true);if(!IFileManager::Get().FileExists(*Target))check(IFileManager::Get().Copy(*Target,*Source)==COPY_OK);}
 UDialogueNode_NPC* Node(UDialogue* D,int Number)
 {const FName ID(*FString::Printf(TEXT("testSlideShow_DialogueNode_NPC_%d"),Number));for(auto* N:D->NPCReplies)if(N->GetID()==ID)return N;return nullptr;}
 UObject* GetObject(UObject* O,const TCHAR* Name)
 {auto* P=FindFProperty<FObjectPropertyBase>(O->GetClass(),Name);return P?P->GetObjectPropertyValue_InContainer(O):nullptr;}
 UEdGraphNode* GraphNode(UEdGraph* Graph,UDialogueNode* Node)
 {for(UEdGraphNode* G:Graph->Nodes)if(GetObject(G,TEXT("DialogueNode"))==Node)return G;return nullptr;}
}
int32 VTGSetupSoapTVExample(const FString& Params)
{
 using namespace SoapExample;
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/2DArt/SlideShow/testSlideShow.testSlideShow"));
 auto* SlideBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/2DArt/SlideShow/SlideShow_L4BlackMarket_SoapTV.SlideShow_L4BlackMarket_SoapTV"));
 auto* World=LoadObject<UWorld>(nullptr,TEXT("/Game/ThirdPerson/Maps/BaseLevel.BaseLevel"));
 if(!BP||!SlideBP||!World)return 10;
 auto* Template=Cast<UDialogue>(GetObject(BP,TEXT("DialogueTemplate")));
 auto* DialogueGraph=Cast<UEdGraph>(GetObject(BP,TEXT("DialogueGraph")));
 if(!Template||!DialogueGraph)return 11;
 AVTGSlideshowDirector* Director=nullptr;
 for(AActor* A:World->PersistentLevel->Actors)if(A && A->GetClass()==SlideBP->GeneratedClass){if(Director)return 12;Director=Cast<AVTGSlideshowDirector>(A);}
 if(!Director||Director->Slides.Num()<3)return 13;
 const FName ID=Director->SlideshowID;
 auto* LevelBP=World->PersistentLevel->GetLevelScriptBlueprint();
 if(!Params.Contains(TEXT("Apply"))&&!Params.Contains(TEXT("Verify")))
 {
  for(auto* N:Template->NPCReplies)UE_LOG(LogTemp,Display,TEXT("SOAP NODE %s text=%s events=%d"),*N->GetID().ToString(),*N->Line.Text.ToString(),N->Events.Num());
  UE_LOG(LogTemp,Display,TEXT("SOAP DIRECTOR id=%s slides=%d"),*ID.ToString(),Director->Slides.Num());return 0;
 }
 for(int Number:{1,3,4,5})if(!Node(Template,Number))return 14;
 auto* Events=FBlueprintEditorUtils::FindEventGraph(BP);
 if(!Events){Events=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("EventGraph"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());FBlueprintEditorUtils::AddUbergraphPage(BP,Events);}
 if(Params.Contains(TEXT("Apply")))
 {
  // Refuse to replace custom user logic if this setup has already been applied.
  for(int Number:{1,3,4})if(!Node(Template,Number)->OnPlayNodeFuncName.IsNone())return 15;
  Backup(TEXT("Content/2DArt/SlideShow/testSlideShow.uasset"));Backup(TEXT("Content/ThirdPerson/Maps/BaseLevel.umap"));
  int Row=0;
  for(int Number:{1,3,4})
  {
   auto* N=Node(Template,Number);auto* GN=GraphNode(DialogueGraph,N);if(!GN)return 16;
   UFunction* Signature=GN->FindFunction(TEXT("OnStartedOrFinished"));if(!Signature)return 17;
   const FString EventName=TEXT("OnDialogueNode Started/Finished Playing - ")+N->GetID().ToString();
   auto* Event=UK2Node_CustomEvent::CreateFromFunction(FVector2D(0,Row*420),Events,EventName,Signature,false);
   N->OnPlayNodeFuncName=Event->CustomFunctionName;
   FindFProperty<FObjectPropertyBase>(GN->GetClass(),TEXT("OnPlayedCustomNode"))->SetObjectPropertyValue_InContainer(GN,Event);
   Event->bCanRenameNode=Event->bIsEditable=false;
   const int SlideNumber=Number==1?1:Number==3?2:3;
   Event->NodeComment=FString::Printf(TEXT("Start this line -> Soap_ep1 image index %d (Show Slide number %d)"),SlideNumber-1,SlideNumber);Event->bCommentBubbleVisible=true;
   auto* Find=Call(Events,AVTGSlideshowDirector::StaticClass(),TEXT("FindSlideshowByID"),360,Row*420+150);Find->FindPinChecked(TEXT("ID"))->DefaultValue=ID.ToString();
   auto* Valid=Call(Events,UKismetSystemLibrary::StaticClass(),TEXT("IsValid"),660,Row*420+150);
   auto* Branch=Add<UK2Node_IfThenElse>(Events,660,Row*420);Branch->AllocateDefaultPins();
   auto* Show=Call(Events,AVTGSlideshowDirector::StaticClass(),TEXT("ShowSlide"),980,Row*420);Show->FindPinChecked(TEXT("SlideNumber"))->DefaultValue=FString::FromInt(SlideNumber);
   Link(Event,TEXT("then"),Branch,TEXT("execute"));Link(Find,TEXT("ReturnValue"),Valid,TEXT("Object"));Link(Valid,TEXT("ReturnValue"),Branch,TEXT("Condition"));Link(Branch,TEXT("then"),Show,TEXT("execute"));Link(Find,TEXT("ReturnValue"),Show,TEXT("self"));
   ++Row;
  }
  // Keep the empty opening/closing nodes as control nodes; wait on visible test lines.
  for(int Number:{2,3,4}){auto* N=Node(Template,Number);if(N){N->Line.Duration=ELineDuration::LD_Never;N->bIsSkippable=true;}}
  auto* End=Node(Template,5);auto* Close=NewObject<UVTGCloseSlideshowEvent>(End,NAME_None,RF_Transactional);Close->SlideshowID=ID;Close->EventRuntime=EEventRuntime::End;Close->FadeSeconds=.5f;End->Events.Add(Close);
  if(!Compile(BP))return 18;
  auto* G=FBlueprintEditorUtils::FindEventGraph(LevelBP);if(!G)return 19;
  auto* Start=Add<UK2Node_CustomEvent>(G,2200,1000);Start->CustomFunctionName=TEXT("StartSoapTVSlideshow");Start->AllocateDefaultPins();
  auto* Player=Call(G,UGameplayStatics::StaticClass(),TEXT("GetPlayerCharacter"),2200,1240);
  auto* Component=Call(G,AActor::StaticClass(),TEXT("GetComponentByClass"),2500,1240);Component->FindPinChecked(TEXT("ComponentClass"))->DefaultObject=UNarrativeComponent::StaticClass();
  auto* CastNode=Add<UK2Node_DynamicCast>(G,2850,1000);CastNode->TargetType=UNarrativeComponent::StaticClass();CastNode->AllocateDefaultPins();
  auto* Begin=Call(G,UNarrativeComponent::StaticClass(),TEXT("BeginDialogue"),3200,1000);Begin->FindPinChecked(TEXT("Dialogue"))->DefaultObject=BP->GeneratedClass;
  Begin->FindPinChecked(TEXT("PlayParams"))->DefaultValue=TEXT("(StartFromID=testSlideShow_DialogueNode_NPC_1,Priority=-1)");
  Begin->NodeComment=TEXT("Edit Dialogue and Start From ID here. Slideshow switching is inside the Dialogue asset.");Begin->bCommentBubbleVisible=true;
  Link(Player,TEXT("ReturnValue"),Component,TEXT("self"));Link(Component,TEXT("ReturnValue"),CastNode,TEXT("Object"));Link(Start,TEXT("then"),CastNode,TEXT("execute"));Link(CastNode,TEXT("then"),Begin,TEXT("execute"));
  UEdGraphPin* CastResult=CastNode->GetCastResultPin();check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(CastResult,Begin->FindPinChecked(TEXT("self"))));
  auto* Failure=Call(G,UKismetSystemLibrary::StaticClass(),TEXT("PrintString"),3200,1260);Failure->FindPinChecked(TEXT("InString"))->DefaultValue=TEXT("SoapTV example: player Narrative Component not found");Link(CastNode,TEXT("CastFailed"),Failure,TEXT("execute"));
  UK2Node_Event* BeginPlay=nullptr;for(UEdGraphNode* N:G->Nodes)if(auto* E=Cast<UK2Node_Event>(N);E&&E->EventReference.GetMemberName()==TEXT("ReceiveBeginPlay"))BeginPlay=E;
  if(!BeginPlay)return 20;
  auto* Out=BeginPlay->FindPinChecked(TEXT("then"));auto Existing=Out->LinkedTo;Out->BreakAllPinLinks();
  auto* Sequence=Add<UK2Node_ExecutionSequence>(G,BeginPlay->NodePosX+240,BeginPlay->NodePosY);Sequence->AllocateDefaultPins();Link(BeginPlay,TEXT("then"),Sequence,TEXT("execute"));
  for(auto* Pin:Existing)check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Sequence->GetThenPinGivenIndex(0),Pin));
  auto* Delay=Call(G,UKismetSystemLibrary::StaticClass(),TEXT("Delay"),Sequence->NodePosX+250,Sequence->NodePosY+220);Delay->FindPinChecked(TEXT("Duration"))->DefaultValue=TEXT("1.0");
  check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Sequence->GetThenPinGivenIndex(1),Delay->FindPinChecked(TEXT("execute"))));
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LevelBP);
  auto* Invoke=Call(G,LevelBP->SkeletonGeneratedClass,TEXT("StartSoapTVSlideshow"),Delay->NodePosX+340,Delay->NodePosY);
  // The new custom event must be exposed on the skeleton before creating its call node.
  if(!Invoke->FindPin(TEXT("execute")))return 21;
  Link(Delay,TEXT("then"),Invoke,TEXT("execute"));
  if(!Compile(LevelBP))return 22;
  if(!Save(BP)||!Save(World))return 23;
 }
 // Check saved/generated dialogue template, event binding and close runtime.

 for(int Number:{1,3,4})if(Node(Template,Number)->OnPlayNodeFuncName.IsNone()||!BP->GeneratedClass->FindFunctionByName(Node(Template,Number)->OnPlayNodeFuncName))return 24;
 bool HasClose=false;for(auto* E:Node(Template,5)->Events)if(auto* C=Cast<UVTGCloseSlideshowEvent>(E);C&&C->SlideshowID==ID&&C->EventRuntime==EEventRuntime::End)HasClose=true;
 if(!HasClose||!LevelBP->GeneratedClass->FindFunctionByName(TEXT("StartSoapTVSlideshow")))return 25;
 UE_LOG(LogTemp,Display,TEXT("SOAP EXAMPLE PASS id=%s NPC1=index0 NPC3=index1 NPC4=index2 NPC5=Close.End"),*ID.ToString());return 0;
}

