#include "UI/VTGSaveIndicator.h"
#include "Components/CircularThrobber.h"
#include "VTGSaveKeyWatcher.h"
#include "Combat/VTGCombatComponent.h"
#include "VTGLevelManagerBase.h"
#include "Components/VerticalBox.h"
#include "MovieScene.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "LevelSequence.h"
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
#include "UI/VTGMainMenu.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/SceneComponent.h"
#include "Engine/LocalPlayer.h"
#include "UI/VTGTerminalWidget.h"
#include "Components/BackgroundBlur.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"

#include "Save/VTGSaveCoordinator.h"
#include "Save/VTGSaveTypes.h"
#include "Kismet/GameplayStatics.h"
struct FVTGTerminalSaveRegression
{

 /**
  * Main menu Continue opens the SAME page in load-only mode. Two things can silently break this and
  * neither shows up as a compile error: renaming the Continue button in WBP_MainMenu (the binding is
  * by name) and reparenting the widget away from UVTGMainMenu. Assert both, plus the load-only shape.
  */

 /**
  * The save gate. These are the states where a save would be wrong rather than merely inconvenient:
  * a fight in progress (the encounter is not in the save), and a cutscene (a staging transform and
  * story gates that started but never finished). Asserted here because this is the only harness with
  * a real pawn and player controller.
  */

 /**
  * Destruction has to survive a load for every map-placed actor, not only the two that implement
  * IVTGSaveable. 8 of the 14 BPLMs destroy ISX collisions, triggers, restrict volumes and NPCs; all of
  * those used to come straight back while the BPLM flag saying "already done" came back true.
  */

 /**
  * The save-key watcher's decision. Renaming in the Outliner is harmless (SetActorLabel never renames
  * the object), so what this must catch is deleting an actor whose NAME a save refers to.
  */

 /** The bottom-right saving spinner: where it sits, and that it takes itself away again. */
 static void SaveIndicator(APlayerController* PC)
 {
  auto* Widget=CreateWidget<UVTGSaveIndicator>(PC,UVTGSaveIndicator::StaticClass());check(Widget);
  Widget->TakeWidget(); // forces RebuildWidget
  check(Widget->Spinner && Widget->Label);
  check(Widget->Label->GetFont().Size==17.f);
  check(GetPathNameSafe(Widget->Label->GetFont().FontObject).Contains(TEXT("NotoSansSC-Medium")));
  auto* Slot=Cast<UCanvasPanelSlot>(Widget->Spinner->GetParent()->Slot);check(Slot);
  const FAnchorData Layout=Slot->GetLayout();
  check(Layout.Anchors.Minimum.Equals(FVector2D(1.f,1.f)) && Layout.Alignment.Equals(FVector2D(1.f,1.f))); // bottom right
  check(Layout.Offsets.Left<0.f && Layout.Offsets.Top<0.f);                                                // inset from the corner

  Widget->Begin();
  check(Widget->GetVisibility()!=ESlateVisibility::Collapsed);
  // Finishing does not hide it immediately: the minimum visible time is what makes it readable.
  Widget->Finish();
  Widget->NativeTick(FGeometry(),0.1f);
  check(Widget->GetVisibility()!=ESlateVisibility::Collapsed);
  Widget->NativeTick(FGeometry(),Widget->MinimumVisibleTime);
  check(Widget->GetVisibility()==ESlateVisibility::Collapsed);
  UE_LOG(LogTemp,Display,TEXT("SAVE INDICATOR PASS: spinner and label build bottom-right inset from the corner; Finish waits out the minimum visible time before it hides itself."));
 }

