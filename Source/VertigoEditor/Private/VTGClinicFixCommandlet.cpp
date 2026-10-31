#include "VTGClinicFixCommandlet.h"
#include "VTGClinicFix.h"
#include "VTGNarrativeInsert.h"
#include "Quests/VTGEnsureQuestStateEvent.h"
#include "DialogueBlueprint.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "Engine/Blueprint.h"
#include "UObject/UnrealType.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "Quest.h"
#include "QuestSM.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/RichTextBlock.h"
#include "Components/Image.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

namespace ClinicFix
{
void Save(UBlueprint* BP)
{
 FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);
 check(Log.NumErrors==0 && BP->Status!=BS_Error);
 auto* Package=BP->GetOutermost();const FString File=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
 const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/ClinicFix/Backup")/(BP->GetName()+TEXT(".uasset"));
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
 if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
 FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(Package,BP,*File,A));
 UE_LOG(LogTemp,Display,TEXT("CLINIC SAVED %s, compile errors=%d"),*BP->GetName(),Log.NumErrors);
}
UDialogueNode* Node(UDialogue* D,const TCHAR* ID)
{for(auto* N:D->GetNodes())if(N->GetID()==ID)return N;checkNoEntry();return nullptr;}
template<class T> T* Event(UDialogueNode* N,bool Verify)
{
 for(auto* E:N->Events)if(auto* Found=Cast<T>(E))return Found;
 check(!Verify);auto* E=NewObject<T>(N,NAME_None,RF_Transactional);E->EventRuntime=EEventRuntime::Start;N->Events.Add(E);return E;
}
}
bool VTGClinicRuntimeTest();
void VTGClinicOverlayPreview();
int32 UVTGClinicFixCommandlet::Main(const FString& Params)
{
 if(Params.Contains(TEXT("RuntimeTest")))return VTGClinicRuntimeTest()?0:10;
 if(Params.Contains(TEXT("Preview"))){VTGClinicOverlayPreview();return 0;}
 using namespace ClinicFix;const bool Verify=Params.Contains(TEXT("VerifyOnly"));
 auto* D=LoadObject<UDialogueBlueprint>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L5Clinic/L5Clinic_Still"));check(D);
 auto* Template=Verify?CastChecked<UDialogueBlueprintGeneratedClass>(D->GeneratedClass)->GetDialogueTemplate():D->DialogueTemplate;
 auto* ButtonNode=Node(Template,TEXT("L5Clinic_Still_DialogueNode_NPC_232"));
 auto* QuestEvent=Event<UVTGEnsureQuestStateEvent>(ButtonNode,Verify);
 if(!Verify)QuestEvent->StateID=TEXT("QuestState_11");check(QuestEvent->StateID==TEXT("QuestState_11"));
 auto* Quest=NewObject<UQuest>(GetTransientPackage(),QuestEvent->Quest);check(Quest);
 // Read the authored quest template: branch 78 must leave precisely the state selected above.
 auto* QBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L5Clinic/Q_L5Clinic"));check(QBP);
 UObject* QT=FindObject<UObject>(QBP,TEXT("QuestTemplate"));check(QT);
 auto* State=FindObject<UQuestState>(QT,TEXT("QuestState_11"));check(State);
 bool HasBranch=false;for(auto* B:State->Branches)if(B && B->GetID()==TEXT("QuestBranch_78"))HasBranch=true;check(HasBranch);
 Event<UVTGClinicButtonEvent>(ButtonNode,Verify);
 auto* LobbyNode=Node(Template,TEXT("L2_Street_NPC_DialogueNode_NPC_9"));
 if(!Verify)LobbyNode->Events.RemoveAll([](UNarrativeEvent* E){return E && E->IsA<UVTGClinicLobbyCameraEvent>();});
 for(auto* E:LobbyNode->Events)check(!E->IsA<UVTGClinicLobbyCameraEvent>());
 if(!Verify)Save(D);
 UE_LOG(LogTemp,Display,TEXT("CLINIC PASS: node 232 enables button and targets state 11 / branch 78; lobby uses Narrative camera Tick restoration"));

 auto* Yard=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L5Clinic/ShoppingCenterYard/BPLM_ShoppingCenter_ClinicYard"));check(Yard);
 UEdGraph* G=Yard->UbergraphPages[0];auto* Schema=GetDefault<UEdGraphSchema_K2>();
 // Each authored idle setter must evaluate its new pose synchronously before the next camera cut.
 int32 Refreshed=0;const auto Nodes=G->Nodes;
 for(UEdGraphNode* N:Nodes)if(auto* Call=Cast<UK2Node_CallFunction>(N);Call && Call->FunctionReference.GetMemberName()==TEXT("Set Idle Anim For ABP"))
 {
  auto* Out=Call->FindPinChecked(TEXT("then"));bool Done=false;
  for(auto* Link:Out->LinkedTo)if(auto* C=Cast<UK2Node_CallFunction>(Link->GetOwningNode());C && C->FunctionReference.GetMemberName()==TEXT("RefreshNarrativePose"))Done=true;
  if(!Done && !Verify)
  {
   auto* C=NewObject<UK2Node_CallFunction>(G);G->AddNode(C,false,false);C->CreateNewGuid();C->SetFromFunction(UVTGNarrativeInsertLibrary::StaticClass()->FindFunctionByName(TEXT("RefreshNarrativePose")));C->AllocateDefaultPins();C->NodePosX=Call->NodePosX+260;C->NodePosY=Call->NodePosY;
   auto Links=Out->LinkedTo;Out->BreakAllPinLinks();for(auto* L:Links)check(Schema->TryCreateConnection(C->FindPinChecked(TEXT("then")),L));
   check(Schema->TryCreateConnection(Out,C->FindPinChecked(TEXT("execute"))));
   for(auto* L:Call->FindPinChecked(TEXT("self"))->LinkedTo)check(Schema->TryCreateConnection(L,C->FindPinChecked(TEXT("Character"))));Done=true;
  }
  check(Done);++Refreshed;
 }
 check(Refreshed>=2);if(!Verify)Save(Yard);
 UE_LOG(LogTemp,Display,TEXT("CLINIC PASS: %d yard idle setters evaluate pose in same frame"),Refreshed);

 auto* W=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay"));check(W);
 auto* Text=CastChecked<URichTextBlock>(W->WidgetTree->FindWidget(TEXT("Line_Side")));
 auto* Image=CastChecked<UImage>(W->WidgetTree->FindWidget(TEXT("DialogueBox_Side")));
 auto* Panel=Cast<UOverlay>(Text->GetParent());
 if(!Panel && !Verify)
 {
  auto* Canvas=CastChecked<UCanvasPanel>(Text->GetParent());const auto Layout=CastChecked<UCanvasPanelSlot>(Text->Slot)->GetLayout();
  Text->RemoveFromParent();Image->RemoveFromParent();
  Panel=W->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),TEXT("SideDialogueContainer"));
  auto* Slot=Canvas->AddChildToCanvas(Panel);Slot->SetLayout(Layout);Slot->SetAutoSize(true);Slot->SetZOrder(1);
  auto* Background=Panel->AddChildToOverlay(Image);Background->SetHorizontalAlignment(HAlign_Fill);Background->SetVerticalAlignment(VAlign_Fill);
  // The image fills the desired size provided by wrapped text plus these insets.
  Image->SetDesiredSizeOverride(FVector2D::ZeroVector);
  auto* Content=Panel->AddChildToOverlay(Text);Content->SetPadding(FMargin(24.f,16.f));Content->SetHorizontalAlignment(HAlign_Fill);Content->SetVerticalAlignment(VAlign_Fill);
  Text->SetAutoWrapText(true);FindFProperty<FFloatProperty>(Text->GetClass(),TEXT("WrapTextAt"))->SetPropertyValue_InContainer(Text,352.f);Text->SetJustification(ETextJustify::Left);
 }
 check(Panel && Panel==Image->GetParent());check(CastChecked<UCanvasPanelSlot>(Panel->Slot)->GetAutoSize());
 check(CastChecked<UOverlaySlot>(Text->Slot)->GetPadding()==FMargin(24.f,16.f));
 if(!Verify)Save(W);
 UE_LOG(LogTemp,Display,TEXT("CLINIC PASS: side text and background share auto-sized overlay, 24x16 padding"));
 return 0;
}

