#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "VTGLevelStatics.h"
#include "HAL/FileManager.h"

namespace AlleyFix
{
const TCHAR* Marker=TEXT("VTG alley repair");
UEdGraphNode* Node(UEdGraph* G,const TCHAR* Name) { for(UEdGraphNode* N:G->Nodes) if(N->GetName()==Name) return N; checkNoEntry();return nullptr; }
UEdGraphPin* Pin(UEdGraphNode* N,const TCHAR* Name) { return N->FindPinChecked(Name); }
void Link(UEdGraphPin* A,UEdGraphPin* B) { check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A,B)); }
template<class T> T* Add(UEdGraph* G,int X,int Y)
{ auto* N=NewObject<T>(G); G->AddNode(N,false,false); N->CreateNewGuid(); N->NodePosX=X;N->NodePosY=Y;N->NodeComment=Marker;return N; }
UK2Node_CallFunction* Call(UEdGraph* G,UFunction* F,int X,int Y)
{ check(F);auto* N=Add<UK2Node_CallFunction>(G,X,Y);N->SetFromFunction(F);N->AllocateDefaultPins();return N; }
bool Save(UBlueprint* BP)
{
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
 if(Log.NumErrors || BP->Status==BS_Error) return false;
 const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 const FString Backup=FPaths::ProjectSavedDir()/TEXT("QA/AlleyBefore")/(BP->GetName()+TEXT(".uasset"));
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
 if(!IFileManager::Get().FileExists(*Backup)) check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
 FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
 return UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args);
}
int32 Repair()
{
 auto* Player=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));
 auto* Manager=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/SaApartment/BPLM_AptAlley"));
 auto* Dialogue=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L3SaApartment_Day1/L3_AptAlley_Day1"));
 check(Player&&Manager&&Dialogue);
 UEdGraph* G=Player->UbergraphPages[0];
 bool Already=false;for(UEdGraphNode* N:G->Nodes) if(N->NodeComment.Contains(Marker))Already=true;
 if(!Already) {
 auto* Move=Node(G,TEXT("K2Node_CallFunction_7346"));
 auto* Yaw=Pin(Node(G,TEXT("K2Node_CallFunction_243")),TEXT("ReturnValue_Yaw"));
 auto* Old=Pin(Move,TEXT("WorldDirection"));
 check(Old->LinkedTo.Num()==1);
 auto* Legacy=Old->LinkedTo[0];
 auto* Right=Call(G,UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("GetRightVector")),Move->NodePosX-800,Move->NodePosY+500);
 GetDefault<UEdGraphSchema_K2>()->SplitPin(Pin(Right,TEXT("InRot")),false);
 Link(Yaw,Pin(Right,TEXT("InRot_Yaw")));
 auto* Select=Call(G,UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("SelectVector")),Move->NodePosX-400,Move->NodePosY+500);
 Link(Pin(Right,TEXT("ReturnValue")),Pin(Select,TEXT("A")));
 Link(Legacy,Pin(Select,TEXT("B")));
 Link(Pin(Node(G,TEXT("K2Node_VariableGet_169")),TEXT("fixed cam add input special value?")),Pin(Select,TEXT("bPickA")));
 Old->BreakAllPinLinks();Link(Pin(Select,TEXT("ReturnValue")),Old);
 Select->NodeComment=TEXT("VTG alley repair: authored fixed yaw must drive BOTH axes. Preserve legacy controls when override is off.");
 if(!Save(Player)) return 2;
 }
 UEdGraph* MG=Manager->UbergraphPages[0];
 auto* Open=Node(MG,TEXT("K2Node_CallFunction_25"));
 GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Pin(Open,TEXT("LevelName")),TEXT("/Game/ThirdPerson/Maps/PoliceStation/L3_CentralPoliceStation_FrontDoor_DownStream"));
 GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Pin(Open,TEXT("Stage")),TEXT("L3Day1Morning"));
 Open->NodeComment=TEXT("VTG alley repair: police entrance and its authored morning stage");
 if(!Save(Manager))return 3;
 const FName EventName(TEXT("VTG_EnterPoliceStation"));
 UEdGraph* DG=Dialogue->UbergraphPages[0];
 UK2Node_CustomEvent* Event=nullptr;
 for(UEdGraphNode* N:DG->Nodes) if(auto* E=Cast<UK2Node_CustomEvent>(N);E&&E->CustomFunctionName==EventName) Event=E;
 if(!Event) {
 Event=Add<UK2Node_CustomEvent>(DG,4000,0);Event->CustomFunctionName=EventName;Event->AllocateDefaultPins();
 FEdGraphPinType Type;Type.PinCategory=UEdGraphSchema_K2::PC_Object;Type.PinSubCategoryObject=UDialogueNode::StaticClass();
 Event->CreateUserDefinedPin(TEXT("Node"),Type,EGPD_Output);
 Type.PinCategory=UEdGraphSchema_K2::PC_Boolean;Type.PinSubCategoryObject=nullptr;
 Event->CreateUserDefinedPin(TEXT("bStarted"),Type,EGPD_Output);
 auto* Branch=Add<UK2Node_IfThenElse>(DG,4300,0);Branch->AllocateDefaultPins();
 Link(Pin(Event,TEXT("then")),Pin(Branch,TEXT("execute"))); Link(Pin(Event,TEXT("bStarted")),Pin(Branch,TEXT("Condition")));
 auto* Find=Call(DG,UVTGLevelStatics::StaticClass()->FindFunctionByName(TEXT("FindValidActorOfClass")),4300,250);
 GetDefault<UEdGraphSchema_K2>()->TrySetDefaultObject(*Pin(Find,TEXT("ActorClass")),Manager->GeneratedClass);
 auto* Leave=Call(DG,Manager->GeneratedClass->FindFunctionByName(TEXT("Leave")),4700,0);
 Link(Pin(Branch,TEXT("then")),Pin(Leave,TEXT("execute")));
 Link(Pin(Find,TEXT("ReturnValue")),Pin(Leave,TEXT("self")));
 }
 auto* Property=FindFProperty<FObjectPropertyBase>(Dialogue->GetClass(),TEXT("DialogueTemplate"));
 auto* Template=CastChecked<UDialogue>(Property->GetObjectPropertyValue_InContainer(Dialogue));
 bool Found=false;
 for(auto* N:Template->GetNodes())if(N->GetName()==TEXT("DialogueNode_NPC_5"))
 { check(N->OnPlayNodeFuncName.IsNone()||N->OnPlayNodeFuncName==EventName);N->OnPlayNodeFuncName=EventName;Found=true; }
 check(Found);
 if(!Save(Dialogue))return 4;
 auto* Generated=CastChecked<UDialogueBlueprintGeneratedClass>(Dialogue->GeneratedClass)->GetDialogueTemplate();
 bool Bound=false;
 for(auto* N:Generated->GetNodes())if(N->GetName()==TEXT("DialogueNode_NPC_5")) Bound=N->OnPlayNodeFuncName==EventName;
 check(Bound);
 UE_LOG(LogTemp,Display,TEXT("ALLEY_REPAIR PASS: camera override axes corrected, police option bound, destination/stage configured; original nodes retained"));
 return 0;
}
}