 static void SaveKeyWatcher(UWorld* World)
 {
  // The watcher only speaks up in an editor world - a play session destroys actors constantly and
  // none of it touches disk - and this harness builds a Game world. Without borrowing the type for
  // the length of this function every assertion below passes vacuously.
  TGuardValue<TEnumAsByte<EWorldType::Type>> AsEditor(World->WorldType,EWorldType::Editor);

  // A level manager with no explicit id is keyed by its actor name, so deleting it matters.
  auto* Bare=World->SpawnActor<AVTGLevelManagerBase>();check(Bare);
  check(Bare->LevelManagerSaveId.IsNone());
  check(!UVTGSaveKeyWatcher::DescribeSaveKeyUse(Bare).IsEmpty());

  // ---- but not when the Blueprint compiler is the one doing the deleting ------------------------
  // Compiling a BPLM destroys and respawns every placed instance, and that arrives on the same
  // delegate as a real deletion. It used to warn on every compile of the Blueprint being worked on.
  {
   TGuardValue<bool> Compiling(GCompilingBlueprint,true);
   check(UVTGSaveKeyWatcher::DescribeSaveKeyUse(Bare).IsEmpty());
  }
  {
   TGuardValue<bool> Reinstancing(GIsReinstancing,true);
   check(UVTGSaveKeyWatcher::DescribeSaveKeyUse(Bare).IsEmpty());
  }
  check(!UVTGSaveKeyWatcher::DescribeSaveKeyUse(Bare).IsEmpty()); // and it speaks again afterwards
  {
   // The backstop, for the teardown that happens after the globals are back down: the trash object
   // the compiler leaves behind. This is the name the reported case actually showed.
   FActorSpawnParameters Trash;Trash.Name=TEXT("REINST_BPLM_Clinic_C_3");
   Trash.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   auto* Stale=World->SpawnActor<AVTGLevelManagerBase>(AVTGLevelManagerBase::StaticClass(),FTransform::Identity,Trash);
   check(Stale && UVTGSaveKeyWatcher::IsReinstancing(Stale));
   check(UVTGSaveKeyWatcher::DescribeSaveKeyUse(Stale).IsEmpty());
   Stale->Destroy();
  }

  // Give it one and it no longer depends on the name at all.
  Bare->LevelManagerSaveId=TEXT("Test.Manager");
  check(UVTGSaveKeyWatcher::DescribeSaveKeyUse(Bare).IsEmpty());

  // An ordinary actor nothing refers to is not a save key.
  FActorSpawnParameters Params;Params.Name=TEXT("VTG_SaveKeyProbe");
  Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Loose=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Params);check(Loose);
  check(UVTGSaveKeyWatcher::DescribeSaveKeyUse(Loose).IsEmpty());

