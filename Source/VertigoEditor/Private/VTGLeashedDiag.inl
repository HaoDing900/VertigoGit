#include "DialogueBlueprintGeneratedClass.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Variable.h"

/** Read-only diagnostics. Nothing here saves an asset. */
struct FVTGGraphDiag
{
 static void Describe(UEdGraphPin* Out)
 {
  if(Out->LinkedTo.Num()==0){UE_LOG(LogTemp,Display,TEXT("    then -> (nothing)"));return;}
  for(UEdGraphPin* L:Out->LinkedTo)
  {
   UEdGraphNode* N=L->GetOwningNode();
   UE_LOG(LogTemp,Display,TEXT("    then -> %s [%s] pin=%s comment=\"%s\" title=\"%s\""),
    *N->GetName(),*N->GetClass()->GetName(),*L->PinName.ToString(),*N->NodeComment,
    *N->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"),TEXT(" ")));
  }
 }

 /** Why does -LevelProgress -VerifyOnly assert? Print what the 4 gated events actually look like. */
 static int32 LevelProgressShape()
 {
  using namespace AlleyFix;
  auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/L4BlackMarketEntrance/BPLM_BlackMarketEntrance"));
  check(BP);
  UEdGraph* G=BP->UbergraphPages[0];
  const TCHAR* Events[]={TEXT("K2Node_CustomEvent_0"),TEXT("K2Node_CustomEvent_2"),TEXT("K2Node_CustomEvent_3"),TEXT("K2Node_CustomEvent_4")};
  const TCHAR* Ids[]={TEXT("BlackMarket.Intro"),TEXT("BlackMarket.MailNotice"),TEXT("BlackMarket.DistantArgue"),TEXT("BlackMarket.MeetSequence")};
  for(int32 I=0;I<4;++I)
  {
   UEdGraphNode* Entry=Node(G,Events[I]);
   UEdGraphPin* Out=Pin(Entry,TEXT("then"));
   const FString Mark=FString(TEXT("VTG saved story: "))+Ids[I];
   const bool bGated=Out->LinkedTo.Num()==1 && Out->LinkedTo[0]->GetOwningNode()->NodeComment==Mark;
   UE_LOG(LogTemp,Display,TEXT("  %s (%s) links=%d gated=%s"),Events[I],Ids[I],Out->LinkedTo.Num(),bGated?TEXT("YES"):TEXT("NO"));
   Describe(Out);
  }
  // Is the EnterSavedStoryEvent gate present anywhere in this graph at all?
  int32 Gates=0;
  for(UEdGraphNode* N:G->Nodes)
   if(N->NodeComment.StartsWith(TEXT("VTG saved story: "))) ++Gates;
  UE_LOG(LogTemp,Display,TEXT("  nodes carrying a \"VTG saved story\" marker: %d (expected 8: gate+branch per event)"),Gates);
  return 0;
 }


 /** Walk the exec chain backwards from Start up to the event that drives it. */
 static void TraceToEntry(UEdGraphNode* Start)
 {
  UEdGraphNode* N=Start;
  for(int32 Hop=0;Hop<30 && N;++Hop)
  {
   UEdGraphPin* In=nullptr;
   for(UEdGraphPin* P:N->Pins)
    if(P->Direction==EGPD_Input && P->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec){In=P;break;}
   if(!In || In->LinkedTo.Num()==0)
   {
    UE_LOG(LogTemp,Display,TEXT("      ^ driven by: %s [%s] \"%s\""),*N->GetName(),*N->GetClass()->GetName(),
     *N->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"),TEXT(" ")));
    return;
   }
   N=In->LinkedTo[0]->GetOwningNode();
  }
  UE_LOG(LogTemp,Display,TEXT("      ^ driven by: (chain longer than 30 hops)"));
 }

 static void DumpNode(UEdGraphNode* N)
 {
  UE_LOG(LogTemp,Display,TEXT("    NODE %s [%s] \"%s\" comment=\"%s\""),*N->GetName(),*N->GetClass()->GetName(),
   *N->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"),TEXT(" ")),*N->NodeComment);
  for(UEdGraphPin* P:N->Pins)
  {
   FString Line=FString::Printf(TEXT("      %s %s"),*P->PinName.ToString(),P->Direction==EGPD_Input?TEXT("IN"):TEXT("OUT"));
   if(!P->DefaultValue.IsEmpty()) Line+=TEXT(" default=")+P->DefaultValue;
   for(UEdGraphPin* L:P->LinkedTo)
    Line+=FString::Printf(TEXT("  -> %s.%s [%s]"),*L->GetOwningNode()->GetName(),*L->PinName.ToString(),
     *L->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"),TEXT(" ")));
   UE_LOG(LogTemp,Display,TEXT("%s"),*Line);
  }
 }

 /** The medicated/leashed plumbing that already exists on the BPLM parent. */
 static int32 LeashedDetail()
 {
  auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/BPLM"));check(BP);
  const TSet<FName> Interesting={TEXT("IsMedicated?"),TEXT("Leashed?")};
  TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
  for(UEdGraph* G : Graphs)
   for(UEdGraphNode* N : G->Nodes)
    if(auto* Var=Cast<UK2Node_Variable>(N))
     if(Interesting.Contains(Var->VariableReference.GetMemberName()))
     {
      UE_LOG(LogTemp,Display,TEXT("  --- %s in %s"),*Var->VariableReference.GetMemberName().ToString(),*G->GetName());
      DumpNode(N);
      TraceToEntry(N);
     }
  return 0;
 }


 /** Who actually calls the parent's Set Leashed / Set Raw / Detect LeashedOrRaw? events. */
 static int32 LeashedCallers()
 {
  const TCHAR* Assets[]={
   TEXT("/Game/LevelNSequence/BPLM"),
   TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"),
   TEXT("/Game/LevelNSequence/In_Level/SaApartment/BPLM_SaApartment"),
   TEXT("/Game/LevelNSequence/In_Level/SaApartment/BPLM_AptAlley"),
   TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt"),
   TEXT("/Game/LevelNSequence/In_Level/L2Bar/BPLM_BarBack"),
   TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight"),
   TEXT("/Game/LevelNSequence/In_Level/L2Tunnel/BPLM_L2TunnelCine"),
   TEXT("/Game/LevelNSequence/In_Level/L3PoliceStation/BPLM_PoliceStation"),
   TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket"),
   TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/AdultStore/BPLM_AdultStore"),
   TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/L4BlackMarketEntrance/BPLM_BlackMarketEntrance"),
   TEXT("/Game/LevelNSequence/In_Level/L4Tunnel_NearMarket/BPLM_Tunnel_NearMarket"),
   TEXT("/Game/LevelNSequence/In_Level/L5Clinic/BPLM_Clinic"),
   TEXT("/Game/LevelNSequence/In_Level/L5Clinic/ShoppingCenterYard/BPLM_ShoppingCenter_ClinicYard"),
   TEXT("/Game/LevelNSequence/In_Level/L6_Tunnel_MarketToFactory/BPLM_Tunnel_MarketToFactory")};
  const TSet<FName> Events={TEXT("Set Leashed"),TEXT("Set Raw"),TEXT("Detect LeashedOrRaw?")};
  int32 Found=0;
  for(const TCHAR* Path : Assets)
  {
   auto* BP=LoadObject<UBlueprint>(nullptr,Path);
   if(!BP){UE_LOG(LogTemp,Display,TEXT("MISSING %s"),Path);continue;}
   TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
   for(UEdGraph* G : Graphs)
    for(UEdGraphNode* N : G->Nodes)
    {
     FName Member=NAME_None;
     if(auto* Call=Cast<UK2Node_CallFunction>(N)) Member=Call->FunctionReference.GetMemberName();
     else if(auto* Event=Cast<UK2Node_Event>(N)) Member=Event->EventReference.GetMemberName();
     if(!Events.Contains(Member)) continue;
     ++Found;
     UE_LOG(LogTemp,Display,TEXT("  %s : %s \"%s\" [%s] in graph %s"),
      *FPaths::GetBaseFilename(Path),*Member.ToString(),
      *N->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"),TEXT(" ")),
      *N->GetClass()->GetName(),*G->GetName());
     TraceToEntry(N);
    }
  }
  UE_LOG(LogTemp,Display,TEXT("  total nodes referencing those three events: %d"),Found);
  return 0;
 }

 /** Where did the BlackMarket.Intro gate go? Print every marked node and whether it is still wired. */
 static int32 OrphanGates()
 {
  auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/L4BlackMarketEntrance/BPLM_BlackMarketEntrance"));check(BP);
  for(UEdGraphNode* N : BP->UbergraphPages[0]->Nodes)
  {
   if(!N->NodeComment.StartsWith(TEXT("VTG saved story: "))) continue;
   int32 ExecIn=0;
   for(UEdGraphPin* P:N->Pins)
    if(P->Direction==EGPD_Input && P->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec) ExecIn+=P->LinkedTo.Num();
   if(N->NodeComment.Contains(TEXT("BlackMarket.Intro"))) TraceToEntry(N);
   UE_LOG(LogTemp,Display,TEXT("  %s [%s] \"%s\" marker=\"%s\" incoming-exec=%d%s"),
    *N->GetName(),*N->GetClass()->GetName(),
    *N->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"),TEXT(" ")),
    *N->NodeComment,ExecIn,ExecIn==0?TEXT("   <-- ORPHANED"):TEXT(""));
  }
  return 0;
 }




 /** Which nodes do the Cond_PlayedNode conditions point at - NPC lines, or player options? */
 static int32 PlayedNodeConditions()
 {
  const TCHAR* Paths[]={
   TEXT("/Game/Narrative/Dialogues/Levels/L3PoliceStation/L3PoliceStation_Still"),
   TEXT("/Game/Narrative/Dialogues/Levels/L3SaApartment_Day1/L3_SaApartmentD1"),
   TEXT("/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_InterestingItemsOntheStreet"),
   TEXT("/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation"),
   TEXT("/Game/Narrative/Dialogues/Levels/L5Clinic/L5Clinic_Still")};
  for(const TCHAR* Path : Paths)
  {
   auto* Object=LoadObject<UObject>(nullptr,Path);
   auto* BP=Cast<UBlueprint>(Object);
   if(Object && !BP)
    if(auto* P=FindFProperty<FObjectPropertyBase>(Object->GetClass(),TEXT("DirectorBlueprint")))
     BP=Cast<UBlueprint>(P->GetObjectPropertyValue_InContainer(Object));
   auto* Class=BP?Cast<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass):nullptr;
   auto* Template=Class?Class->GetDialogueTemplate():nullptr;
   if(!Template){UE_LOG(LogTemp,Display,TEXT("NO TEMPLATE %s"),Path);continue;}
   UE_LOG(LogTemp,Display,TEXT("DIALOGUE %s"),*FPaths::GetBaseFilename(Path));
   TMap<FString,FString> IdToClass;
   for(auto* N : Template->GetNodes())
    if(auto* Id=FindFProperty<FNameProperty>(N->GetClass(),TEXT("ID")))
     IdToClass.Add(Id->GetPropertyValue_InContainer(N).ToString(),N->GetClass()->GetName());
   for(auto* N : Template->GetNodes())
   {
    auto* CondArray=FindFProperty<FArrayProperty>(N->GetClass(),TEXT("Conditions"));
    if(!CondArray)continue;
    FScriptArrayHelper Conds(CondArray,CondArray->ContainerPtrToValuePtr<void>(N));
    auto* Inner=CastField<FObjectPropertyBase>(CondArray->Inner);
    if(!Inner)continue;
    for(int32 I=0;I<Conds.Num();++I)
    {
     UObject* Cond=Inner->GetObjectPropertyValue(Conds.GetRawPtr(I));
     if(!Cond || !Cond->GetClass()->GetName().Contains(TEXT("PlayedNode")))continue;
     FString Args;
     for(TFieldIterator<FProperty> P(Cond->GetClass());P;++P)
     {
      if(!P->GetOwnerClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))continue;
      FString V;P->ExportText_InContainer(0,V,Cond,Cond,Cond,PPF_None);
      Args+=FString::Printf(TEXT(" %s=%s"),*P->GetName(),*V);
      if(const FString* Target=IdToClass.Find(V))Args+=FString::Printf(TEXT(" -> node class %s"),**Target);
     }
     auto* OwnId=FindFProperty<FNameProperty>(N->GetClass(),TEXT("ID"));
     UE_LOG(LogTemp,Display,TEXT("  on %s (%s) cond %s:%s"),
      OwnId?*OwnId->GetPropertyValue_InContainer(N).ToString():TEXT("?"),
      *N->GetClass()->GetName(),*Cond->GetClass()->GetName(),*Args);
    }
   }
  }
  return 0;
 }

 /** What do the BPLMs actually destroy? Needed to know where IVTGSaveable has to go. */
 static int32 DestroyTargets()
 {
  TArray<FString> Paths;
  IFileManager::Get().FindFilesRecursive(Paths,*(FPaths::ProjectContentDir()/TEXT("LevelNSequence")),TEXT("BPLM*.uasset"),true,false);
  TMap<FString,int32> Classes;
  for(const FString& File : Paths)
  {
   FString Package;
   if(!FPackageName::TryConvertFilenameToLongPackageName(File,Package))continue;
   auto* BP=Cast<UBlueprint>(LoadObject<UObject>(nullptr,*Package));
   if(!BP)continue;
   TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
   for(UEdGraph* G : Graphs)
    for(UEdGraphNode* N : G->Nodes)
    {
     auto* Call=Cast<UK2Node_CallFunction>(N);
     if(!Call || Call->FunctionReference.GetMemberName()!=FName(TEXT("K2_DestroyActor"))) continue;
     UEdGraphPin* Self=N->FindPin(TEXT("self"));
     if(!Self){UE_LOG(LogTemp,Display,TEXT("%s: destroy with no self pin"),*FPaths::GetBaseFilename(Package));continue;}
     if(Self->LinkedTo.Num()==0)
     {
      UE_LOG(LogTemp,Display,TEXT("%s: destroys SELF (the BPLM)"),*FPaths::GetBaseFilename(Package));
      continue;
     }
     for(UEdGraphPin* Source : Self->LinkedTo)
     {
      UEdGraphNode* Owner=Source->GetOwningNode();
      const FString Type=Source->PinType.PinSubCategoryObject.IsValid()?Source->PinType.PinSubCategoryObject->GetPathName():Source->PinType.PinCategory.ToString();
      UE_LOG(LogTemp,Display,TEXT("%s: destroys <- %s \"%s\" type=%s"),*FPaths::GetBaseFilename(Package),
       *Owner->GetName(),*Owner->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"),TEXT(" ")),*Type);
      Classes.FindOrAdd(Type)++;
     }
    }
  }
  UE_LOG(LogTemp,Display,TEXT("---- destroyed types ----"));
  for(const auto& Pair : Classes) UE_LOG(LogTemp,Display,TEXT("  %s  x%d"),*Pair.Key,Pair.Value);
  return 0;
 }

 /** The empty-state text in the Mail tab: the thing the save page's "not available" must match. */
 static int32 MailEmptyStyle()
 {
  const TCHAR* Paths[]={
   TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/InTabWidget/WB_TabWidget_Mail"),
   TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/WB_TabSwitcherMenu")};
  for(const TCHAR* Path : Paths)
  {
   auto* BP=Cast<UWidgetBlueprint>(LoadObject<UObject>(nullptr,Path));
   if(!BP){UE_LOG(LogTemp,Display,TEXT("MISSING %s"),Path);continue;}
   UE_LOG(LogTemp,Display,TEXT("WIDGET %s"),Path);
   BP->WidgetTree->ForEachWidget([](UWidget* W)
   {
    auto* Text=Cast<UTextBlock>(W);
    if(!Text)return;
    const FSlateFontInfo Font=Text->GetFont();
    FString Slot;
    if(auto* Canvas=Cast<UCanvasPanelSlot>(Text->Slot))
    {
     const FAnchorData L=Canvas->GetLayout();
     Slot=FString::Printf(TEXT("canvas anchors=(%.2f,%.2f)-(%.2f,%.2f) offsets=(%.1f,%.1f,%.1f,%.1f) align=(%.2f,%.2f)"),
      L.Anchors.Minimum.X,L.Anchors.Minimum.Y,L.Anchors.Maximum.X,L.Anchors.Maximum.Y,
      L.Offsets.Left,L.Offsets.Top,L.Offsets.Right,L.Offsets.Bottom,L.Alignment.X,L.Alignment.Y);
    }
    else if(auto* Overlay=Cast<UOverlaySlot>(W->Slot)) Slot=FString::Printf(TEXT("overlay h=%d v=%d padding=(%.1f,%.1f,%.1f,%.1f)"),int32(Overlay->HorizontalAlignment),int32(Overlay->VerticalAlignment),Overlay->Padding.Left,Overlay->Padding.Top,Overlay->Padding.Right,Overlay->Padding.Bottom);
    else if(W->Slot) Slot=W->Slot->GetClass()->GetName();
    UE_LOG(LogTemp,Display,TEXT("  TEXT %s text=\"%s\" font=%s typeface=%s size=%.1f visibility=%d %s"),
     *W->GetName(),*Text->GetText().ToString().Replace(TEXT("\n"),TEXT(" ")),
     *GetPathNameSafe(Font.FontObject),*Font.TypefaceFontName.ToString(),Font.Size,
     int32(W->GetVisibility()),*Slot);
   });
  }
  return 0;
 }

 /** Who declares, reads and writes "IsMedicated?" / "Leashed?" today. */
 static int32 LeashedWiring()
 {
  const TCHAR* Assets[]={
   TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"),
   TEXT("/Game/ThirdPerson/GameInstance/BP_VTG_GameInstance"),
   TEXT("/Game/LevelNSequence/BPLM"),
   TEXT("/Game/LevelNSequence/In_Level/SaApartment/BPLM_SaApartment")};
  const TSet<FName> Interesting={TEXT("IsMedicated?"),TEXT("Leashed?")};
  for(const TCHAR* Path : Assets)
  {
   auto* BP=LoadObject<UBlueprint>(nullptr,Path);
   if(!BP){UE_LOG(LogTemp,Display,TEXT("MISSING %s"),Path);continue;}
   UE_LOG(LogTemp,Display,TEXT("ASSET %s"),Path);
   for(const FBPVariableDescription& V : BP->NewVariables)
    if(Interesting.Contains(V.VarName))
     UE_LOG(LogTemp,Display,TEXT("  DECLARES %s (%s)"),*V.VarName.ToString(),*V.VarType.PinCategory.ToString());
   TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
   for(UEdGraph* G : Graphs)
    for(UEdGraphNode* N : G->Nodes)
     if(auto* Var=Cast<UK2Node_Variable>(N))
     {
      const FName Member=Var->VariableReference.GetMemberName();
      if(!Interesting.Contains(Member)) continue;
      UE_LOG(LogTemp,Display,TEXT("  %s %s in graph %s (node %s, scope %s)"),
       N->IsA<UK2Node_VariableSet>()?TEXT("SET"):TEXT("GET"),*Member.ToString(),*G->GetName(),*N->GetName(),
       *GetPathNameSafe(Var->VariableReference.GetMemberParentClass()));
     }
  }
  return 0;
 }
};
