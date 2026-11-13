#include "UI/VTGMapLocationNode.h"
#include "Components/CanvasPanel.h"
#include "Components/SizeBox.h"
#include "Engine/Font.h"

struct FVTGMapPresentationRegression
{
 static void Run(UWidgetBlueprint* BP)
 {
  FEditorScriptExecutionGuard Guard;
  if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
  const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
  auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("MapPresentationTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* Dummy=GI->GetWorld();GI->GetWorldContext()->SetCurrentWorld(W);W->SetGameInstance(GI);Dummy->DestroyWorld(false);
  auto* PC=W->SpawnActor<APlayerController>();auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,FPlatformUserId::CreateFromInternalId(0));PC->SetPlayer(Local);auto* Map=CreateWidget<UUserWidget>(PC,BP->GeneratedClass.Get());check(Map);auto Slate=Map->TakeWidget();
  TArray<UVTGMapLocationNode*> Nodes;Map->WidgetTree->ForEachWidget([&](UWidget* Widget){if(auto* Node=Cast<UVTGMapLocationNode>(Widget))Nodes.Add(Node);});check(Nodes.Num()==4);
  for(auto* Node:Nodes)
  {
   check(!Node->IsNodeSelected());check(Node->LocationNameButton && Node->LocationNameButton->GetVisibility()==ESlateVisibility::Collapsed);
   check(Node->GetWidgetFromName(TEXT("Txt_NodeName"))->GetVisibility()==ESlateVisibility::Collapsed);
   check(!CastChecked<UButton>(Node->GetWidgetFromName(TEXT("Btn_NodeInteractor")))->GetToolTip());
  }
  auto Click=[](UVTGMapLocationNode* Node){auto* B=CastChecked<UButton>(Node->GetWidgetFromName(TEXT("Btn_NodeInteractor")));B->OnPressed.Broadcast();B->OnClicked.Broadcast();};
  for(int I=0;I<Nodes.Num();++I)
  {
   auto* Node=Nodes[I];Node->LocationDescription=FText::FromString(FString::Printf(TEXT("Custom location introduction %d"),I));
   Click(Node);check(Node->IsNodeSelected() && Node->LocationNameButton->IsVisible());check(Node->LocationNameLabel->GetText().EqualTo(Node->LocationDisplayName));
   if(I==0){check(!Node->ShowDescription);Node->LocationNameButton->OnClicked.Broadcast();}
   check(Node->DescriptionPanel && Node->DescriptionPanel->IsVisible() && Node->DescriptionLabel->GetText().EqualTo(Node->LocationDescription));
   check(GetNameSafe(Node->DescriptionLabel->GetFont().FontObject)==TEXT("NotoSansSC-Regular_Font"));
   for(auto* Other:Nodes)if(Other!=Node){check(!Other->IsNodeSelected() && !Other->LocationNameButton->IsVisible());check(!Other->DescriptionPanel || !Other->DescriptionPanel->IsVisible());}
   check(Node->GetWidgetFromName(TEXT("VB_SubAreaList"))->GetVisibility()==ESlateVisibility::Collapsed);
  }
  Click(Nodes.Last());check(!Nodes.Last()->IsNodeSelected() && !Nodes.Last()->DescriptionPanel->IsVisible());
  GI->RemoveLocalPlayer(Local);GI->Shutdown();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
  UE_LOG(LogTemp,Display,TEXT("MAP PRESENTATION PASS: unselected captions hidden, selected names in badges, name click opens custom Noto description, selection replaces prior panel, no tooltip/subarea placeholder"));
 }
};

int32 RepairMapPresentation(bool Verify)
{
 auto* NodeBP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/InTabWidget/MapWBP/WBP_MapNode"));auto* MapBP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/InTabWidget/MapWBP/WBP_Map_1"));check(NodeBP && MapBP);
 UEdGraph* G=NodeBP->UbergraphPages[0];
 if(!Verify)
 {
  const FString File=FPackageName::LongPackageNameToFilename(NodeBP->GetOutermost()->GetName(),TEXT(".uasset"));const FString Backup=FPaths::ProjectSavedDir()/TEXT("QA/MapPresentationBefore/WBP_MapNode.uasset");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
  for(UEdGraphNode* N:G->Nodes)if(auto* Call=Cast<UK2Node_CallFunction>(N);Call && Call->FunctionReference.GetMemberName()==TEXT("SetVisibility"))
   if(auto* Target=Call->FindPin(TEXT("self"));Target && Target->LinkedTo.Num()==1)
    if(auto* Getter=Cast<UK2Node_VariableGet>(Target->LinkedTo[0]->GetOwningNode());Getter && (Getter->VariableReference.GetMemberName()==TEXT("Txt_NodeName") || Getter->VariableReference.GetMemberName()==TEXT("VB_SubAreaList")))
     GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Call->FindPinChecked(TEXT("InVisibility")),TEXT("Collapsed"));
  auto* End=AlleyFix::Node(G,TEXT("K2Node_CallFunction_14"))->FindPinChecked(TEXT("then"));
  if(End->LinkedTo.IsEmpty()){auto* Refresh=AlleyFix::Call(G,UVTGMapLocationNode::StaticClass()->FindFunctionByName(TEXT("RefreshLocationText")),End->GetOwningNode()->NodePosX+300,End->GetOwningNode()->NodePosY);Refresh->NodeComment=TEXT("Map selection: refresh selected badge and remove old description on reset");AlleyFix::Link(End,Refresh->FindPinChecked(TEXT("execute")));}
  check(AlleyFix::Save(NodeBP));
 }
 check(AlleyFix::Node(G,TEXT("K2Node_CallFunction_14"))->FindPinChecked(TEXT("then"))->LinkedTo.Num()==1);
 FVTGMapPresentationRegression::Run(MapBP);
 FVTGTerminalSaveRegression::Run();
 return 0;
}
