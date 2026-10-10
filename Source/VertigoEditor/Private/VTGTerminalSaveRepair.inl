#include "UI/VTGTerminalSaveLibrary.h"
#include "K2Node_Self.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "UI/VTGTerminalSavePage.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/SceneComponent.h"

#include "Save/VTGSaveCoordinator.h"
#include "Save/VTGSaveTypes.h"
#include "Kismet/GameplayStatics.h"
struct FVTGTerminalSaveRegression
{
 static void Run()
 {
  const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
  auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("TerminalSaveRegression"),nullptr,true,ERHIFeatureLevel::Num,&Init);
  auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(World);
  auto* GI=NewObject<UVTGGameInstanceBase>();World->SetGameInstance(GI);Context.OwningGameInstance=GI;
  auto* Save=NewObject<UVTGSaveCoordinator>(GI);
  auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(BP);
  FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Pawn=World->SpawnActor<APawn>(BP->GeneratedClass,FTransform(FRotator(0,37,0),FVector(100,200,300)),Params);
  auto* PC=World->SpawnActor<APlayerController>();PC->Possess(Pawn);
  if(!UGameplayStatics::GetPlayerController(World,0))World->AddController(PC);
  check(UGameplayStatics::GetPlayerPawn(World,0)==Pawn);
  auto* Health=FindFProperty<FNumericProperty>(Pawn->GetClass(),TEXT("Health"));check(Health&&Health->IsInteger());
  Health->SetIntPropertyValue(Health->ContainerPtrToValuePtr<void>(Pawn),int64(37));
  auto* Snapshot=NewObject<UVTGSaveGame>();Snapshot->Meta.DisplayLabel=TEXT("Terminal Save");Snapshot->Meta.SlotIndex=1;
  Snapshot->PersistentInts.Add(TEXT("Test.Progress"),3);Snapshot->PersistentNames.Add(TEXT("Test.Stage"),TEXT("ReadMail"));
  Save->GatherWorldState(World,Snapshot);
  UE_LOG(LogTemp,Display,TEXT("SNAPSHOT health=%d value=%f transform=%d inventory=%d"),Snapshot->bHasTerminalHealth,Snapshot->TerminalHealth,Snapshot->bHasPlayerTransform,Snapshot->bHasPlayerInventory);
  check(Snapshot->bHasTerminalHealth&&Snapshot->TerminalHealth==37.0&&Snapshot->bHasPlayerTransform&&Snapshot->bHasPlayerInventory);
  TArray<uint8> Bytes;check(UGameplayStatics::SaveGameToMemory(Snapshot,Bytes));
  auto* Loaded=CastChecked<UVTGSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
  check(Loaded->PersistentInts.FindChecked(TEXT("Test.Progress"))==3 && Loaded->PersistentNames.FindChecked(TEXT("Test.Stage"))==FName(TEXT("ReadMail")));
  check(Loaded->TerminalHealth==37.0&&Loaded->PlayerTransform.Equals(Pawn->GetActorTransform()));
  // Components have not begun play in this isolated world; validate their captured payload above,
  // then exercise position/health restore without invoking inventory or equipment gameplay.
  Loaded->bHasPlayerInventory=false;Loaded->ActorRecords.Empty();
  Health->SetIntPropertyValue(Health->ContainerPtrToValuePtr<void>(Pawn),int64(1));Pawn->SetActorLocation(FVector::ZeroVector);
  Save->ApplyWorldState(World,Loaded);
  check(Health->GetSignedIntPropertyValue(Health->ContainerPtrToValuePtr<void>(Pawn))==37);
  check(Pawn->GetActorTransform().Equals(Snapshot->PlayerTransform));
  Snapshot->Meta.DisplayLabel=TEXT("Autosave");Snapshot->Meta.SlotIndex=0;Snapshot->bHasTerminalHealth=false;Save->GatherWorldState(World,Snapshot);check(!Snapshot->bHasTerminalHealth);
    if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
  auto* TerminalBP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/WB_TabSwitcher"));
  auto* Terminal=CreateWidget<UUserWidget>(World,TerminalBP->GeneratedClass.Get());check(Terminal);
  auto* Switcher=CastChecked<UWidgetSwitcher>(Terminal->GetWidgetFromName(TEXT("WidgetSwitcher_132")));
  const int32 OriginalPageCount=Switcher->GetChildrenCount();
  auto* Existing=Switcher->GetChildAt(0);check(Existing);
  check(UVTGTerminalSaveLibrary::HandleTerminalSave(Terminal,254));
  auto* Page=Cast<UVTGTerminalSavePage>(Switcher->GetActiveWidget());check(Page);Page->TakeWidget();
  auto* Button=Cast<UButton>(Page->GetWidgetFromName(TEXT("SaveGameButton")));
  auto* Status=Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("SaveStatus")));
  check(Button&&Button->OnClicked.IsBound()&&Status);
  check(Status->GetText().ToString()==TEXT("Select a slot to save or load."));
  Switcher->SetActiveWidget(Existing);check(UVTGTerminalSaveLibrary::HandleTerminalSave(Terminal,254));
  check(Switcher->GetChildrenCount()==OriginalPageCount+1&&Switcher->GetActiveWidget()==Page);
  check(Status->GetText().ToString()==TEXT("Select a slot to save or load."));
  // No subsystem is initialized in this isolated UI world: a click should report unavailable,
  // proving the button is wired while avoiding writes to any user save file.
  Button->OnClicked.Broadcast();check(Status->GetText().ToString()==TEXT("Saving is unavailable."));
      auto* TerminalFrame=Cast<UImage>(Page->GetWidgetFromName(TEXT("TerminalFrame")));check(TerminalFrame&&TerminalFrame->GetBrush().GetResourceObject());
  check(TerminalFrame->GetBrush().GetResourceObject()->GetName()==TEXT("terminal_BG_Image_copy"));
  auto* Prop=FindFProperty<FObjectPropertyBase>(Pawn->GetClass(),TEXT("Terminal"));check(Prop);
  auto* PhysicalTerminal=CastChecked<USceneComponent>(Prop->GetObjectPropertyValue_InContainer(Pawn));PhysicalTerminal->SetVisibility(false);
  Page->NativeTick(FGeometry(),.016f);check(Page->GetVisibility()!=ESlateVisibility::Collapsed);
  UE_LOG(LogTemp,Display,TEXT("TERMINAL FRAME PASS: original frame texture present; hidden 3D prop does not hide 2D page."));
  auto* Screen=Page->GetWidgetFromName(TEXT("TerminalScreen"));check(Screen&&Screen->GetClipping()==EWidgetClipping::ClipToBoundsAlways);
  auto* ScreenSlot=CastChecked<UCanvasPanelSlot>(Screen->Slot);const auto Layout=ScreenSlot->GetLayout();
  check(FMath::IsNearlyEqual(Layout.Offsets.Right,1152.f)&&FMath::IsNearlyEqual(Layout.Offsets.Left,-580.f));
  Page->SelectSlot(0);check(!Page->SaveButton->GetIsEnabled()&&!Page->LoadButton->GetIsEnabled());
  Page->SelectSlot(1);check(Page->SaveButton->GetIsEnabled()&&!Page->LoadButton->GetIsEnabled());
  Page->Metas[1].bIsValid=true;Page->UpdateSelection();Page->SaveButton->OnClicked.Broadcast();check(Page->PendingAction==1);
  Page->CancelButton->OnClicked.Broadcast();check(Page->PendingAction==0);
  Page->LoadButton->OnClicked.Broadcast();check(Page->PendingAction==2);
  Terminal->SetVisibility(ESlateVisibility::Collapsed);check(Page->GetVisibility()==ESlateVisibility::Collapsed&&Page->PendingAction==0);
  Terminal->SetVisibility(ESlateVisibility::SelfHitTestInvisible);UVTGTerminalSaveLibrary::HandleTerminalSave(Terminal,254);
  check(Page->GetVisibility()!=ESlateVisibility::Collapsed&&Switcher->GetChildrenCount()==OriginalPageCount+1);
  UE_LOG(LogTemp,Display,TEXT("TERMINAL SLOTS PASS: authored screen bounds and clipping, automatic slot protected, empty load disabled, overwrite/load confirmations, terminal close hides page and cancels pending actions."));
  UE_LOG(LogTemp,Display,TEXT("TERMINAL PAGE PASS: selecting/reselecting only opens one page, explicit button is bound, only clicking changes save status."));
    FString PreviewFile;
  if(FParse::Value(FCommandLine::Get(),TEXT("PreviewFile="),PreviewFile)){
   FWidgetRenderer Renderer(true);
   auto Slate=Page->TakeWidget();auto* Target=Renderer.DrawWidget(Slate,FVector2D(1920,1080));check(Target);FlushRenderingCommands();
   TArray<FColor> Pixels;check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));
   TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1920,1080,Pixels,PNG);check(FFileHelper::SaveArrayToFile(PNG,*PreviewFile));
   UE_LOG(LogTemp,Display,TEXT("TERMINAL PREVIEW saved: %s"),*PreviewFile);
  }
  World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
  UE_LOG(LogTemp,Display,TEXT("TERMINAL STATE PASS: real player position, current health, inventory capture and progress flags serialize; health/position restore; checkpoint behavior unchanged. No user save files touched."));
 }
};
namespace TerminalPatch
{
const TCHAR* Marker=TEXT("VTG terminal save");
void Insert(UBlueprint* BP,const TCHAR* EventName,bool Prepare)
{
 UEdGraph* G=BP->UbergraphPages[0];
 for(UEdGraphNode* N:G->Nodes)if(N->NodeComment==Marker)return;
 auto* Event=AlleyFix::Node(G,EventName);
 auto* Start=AlleyFix::Pin(Event,TEXT("then"));check(Start->LinkedTo.Num()==1);
 auto* Old=Start->LinkedTo[0];Start->BreakAllPinLinks();
 auto* Call=AlleyFix::Call(G,UVTGTerminalSaveLibrary::StaticClass()->FindFunctionByName(Prepare?TEXT("PrepareTerminalTabs"):TEXT("HandleTerminalSave")),Event->NodePosX+250,Event->NodePosY-250);
 Call->NodeComment=Marker;
 auto* Self=AlleyFix::Add<UK2Node_Self>(G,Call->NodePosX-120,Call->NodePosY+150);Self->AllocateDefaultPins();
 AlleyFix::Link(AlleyFix::Pin(Self,TEXT("self")),AlleyFix::Pin(Call,Prepare?TEXT("Menu"):TEXT("Terminal")));
 AlleyFix::Link(Start,AlleyFix::Pin(Call,TEXT("execute")));
 if(Prepare)AlleyFix::Link(AlleyFix::Pin(Call,TEXT("then")),Old);
 else {
  AlleyFix::Link(AlleyFix::Pin(Event,TEXT("Index")),AlleyFix::Pin(Call,TEXT("Index")));
  auto* Branch=AlleyFix::Add<UK2Node_IfThenElse>(G,Call->NodePosX+400,Call->NodePosY);Branch->AllocateDefaultPins();
  AlleyFix::Link(AlleyFix::Pin(Call,TEXT("then")),AlleyFix::Pin(Branch,TEXT("execute")));
  AlleyFix::Link(AlleyFix::Pin(Call,TEXT("ReturnValue")),AlleyFix::Pin(Branch,TEXT("Condition")));
  AlleyFix::Link(AlleyFix::Pin(Branch,TEXT("else")),Old);
 }
}
int32 Run(bool VerifyOnly)
{
 auto* BP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/WB_TabSwitcher"));
 auto* Menu=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/WB_TabSwitcherMenu"));
 check(BP&&Menu);
 if(!VerifyOnly){
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("QA/TerminalSaveBefore");IFileManager::Get().MakeDirectory(*Dir,true);
  for(auto* Asset:{BP,Menu}){
   const FString File=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),TEXT(".uasset"));
   const FString Backup=Dir/(Asset->GetName()+TEXT(".uasset"));
   if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
  }
  Insert(Menu,TEXT("K2Node_Event_4"),true);
  Insert(BP,TEXT("K2Node_CustomEvent_2"),false);
  if(!AlleyFix::Save(Menu)||!AlleyFix::Save(BP))return 1;
 }
 auto* Template=CastChecked<UUserWidget>(BP->WidgetTree->FindWidget(TEXT("WB_TabSwitcher")));
 auto* Instance=DuplicateObject<UUserWidget>(Template,GetTransientPackage());
 auto* Prop=FindFProperty<FArrayProperty>(Instance->GetClass(),TEXT("Tabs"));check(Prop);
 UScriptStruct* Struct=CastFieldChecked<FStructProperty>(Prop->Inner)->Struct;
 FBoolProperty* Inv=nullptr;FByteProperty* Index=nullptr;
 for(TFieldIterator<FProperty>P(Struct);P;++P){
  if(P->GetName().StartsWith(TEXT("ShowInventory_")))Inv=CastField<FBoolProperty>(*P);
  if(P->GetName().StartsWith(TEXT("WidgetIndex_")))Index=CastField<FByteProperty>(*P);
 }
 check(Inv&&Index);
 auto Validate=[&](){
  FScriptArrayHelper A(Prop,Prop->ContainerPtrToValuePtr<void>(Instance));
  TSet<uint8> Indices;
  for(int32 I=0;I<A.Num();++I){check(!Inv->GetPropertyValue_InContainer(A.GetRawPtr(I)));Indices.Add(Index->GetPropertyValue_InContainer(A.GetRawPtr(I)));}
  check(A.Num()==5 && Indices.Num()==5 && Indices.Contains(0)&&Indices.Contains(1)&&Indices.Contains(2)&&Indices.Contains(3)&&Indices.Contains(254));
 };
 UVTGTerminalSaveLibrary::PrepareTerminalTabs(Instance);Validate();
 UVTGTerminalSaveLibrary::PrepareTerminalTabs(Instance);Validate();
 check(!UVTGTerminalSaveLibrary::HandleTerminalSave(nullptr,3));
 check(UVTGTerminalSaveLibrary::HandleTerminalSave(nullptr,254));
 for(auto* Asset:{BP,Menu}){
  FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(Asset,EBlueprintCompileOptions::None,&Log);check(Log.NumErrors==0);
  bool Hook=false;for(UEdGraphNode* N:Asset->UbergraphPages[0]->Nodes)if(N->NodeComment==Marker)Hook=true;check(Hook);
 }
 FVTGTerminalSaveRegression::Run();
 UE_LOG(LogTemp,Display,TEXT("TERMINAL SAVE PASS: ITEM hidden, authored tabs preserved, exactly one SAVE after repeated construction, save action consumed, both blueprints compile."));
 return 0;
}
}