  // ...but one a real BPLM holds a reference to is: that is how the destroy targets are reached, and
  // the destroyed-actor record is keyed by actor name.
  auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/L4BlackMarketEntrance/BPLM_BlackMarketEntrance"));check(BP);
  auto* Real=World->SpawnActor<AVTGLevelManagerBase>(BP->GeneratedClass);check(Real);
  FObjectPropertyBase* Slot=nullptr;
  for(TFieldIterator<FObjectPropertyBase> P(Real->GetClass());P;++P)
   if(P->GetOwnerClass() && P->GetOwnerClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint) && Loose->GetClass()->IsChildOf(P->PropertyClass))
   {Slot=*P;break;}
  check(Slot); // BPLM_BlackMarketEntrance keeps references to the ISX collisions it removes
  Slot->SetObjectPropertyValue_InContainer(Real,Loose);
  const FString Reason=UVTGSaveKeyWatcher::DescribeSaveKeyUse(Loose);
  check(!Reason.IsEmpty() && Reason.Contains(Slot->GetName()));
  Slot->SetObjectPropertyValue_InContainer(Real,nullptr);
  check(UVTGSaveKeyWatcher::DescribeSaveKeyUse(Loose).IsEmpty());

  Bare->Destroy();Real->Destroy();Loose->Destroy();
  UE_LOG(LogTemp,Display,TEXT("SAVE KEY WATCHER PASS: warns for a level manager with no LevelManagerSaveId and for an actor a level manager refers to; silent once an id is set and for an actor nothing refers to."));
 }

 static void DestructionTracking(UWorld* World,APawn* Pawn,UVTGSaveCoordinator* Save)
 {
  FActorSpawnParameters Params;Params.Name=TEXT("VTG_DestroyTrackingProbe");
  Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Probe=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Params);check(Probe);
  Probe->SetFlags(RF_WasLoaded); // stand in for "came with the map"
  const FName ProbeId=Save->ResolveSaveId(Probe);
  check(ProbeId==Probe->GetFName());                 // no interface, so the actor name is the key
  check(Save->ShouldTrackDestruction(Probe));
  check(!Save->ShouldTrackDestruction(Pawn));        // the player is respawned, never "stayed gone"
  auto* Manager=World->SpawnActor<AVTGLevelManagerBase>();check(Manager);
  check(!Save->ShouldTrackDestruction(Manager));     // the level's brain, and one destroys itself

  Save->DestroyedActorIds.Reset();
  Save->HandleActorDestroyed(Probe);
  check(Save->DestroyedActorIds.Contains(ProbeId));
  Save->HandleActorDestroyed(Pawn);
  Save->HandleActorDestroyed(Manager);
  check(Save->DestroyedActorIds.Num()==1);           // neither of those was recorded

  // Dead counts as destroyed. HandleDeath only sets CombatState, so without this a killed enemy stood
  // back up unhurt after a load - which is exactly the "dead vs alive" distinction the save must keep.
  FActorSpawnParameters Corpse;Corpse.Name=TEXT("VTG_DeadEnemyProbe");
  Corpse.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Enemy=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Corpse);check(Enemy);
  Enemy->SetFlags(RF_WasLoaded);
  auto* EnemyCombat=NewObject<UVTGCombatComponent>(Enemy);Enemy->AddInstanceComponent(EnemyCombat);EnemyCombat->RegisterComponent();
  auto* Snapshot=NewObject<UVTGSaveGame>();
  Save->DestroyedActorIds.Reset();
  Save->GatherWorldState(World,Snapshot);
  check(!Snapshot->DestroyedActors.Contains(Save->ResolveSaveId(Enemy)));   // alive: not recorded
  EnemyCombat->SetCombatState(EVTGCombatState::Dead);
  check(EnemyCombat->GetCombatState()==EVTGCombatState::Dead && !EnemyCombat->IsAlive());
  auto* AfterDeath=NewObject<UVTGSaveGame>();
  Save->GatherWorldState(World,AfterDeath);
  check(AfterDeath->DestroyedActors.Contains(Save->ResolveSaveId(Enemy)));  // dead: recorded
  check(!AfterDeath->DestroyedActors.Contains(Save->ResolveSaveId(Pawn)));  // never the player
  Enemy->Destroy();
  Save->DestroyedActorIds.Reset();

  // And a load removes the copy the map spawned, without needing the interface. A second probe,
  // because the first one is still alive here - HandleActorDestroyed only records, it does not destroy.
  FActorSpawnParameters Reloaded;Reloaded.Name=TEXT("VTG_DestroyTrackingReloaded");
  Reloaded.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Fresh=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Reloaded);check(Fresh);
  Fresh->SetFlags(RF_WasLoaded);
  auto* Data=NewObject<UVTGSaveGame>();
  Data->DestroyedActors.Add(Save->ResolveSaveId(Fresh));
  Save->ApplyWorldState(World,Data);
  check(Fresh->IsActorBeingDestroyed());
  check(!Probe->IsActorBeingDestroyed());           // not in the list, so it stays
  check(!Pawn->IsActorBeingDestroyed() && !Manager->IsActorBeingDestroyed());
  Probe->Destroy();
  Save->DestroyedActorIds.Reset();
  Manager->Destroy();
  UE_LOG(LogTemp,Display,TEXT("DESTRUCTION TRACKING PASS: a map-placed actor with no interface is recorded and stays destroyed across a load; the player pawn and level managers are never recorded; a dead combat actor is recorded as destroyed and a live one is not."));
 }

 static void SaveGate(UWorld* World,APawn* Pawn,UVTGSaveCoordinator* Save)
 {
  FText Reason;
  auto* Combat=FindFProperty<FBoolProperty>(Pawn->GetClass(),TEXT("InCombat"));check(Combat);
  Combat->SetPropertyValue_InContainer(Pawn,false);
  check(!Save->IsPlayerInCombat(World));
  check(Save->CanSaveNow(World,Reason) && Reason.IsEmpty());

  // In a fight: refused, and the reason is the line the terminal shows.
  Combat->SetPropertyValue_InContainer(Pawn,true);
  check(Save->IsPlayerInCombat(World));
  check(!Save->CanSaveNow(World,Reason));
  check(Reason.ToString().Contains(TEXT("conflict")));
  // SaveTerminalSlot refuses as well, but in this harness it refuses earlier - the GameInstance has no
  // world context, so it never reaches the combat test. Assert only that it refuses, and keep the
  // wording assertion on CanSaveNow above, which is the one the page actually shows.
  check(!Save->SaveTerminalSlot(1,Reason));
  Combat->SetPropertyValue_InContainer(Pawn,false);

  // ---- a story section holding the save button closed --------------------------------------------
  // The alley case: a stretch whose scene cannot be rebuilt from an arbitrary moment, so the player
  // is not allowed to pick one. What must stay true is that it closes and reopens on demand, that it
  // is runtime-only, and - the whole reason it is safe - that the combat checkpoint ignores it.
  {
   auto* Manager=World->SpawnActor<AVTGLevelManagerBase>();check(Manager);
   check(Save->CanSaveNow(World,Reason));
   Manager->SetManualSaveBlocked(true);
   check(Manager->IsManualSaveBlocked());
   check(!Save->CanSaveNow(World,Reason));
   check(Reason.ToString().Contains(TEXT("sequence")));

   // The block is not progress: a save written here must not carry it, or loading that save would
   // come back with the terminal dead and no way to reopen it.
   FVTGLevelProgressRecord Record;
   Manager->CaptureLevelProgress(Record);
   for (const auto& Pair : Record.Values) check(Pair.Key != TEXT("bManualSaveBlocked"));
   auto* Fresh=World->SpawnActor<AVTGLevelManagerBase>();check(Fresh);
   Fresh->RestoreLevelProgress(Record);
   check(!Fresh->IsManualSaveBlocked());   // a fresh level entry always starts open
   Fresh->Destroy();

   Manager->SetManualSaveBlocked(false);
   check(Save->CanSaveNow(World,Reason));
   Manager->Destroy();
  }

  // A playing level sequence: refused too, with its own reason.
  auto* SequenceActor=World->SpawnActor<ALevelSequenceActor>();check(SequenceActor);
  auto* Sequence=NewObject<ULevelSequence>(SequenceActor,TEXT("SaveGateSequence"));Sequence->Initialize();
  Sequence->GetMovieScene()->SetPlaybackRange(0,1000);
  SequenceActor->SetSequence(Sequence);
  SequenceActor->InitializePlayer();
  if(auto* Player=SequenceActor->GetSequencePlayer())
  {
   Player->Play();
   if(Player->IsPlaying())
   {
    check(!Save->CanSaveNow(World,Reason));
    check(Reason.ToString().Contains(TEXT("cutscene")));
    Player->Stop();
   }
   check(Save->CanSaveNow(World,Reason));
  }
  SequenceActor->Destroy();

  // The player must never be able to write a reserved slot, fight or no fight.
  check(!Save->SaveTerminalSlot(Save->GetAutoSaveSlot(),Reason));
  check(!Save->SaveTerminalSlot(Save->GetStoryAutoSaveSlot(),Reason));
  UE_LOG(LogTemp,Display,TEXT("SAVE GATE PASS: allowed when quiet; refused in combat with the terminal's own wording, refused during a playing level sequence, refused while a level manager holds a story section closed - and that block is runtime-only, so it never rides a save back into a dead terminal; reserved slots refused either way."));
 }

 static void MainMenuPage(APlayerController* PC)
 {
  auto* MenuBP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/UI/MainMenu/WBP_MainMenu"));check(MenuBP);
  UClass* MenuClass=MenuBP->GeneratedClass.Get();check(MenuClass);
  check(MenuClass->IsChildOf(UVTGMainMenu::StaticClass())); // reparented away from the C++ menu
  auto* Menu=CreateWidget<UUserWidget>(PC,MenuClass);check(Menu);
  Menu->TakeWidget();
  check(Cast<UButton>(Menu->GetWidgetFromName(TEXT("Button")))); // Continue button renamed

  auto* Page=CreateWidget<UVTGTerminalSavePage>(PC,UVTGTerminalSavePage::StaticClass(),TEXT("MainMenuSavePageTest"));check(Page);
  Page->TakeWidget(); // forces RebuildWidget, which is where the buttons are constructed
  check(Page->SaveButton && Page->LoadButton);
  // The "not available" line must keep matching WB_TabWidget_Mail's "NoMessage": same font, same
  // size, centred. If someone restyles the Mail empty state, this is where the drift shows up.
  check(Page->Unavailable && Page->PageBody);
  check(Page->Unavailable->GetFont().Size==25.f);
  check(GetPathNameSafe(Page->Unavailable->GetFont().FontObject).Contains(TEXT("NotoSansSC-Medium")));
  check(Page->Unavailable->GetVisibility()==ESlateVisibility::Collapsed); // only a fight shows it
  check(Page->PageBody->GetVisibility()!=ESlateVisibility::Collapsed);
  check(Page->SaveButton->GetVisibility()!=ESlateVisibility::Collapsed); // terminal mode shows SAVE

  Page->OpenForMainMenu(Menu);
  check(Page->LoadOnly);
  check(Page->SaveButton->GetVisibility()==ESlateVisibility::Collapsed);
  check(Page->GetVisibility()!=ESlateVisibility::Collapsed);

  // CLOSE must hand the screen back to the menu, not try to close a terminal that is not there.
  Menu->SetVisibility(ESlateVisibility::Collapsed);
  Page->CloseClicked();
  check(Page->GetVisibility()==ESlateVisibility::Collapsed);
  check(Menu->GetVisibility()!=ESlateVisibility::Collapsed);
  UE_LOG(LogTemp,Display,TEXT("MAIN MENU SAVE PAGE PASS: WBP_MainMenu still derives from UVTGMainMenu and keeps its \"Button\"; page opens load-only with SAVE hidden; the unavailable line matches the Mail empty state; CLOSE returns the menu."));
 }

 static void Run()
 {
  if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
  const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
  auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("TerminalSaveRegression"),nullptr,true,ERHIFeatureLevel::Num,&Init);
  auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(World);
  auto* GI=NewObject<UVTGGameInstanceBase>();World->SetGameInstance(GI);Context.OwningGameInstance=GI;
  auto* Save=NewObject<UVTGSaveCoordinator>(GI);
  auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(BP);
  FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Pawn=World->SpawnActor<APawn>(BP->GeneratedClass,FTransform(FRotator(0,37,0),FVector(100,200,300)),Params);
  auto* PC=World->SpawnActor<APlayerController>(LoadClass<APlayerController>(nullptr,TEXT("/Game/ThirdPerson/PC_Game_VTG.PC_Game_VTG_C")));check(PC);PC->Possess(Pawn);
  auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,FPlatformUserId::CreateFromInternalId(0));PC->SetPlayer(Local);
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
  auto* Terminal=CreateWidget<UUserWidget>(PC,TerminalBP->GeneratedClass.Get());check(Terminal);
  const double OriginalTime=World->TimeSeconds,OriginalRealTime=World->RealTimeSeconds;
  World->TimeSeconds=1.499;check(!UVTGTerminalSaveLibrary::TryAcceptTerminalInput(Pawn));
  World->TimeSeconds=1.5;World->RealTimeSeconds=1.5;check(UVTGTerminalSaveLibrary::TryAcceptTerminalInput(Pawn));
  auto* SafeTerminal=Cast<UVTGTerminalWidget>(Terminal);
  if(SafeTerminal)
  {
   const FKeyEvent Repeat(EKeys::Tab,FModifierKeysState(),0,true,0,0);
   check(SafeTerminal->NativeOnPreviewKeyDown(FGeometry(),Repeat).IsEventHandled());
  }
  for(int I=0;I<24;++I){World->RealTimeSeconds=1.5+.01*I;check(!UVTGTerminalSaveLibrary::TryAcceptTerminalInput(Pawn));}
  World->RealTimeSeconds=1.75;check(UVTGTerminalSaveLibrary::TryAcceptTerminalInput(Pawn));
  for(int I=0;I<100;++I){check(!UVTGTerminalSaveLibrary::TryAcceptTerminalInput(Pawn));World->RealTimeSeconds+=.25;check(UVTGTerminalSaveLibrary::TryAcceptTerminalInput(Pawn));}
  World->TimeSeconds=OriginalTime;World->RealTimeSeconds=OriginalRealTime;
  check(UVTGTerminalSaveLibrary::ResolveTerminalTabIndex(0)==1 && UVTGTerminalSaveLibrary::ResolveTerminalTabIndex(3)==3);
  UE_LOG(LogTemp,Display,TEXT("TERMINAL INPUT PASS: 1.499s rejected, 1.5s accepted, 250ms shared cooldown, 100 rapid cycles, repeated Tab consumed, Document redirected"));
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
  auto* Shared=CastChecked<UUserWidget>(Page->GetWidgetFromName(TEXT("TerminalSharedBackground")));
  Shared->TakeWidget();
  auto* TerminalFrame=Cast<UImage>(Shared->GetWidgetFromName(TEXT("Terminal")));check(TerminalFrame&&TerminalFrame->GetBrush().GetResourceObject());
  auto* Blur=CastChecked<UBackgroundBlur>(Shared->GetWidgetFromName(TEXT("BackgroundBlur_92")));check(Blur->GetBlurStrength()>0);
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
  // Exercise input mode against an actual scene viewport, rather than only
  // asserting gate timers. This catches a visible terminal left in Game mode.
  auto* Viewport=NewObject<UGameViewportClient>(GEngine);Context.GameViewport=Viewport;Local->ViewportClient=Viewport;
  auto ViewportWidget=SNew(SViewport);auto SceneViewport=MakeUnique<FSceneViewport>(Viewport,ViewportWidget);Viewport->Viewport=SceneViewport.Get();
  UActorComponent* HUD=nullptr;FObjectPropertyBase* TabProperty=nullptr;
  for(auto* Component:Pawn->GetComponents())if(auto* P=FindFProperty<FObjectPropertyBase>(Component->GetClass(),TEXT("TabSwitchWidget"))){HUD=Component;TabProperty=P;break;}
  check(HUD && TabProperty && SafeTerminal);TabProperty->SetObjectPropertyValue_InContainer(HUD,Terminal);
  auto TerminalSlate=Terminal->TakeWidget();
  for(int Cycle=0;Cycle<100;++Cycle)
  {
   Terminal->SetVisibility(ESlateVisibility::Visible);Terminal->SetOwningPlayer(nullptr);
   UVTGTerminalSaveLibrary::SynchronizeTerminalInput(Pawn);
   if(Cycle==0)UE_LOG(LogTemp,Display,TEXT("TERMINAL FOCUS STATE owner=%d owns=%d cursor=%d ignore=%d mode=%s local=%d"),Terminal->GetOwningPlayer()==PC,SafeTerminal->OwnsInput,PC->bShowMouseCursor,Viewport->IgnoreInput(),*PC->GetCurrentInputModeDebugString(),PC->IsLocalController());
   check(Terminal->GetOwningPlayer()==PC && SafeTerminal->OwnsInput && PC->bShowMouseCursor && Viewport->IgnoreInput());
   check(PC->GetCurrentInputModeDebugString().StartsWith(TEXT("UI Only")));
   // Use the game's real controller override: CommonUI may request gameplay
   // after opening. A registered terminal owner must retain UI input.
   PC->SetInputMode(FInputModeGameOnly());PC->SetShowMouseCursor(false);
   check(Viewport->IgnoreInput() && PC->ShouldShowMouseCursor());
   Terminal->SetVisibility(ESlateVisibility::Collapsed);
   check(!SafeTerminal->OwnsInput && !PC->bShowMouseCursor && !Viewport->IgnoreInput());
   check(PC->GetCurrentInputModeDebugString().StartsWith(TEXT("Game Only")));
  }
  UVTGTerminalSaveLibrary::SynchronizeTerminalInput(Pawn);check(!Viewport->IgnoreInput());
  // Exercise the actual Blueprint Close route through the native preview handler.
  // The inventory sibling is intentionally absent in this isolated test world.
  if(auto* Initialized=FindFProperty<FBoolProperty>(HUD->GetClass(),TEXT("HUD_Initialized")))Initialized->SetPropertyValue_InContainer(HUD,true);
  Terminal->SetVisibility(ESlateVisibility::Visible);UVTGTerminalSaveLibrary::SynchronizeTerminalInput(Pawn);
  World->TimeSeconds=3;World->RealTimeSeconds=100;
  const FKeyEvent TabPress(EKeys::Tab,FModifierKeysState(),0,false,0,0);
  check(SafeTerminal->NativeOnPreviewKeyDown(FGeometry(),TabPress).IsEventHandled());
  check(Terminal->GetVisibility()==ESlateVisibility::Collapsed && !Viewport->IgnoreInput() && !PC->bShowMouseCursor);
  World->TimeSeconds=OriginalTime;World->RealTimeSeconds=OriginalRealTime;
  Viewport->Viewport=nullptr;SceneViewport.Reset();Context.GameViewport=nullptr;Local->ViewportClient=nullptr;
  UE_LOG(LogTemp,Display,TEXT("TERMINAL FOCUS PASS: 100 scene-viewport UI Only/Game Only transitions, cursor, owning player repair, hidden-page resync, actual Tab-to-Blueprint-Close route"));
  SaveGate(World,Pawn,Save);
  DestructionTracking(World,Pawn,Save);
  SaveKeyWatcher(World);
  SaveIndicator(PC);
  MainMenuPage(PC);
  GI->RemoveLocalPlayer(Local);World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
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
  check(A.Num()==4 && Indices.Num()==4 && !Indices.Contains(0)&&Indices.Contains(1)&&Indices.Contains(2)&&Indices.Contains(3)&&Indices.Contains(254));
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
