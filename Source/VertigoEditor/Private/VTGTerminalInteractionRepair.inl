#include "UI/VTGTerminalWidget.h"
#include "Components/BackgroundBlur.h"

namespace TerminalInteraction
{
const FString Marker=TEXT("VTG terminal input safety");
UEdGraph* Graph(UBlueprint* BP,const TCHAR* Name)
{
 for(UEdGraph* G:BP->FunctionGraphs)if(G->GetName().Replace(TEXT(" "),TEXT("")).Equals(Name,ESearchCase::IgnoreCase))return G;
 checkf(false,TEXT("Missing graph %s on %s"),Name,*BP->GetName());return nullptr;
}
void RedirectDocument(UBlueprint* BP,const TCHAR* GraphName,const TCHAR* Input,bool Verify)
{
 auto* G=Graph(BP,GraphName);auto* Entry=AlleyFix::Node(G,TEXT("K2Node_FunctionEntry_0"));auto* Pin=Entry->FindPinChecked(Input);
 UK2Node_CallFunction* Resolve=nullptr;
 for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N);C && C->FunctionReference.GetMemberName()==TEXT("ResolveTerminalTabIndex"))Resolve=C;
 if(!Verify && !Resolve)
 {
  const auto Links=Pin->LinkedTo;Pin->BreakAllPinLinks();
  Resolve=AlleyFix::Call(G,UVTGTerminalSaveLibrary::StaticClass()->FindFunctionByName(TEXT("ResolveTerminalTabIndex")),Entry->NodePosX+200,Entry->NodePosY+200);Resolve->NodeComment=Marker;
  AlleyFix::Link(Pin,Resolve->FindPinChecked(TEXT("Index")));
  for(auto* Link:Links)AlleyFix::Link(Resolve->FindPinChecked(TEXT("ReturnValue")),Link);
 }
 check(Resolve && Pin->LinkedTo.Num()==1 && Pin->LinkedTo[0]->GetOwningNode()==Resolve);
}
int32 Run(bool Verify)
{
 auto* Player=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));
 auto* Terminal=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/WB_TabSwitcher"));
 auto* Menu=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/WB_TabSwitcherMenu"));check(Player&&Terminal&&Menu);
 if(!Verify)for(auto* BP:{Player,static_cast<UBlueprint*>(Terminal),static_cast<UBlueprint*>(Menu)})
 {
  const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),TEXT(".uasset"));
  const FString Backup=FPaths::ProjectSavedDir()/TEXT("QA/TerminalInteractionBefore")/(BP->GetName()+TEXT(".uasset"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
  if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
 }
 UEdGraph* G=Player->UbergraphPages[0];auto* Input=AlleyFix::Node(G,TEXT("K2Node_EnhancedInputAction_8"));
 auto* ExtraMode=AlleyFix::Node(G,TEXT("K2Node_CallFunction_219"));auto* ReadyDelay=AlleyFix::Pin(AlleyFix::Node(G,TEXT("K2Node_CallFunction_275")),TEXT("Duration"));
 UK2Node_CallFunction* Gate=nullptr;
 UK2Node_CallFunction* Sync=nullptr;
 for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N);C && C->FunctionReference.GetMemberName()==TEXT("TryAcceptTerminalInput"))Gate=C;
 for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N);C && C->FunctionReference.GetMemberName()==TEXT("SynchronizeTerminalInput"))Sync=C;
 if(!Verify)
 {
  if(!Gate)
  {
   auto* Trigger=Input->FindPinChecked(TEXT("Triggered"));check(Trigger->LinkedTo.Num()==1);auto* Old=Trigger->LinkedTo[0];Trigger->BreakAllPinLinks();
   Gate=AlleyFix::Call(G,UVTGTerminalSaveLibrary::StaticClass()->FindFunctionByName(TEXT("TryAcceptTerminalInput")),Input->NodePosX+220,Input->NodePosY-300);Gate->NodeComment=Marker;
   auto* Self=AlleyFix::Add<UK2Node_Self>(G,Gate->NodePosX-160,Gate->NodePosY+150);Self->AllocateDefaultPins();AlleyFix::Link(Self->FindPinChecked(TEXT("self")),Gate->FindPinChecked(TEXT("Player")));
   auto* Branch=AlleyFix::Add<UK2Node_IfThenElse>(G,Gate->NodePosX+300,Gate->NodePosY);Branch->AllocateDefaultPins();Branch->NodeComment=Marker;
   AlleyFix::Link(Input->FindPinChecked(TEXT("Started")),Gate->FindPinChecked(TEXT("execute")));AlleyFix::Link(Gate->FindPinChecked(TEXT("then")),Branch->FindPinChecked(TEXT("execute")));AlleyFix::Link(Gate->FindPinChecked(TEXT("ReturnValue")),Branch->FindPinChecked(TEXT("Condition")));AlleyFix::Link(Branch->FindPinChecked(TEXT("then")),Old);
  }
  ExtraMode->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();ExtraMode->NodeComment=TEXT("Disconnected: Inventory HUD owns input mode; never set UI Only after a possible close");
  if(!Sync)
  {
   Sync=AlleyFix::Call(G,UVTGTerminalSaveLibrary::StaticClass()->FindFunctionByName(TEXT("SynchronizeTerminalInput")),ExtraMode->NodePosX+150,ExtraMode->NodePosY+250);Sync->NodeComment=TEXT("VTG terminal: synchronize actual visibility, owning player, UI focus and cursor after opening");
   auto* Self=AlleyFix::Add<UK2Node_Self>(G,Sync->NodePosX-150,Sync->NodePosY+150);Self->AllocateDefaultPins();AlleyFix::Link(Self->FindPinChecked(TEXT("self")),Sync->FindPinChecked(TEXT("Player")));
   for(const TCHAR* Name:{TEXT("K2Node_CallFunction_122"),TEXT("K2Node_CallFunction_227")})AlleyFix::Link(AlleyFix::Node(G,Name)->FindPinChecked(TEXT("then")),Sync->FindPinChecked(TEXT("execute")));
  }
  GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*ReadyDelay,TEXT("1.5"));
  Terminal->ParentClass=UVTGTerminalWidget::StaticClass();FBlueprintEditorUtils::RefreshAllNodes(Terminal);
 }
 check(Gate && Input->FindPinChecked(TEXT("Triggered"))->LinkedTo.IsEmpty() && Input->FindPinChecked(TEXT("Started"))->LinkedTo.Num()==1);
 check(ExtraMode->FindPinChecked(TEXT("execute"))->LinkedTo.IsEmpty() && FCString::Atof(*ReadyDelay->DefaultValue)==1.5f);
 check(Sync);
 for(const TCHAR* Name:{TEXT("K2Node_CallFunction_122"),TEXT("K2Node_CallFunction_227")})check(AlleyFix::Node(G,Name)->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Sync->FindPinChecked(TEXT("execute"))));
 check(Terminal->ParentClass==UVTGTerminalWidget::StaticClass());
 RedirectDocument(Terminal,TEXT("SetActiveTab"),TEXT("Index"),Verify);
 RedirectDocument(Menu,TEXT("SetActiveButtonOnOpen"),TEXT("TabIndex"),Verify);
 if(!Verify)for(auto* BP:{Player,static_cast<UBlueprint*>(Terminal),static_cast<UBlueprint*>(Menu)})check(AlleyFix::Save(BP));
 FVTGTerminalSaveRegression::Run();
 UE_LOG(LogTemp,Display,TEXT("TERMINAL_INTERACTION_PASS: Started-only input, 1.5-second startup gate, shared 250ms debounce, no redundant UI input mode, Document unreachable, inherited background blur"));
 return 0;
}
}
