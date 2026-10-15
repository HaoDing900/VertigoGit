#include "VTGHUDUnificationCommandlet.h"
#include "UI/VTGActionHUD.h"
#include "Combat/VTGCombatPresentation.h"
#include "Save/VTGL2FightStateComponent.h"
#include "Save/VTGSaveCoordinator.h"
#include "VTGGameInstanceBase.h"
#include "VTGLevelManagerBase.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ScaleBox.h"
#include "Components/OverlaySlot.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "GameFramework/PlayerController.h"
#include "CommonInputBaseTypes.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Kismet/GameplayStatics.h"
#include "Components/WeaponBase_ISX.h"
#include "Objects/Item.h"
#include "CommonActionWidget.h"
#include "Engine/DataTable.h"

namespace HUDUnification
{
void Compile(UBlueprint* BP){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog L;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&L);checkf(!L.NumErrors,TEXT("Compile %s"),*BP->GetName());}
void Backup(UObject* O){auto File=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());auto Dest=FPaths::ProjectDir()/TEXT("Scripts/HUDUnification/Backup")/(O->GetName()+TEXT(".uasset"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest),true);if(!IFileManager::Get().FileExists(*Dest))check(IFileManager::Get().Copy(*Dest,*File)==COPY_OK);}
void Save(UObject* O){O->GetOutermost()->FullyLoad();ResetLoaders(O->GetOutermost());O->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(O->GetOutermost(),O,*FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),A));}
FString TextureForKey(FKey Key)
{
 FString K=Key.GetFName().ToString(),Suffix;
 const TMap<FString,FString> PC={{"SpaceBar","Space"},{"Escape","Esc"},{"LeftMouseButton","Mouse_Left"},{"RightMouseButton","Mouse_Right"},{"MiddleMouseButton","Mouse_Middle"},{"LeftControl","Ctrl"},{"RightControl","Ctrl"},{"LeftShift","Shift"},{"RightShift","Shift"},{"LeftAlt","Alt"},{"RightAlt","Alt"},{"LeftCommand","Command"},{"RightCommand","Command"},{"CapsLock","Caps_Lock"},{"Delete","Del"},{"PageUp","Page_Up"},{"PageDown","Page_Down"},{"Up","Arrow_Up"},{"Down","Arrow_Down"},{"Left","Arrow_Left"},{"Right","Arrow_Right"},{"Zero","T_0"},{"One","1"},{"Two","2"},{"Three","3"},{"Four","4"},{"Five","5"},{"Six","6"},{"Seven","7"},{"Eight","8"},{"Nine","9"},{"Hyphen","Minus"},{"Equals","Plus"},{"LeftBracket","Bracket_Left"},{"RightBracket","Bracket_Right"},{"Backslash","Slash"},{"Slash","Slash"},{"Semicolon","Semicolon"},{"Apostrophe","Quote"},{"Tilde","Tilda"}};
 const TMap<FString,FString> Pad={{"Gamepad_FaceButton_Bottom","A"},{"Gamepad_FaceButton_Right","B"},{"Gamepad_FaceButton_Left","X"},{"Gamepad_FaceButton_Top","Y"},{"Gamepad_LeftShoulder","LB"},{"Gamepad_RightShoulder","RB"},{"Gamepad_LeftTrigger","LT"},{"Gamepad_LeftTriggerAxis","LT"},{"Gamepad_RightTrigger","RT"},{"Gamepad_RightTriggerAxis","RT"},{"Gamepad_LeftThumbstick","Left_Stick_Click"},{"Gamepad_RightThumbstick","Right_Stick_Click"},{"Gamepad_LeftX","Left_Stick"},{"Gamepad_LeftY","Left_Stick"},{"Gamepad_RightX","Right_Stick"},{"Gamepad_RightY","Right_Stick"},{"Gamepad_Special_Left","View"},{"Gamepad_Special_Right","Menu"},{"Gamepad_DPad_Up","Dpad_Up"},{"Gamepad_DPad_Down","Dpad_Down"},{"Gamepad_DPad_Left","Dpad_Left"},{"Gamepad_DPad_Right","Dpad_Right"}};
 if(Key.IsGamepadKey()){const auto* P=Pad.Find(K);if(!P)return {};return TEXT("/Game/Inventory/InventoryXBased/UMG/Textures/Controls/Gamepad/T_Gamepad_")+*P;}
 const auto* P=PC.Find(K);Suffix=P?*P:K;
 const FString Path=TEXT("/Game/Inventory/InventoryXBased/UMG/Textures/Controls/PC/T_")+Suffix+TEXT("_Key_Dark");
 return FPackageName::DoesPackageExist(Path)?Path:FString();
}
void Controllers(bool Verify)
{
 for(const TCHAR* Name:{TEXT("ControllerData_PC_Keyboard"),TEXT("ControllerData_PC_Gamepad_PS4"),TEXT("ControllerData_PC_Gamepad_Xbox")})
 {
  const FString Target=TEXT("/Game/UI/ControllerData/")+FString(Name);auto* BP=LoadObject<UBlueprint>(nullptr,*Target);
  if(!Verify){if(!BP)BP=CastChecked<UBlueprint>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(Name,TEXT("/Game/UI/ControllerData"),LoadObject<UBlueprint>(nullptr,*(TEXT("/NarrativeCommonUI/ControllerData/")+FString(Name)))));Compile(BP);auto* Data=CastChecked<UCommonInputBaseControllerData>(BP->GeneratedClass->GetDefaultObject());
   // Update every existing brush and add any project key that was missing from the source data.
   TArray<FKey> Keys;EKeys::GetAllKeys(Keys);for(const auto& Key:Keys){if(Key.IsGamepadKey()!=(Data->InputType==ECommonInputType::Gamepad))continue;const auto Path=TextureForKey(Key);if(Path.IsEmpty())continue;auto* Texture=LoadObject<UTexture2D>(nullptr,*Path);if(!Texture)continue;auto* Entry=Data->InputBrushDataMap.FindByPredicate([&](const FCommonInputKeyBrushConfiguration& E){return E.Key==Key;});if(!Entry){FCommonInputKeyBrushConfiguration E;E.Key=Key;Data->InputBrushDataMap.Add(E);Entry=&Data->InputBrushDataMap.Last();}Entry->KeyBrush.SetResourceObject(Texture);Entry->KeyBrush.ImageSize=FVector2D(32.f*float(Texture->GetSizeX())/FMath::Max(1,Texture->GetSizeY()),32);Entry->KeyBrush.DrawAs=ESlateBrushDrawType::Image;}
   Save(BP);
  }
  check(BP);const auto* Data=CastChecked<UCommonInputBaseControllerData>(BP->GeneratedClass->GetDefaultObject());UE_LOG(LogTemp,Display,TEXT("CONTROLLER %s: %s"),Name,*Data->GamepadName.ToString());FSlateBrush Brush;check(Data->TryGetInputBrush(Brush,Data->InputType==ECommonInputType::Gamepad?EKeys::Gamepad_FaceButton_Bottom:EKeys::SpaceBar));check(Brush.GetResourceObject()->GetPathName().Contains(TEXT("/Inventory/InventoryXBased/UMG/Textures/Controls/")));
 }
}
void Action(UWidgetBlueprint* BP,bool Verify)
{
 if(!Verify){Backup(BP);auto* Root=CastChecked<UCanvasPanel>(BP->WidgetTree->RootWidget);
  for(const TCHAR* Key:{TEXT("DodgeKey"),TEXT("PunchKey")})
  {auto* Text=BP->WidgetTree->FindWidget(Key);check(Text);auto* Badge=CastChecked<UBorder>(BP->WidgetTree->FindWidget(*FString(Key).Append(TEXT("Badge"))));const bool Dodge=FString(Key)==TEXT("DodgeKey");auto* BadgeSlot=CastChecked<UCanvasPanelSlot>(Badge->Slot);BadgeSlot->SetSize(FVector2D(105,48));BadgeSlot->SetPosition(FVector2D(-145,Dodge?-169:-113));auto* Label=BP->WidgetTree->FindWidget(Dodge?TEXT("DodgeLabel"):TEXT("PunchLabel"));CastChecked<UCanvasPanelSlot>(Label->Slot)->SetPosition(FVector2D(-300,Dodge?-160:-104));if(!BP->WidgetTree->FindWidget(*FString(Key).Append(TEXT("Icon")))){Badge->SetBrushColor(FLinearColor::Transparent);Badge->SetPadding(FMargin(0));Badge->RemoveChild(Text);auto* Overlay=BP->WidgetTree->ConstructWidget<UOverlay>();Badge->SetContent(Overlay);Overlay->AddChildToOverlay(Text);Text->SetVisibility(ESlateVisibility::Collapsed);auto* Icon=BP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),*FString(Key).Append(TEXT("Icon")));Icon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,*TextureForKey(FString(Key)==TEXT("DodgeKey")?EKeys::SpaceBar:EKeys::LeftMouseButton)));auto* Slot=Overlay->AddChildToOverlay(Icon);Slot->SetHorizontalAlignment(HAlign_Center);Slot->SetVerticalAlignment(VAlign_Fill);}auto* Icon=CastChecked<UImage>(BP->WidgetTree->FindWidget(*FString(Key).Append(TEXT("Icon"))));if(auto* Overlay=Cast<UOverlay>(Icon->GetParent())){Overlay->RemoveChild(Icon);auto* Scale=BP->WidgetTree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);Scale->SetContent(Icon);Overlay->AddChildToOverlay(Scale);}}
  if(!BP->WidgetTree->FindWidget(TEXT("BloodOverlay"))){auto* Old=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Blueprints/Widget/Unused/WBP_LegacyActionHUD"));auto* Reference=CastChecked<UImage>(Old->WidgetTree->FindWidget(TEXT("BloodOverlay")));auto* Blood=BP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("BloodOverlay"));Blood->SetBrush(Reference->GetBrush());Blood->SetColorAndOpacity(Reference->GetColorAndOpacity());Blood->SetVisibility(ESlateVisibility::Collapsed);auto* S=Root->AddChildToCanvas(Blood);S->SetAnchors(FAnchors(0,0,1,1));S->SetOffsets(FMargin(0));S->SetZOrder(-1);}
  Compile(BP);Save(BP);
 }
 check(BP->WidgetTree->FindWidget(TEXT("BloodOverlay")) && BP->WidgetTree->FindWidget(TEXT("DodgeKeyIcon")) && BP->WidgetTree->FindWidget(TEXT("PunchKeyIcon")));
}
void PlayerPolicy(UBlueprint* BP,bool Verify)
{
 if(!Verify){Backup(BP);bool Found=false;
 for(UEdGraph* G:BP->UbergraphPages){const auto Nodes=G->Nodes;for(UEdGraphNode* Node:Nodes)if(auto* N=Cast<UK2Node_CallFunction>(Node);N && N->FunctionReference.GetMemberName()==TEXT("BooleanOR") && N->NodeComment.Contains(TEXT("sewer stays in combat")))
 {auto* Policy=NewObject<UK2Node_CallFunction>(G);G->AddNode(Policy,false,false);Policy->CreateNewGuid();Policy->SetFromFunction(UVTGPlayerPresentationComponent::StaticClass()->FindFunctionByName(TEXT("ResolveCombatState")));Policy->NodePosX=N->NodePosX;Policy->NodePosY=N->NodePosY;Policy->NodeComment=TEXT("Sewer free combat ends after Boss defeat; cinematic/dead players stay out of combat");Policy->AllocateDefaultPins();
 auto* Get=NewObject<UK2Node_VariableGet>(G);G->AddNode(Get,false,false);Get->CreateNewGuid();Get->VariableReference.SetSelfMember(TEXT("CombatPresentation"));Get->AllocateDefaultPins();Get->NodePosX=N->NodePosX-180;Get->NodePosY=N->NodePosY+180;auto* Schema=GetDefault<UEdGraphSchema_K2>();check(Schema->TryCreateConnection(Get->FindPinChecked(TEXT("CombatPresentation")),Policy->FindPinChecked(TEXT("self"))));
 for(const auto& Pair:{TPair<FName,FName>(TEXT("A"),TEXT("EnemyNearby")),TPair<FName,FName>(TEXT("B"),TEXT("SewerLevel")),TPair<FName,FName>(TEXT("ReturnValue"),TEXT("ReturnValue"))}){auto* Old=N->FindPinChecked(Pair.Key);const auto Links=Old->LinkedTo;Old->BreakAllPinLinks();for(auto* P:Links)check(Schema->TryCreateConnection(Policy->FindPinChecked(Pair.Value),P));}
 FBlueprintEditorUtils::RemoveNode(BP,N,true);Found=true;}}
 if(Found){Compile(BP);Save(BP);}
 }
 bool Found=false;for(UEdGraph* G:BP->UbergraphPages)for(UEdGraphNode* Node:G->Nodes)if(auto* N=Cast<UK2Node_CallFunction>(Node);N && N->FunctionReference.GetMemberName()==TEXT("ResolveCombatState"))Found=true;check(Found);
}
}

struct FVTGHUDUnificationRegression
{
 static void Run(UBlueprint* Player,UWidgetBlueprint* HUD,UBlueprint* Boss)
 {
  if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
  const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
  auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("HUDUnificationRegression"),nullptr,true,ERHIFeatureLevel::Num,&Init);auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(World);
  auto* GI=NewObject<UVTGGameInstanceBase>();World->SetGameInstance(GI);Context.OwningGameInstance=GI;GI->Init();auto* Save=GI->GetSubsystem<UVTGSaveCoordinator>();check(Save);
  FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;auto* Pawn=World->SpawnActor<APawn>(Player->GeneratedClass,FTransform::Identity,Spawn);auto* PC=World->SpawnActor<APlayerController>();auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,0);PC->SetPlayer(Local);PC->Possess(Pawn);if(!UGameplayStatics::GetPlayerPawn(World,0))World->AddController(PC);
  auto* Presentation=Pawn->FindComponentByClass<UVTGPlayerPresentationComponent>();check(Presentation);
  auto SetBool=[&](const TCHAR* N,bool V){auto* P=FindFProperty<FBoolProperty>(Pawn->GetClass(),N);check(P);P->SetPropertyValue_InContainer(Pawn,V);};
  auto SetNumber=[&](const TCHAR* N,double V){auto* P=FindFProperty<FNumericProperty>(Pawn->GetClass(),N);check(P);auto* A=P->ContainerPtrToValuePtr<void>(Pawn);if(P->IsInteger())P->SetIntPropertyValue(A,int64(V));else P->SetFloatingPointPropertyValue(A,V);};
  SetBool(TEXT("InCombat"),true);SetBool(TEXT("IsDead?"),false);SetBool(TEXT("HidePlayerNCam"),false);SetBool(TEXT("HasWeaponInHand"),false);SetBool(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),false);SetNumber(TEXT("MaxHealth_Normal"),100);SetNumber(TEXT("Health"),15);SetNumber(TEXT("CurrentMeleeWeapon"),0);
  Presentation->TickComponent(.016,LEVELTICK_All,nullptr);auto* Action=CastChecked<UVTGActionHUD>(Presentation->Hearts);auto* Blood=Action->GetWidgetFromName(TEXT("BloodOverlay"));check(Blood && Blood->GetVisibility()==ESlateVisibility::HitTestInvisible);
  SetNumber(TEXT("Health"),16);Presentation->TickComponent(.016,LEVELTICK_All,nullptr);check(Blood->GetVisibility()==ESlateVisibility::Collapsed);
  SetNumber(TEXT("Health"),1);Presentation->TickComponent(.016,LEVELTICK_All,nullptr);check(Blood->GetVisibility()==ESlateVisibility::HitTestInvisible);
  SetNumber(TEXT("Health"),0);Presentation->TickComponent(.016,LEVELTICK_All,nullptr);check(Blood->GetVisibility()==ESlateVisibility::Collapsed);
  SetNumber(TEXT("Health"),10);SetBool(TEXT("InCombat"),false);Presentation->TickComponent(.016,LEVELTICK_All,nullptr);check(Blood->GetVisibility()==ESlateVisibility::Collapsed);
  SetBool(TEXT("InCombat"),true);SetBool(TEXT("IsDead?"),true);Presentation->TickComponent(.016,LEVELTICK_All,nullptr);check(Blood->GetVisibility()==ESlateVisibility::Collapsed);SetBool(TEXT("IsDead?"),false);SetNumber(TEXT("Health"),100);
  // Exercise the component names used by the user's attached meshes even if the editor has unsaved components.
  auto* Knuckle=NewObject<UStaticMeshComponent>(Pawn,TEXT("Weapon_BrassKnuckle_L_Test"));Pawn->AddInstanceComponent(Knuckle);Knuckle->RegisterComponent();Presentation->RefreshBrassKnuckles();check(!Knuckle->bHiddenInGame && Knuckle->IsVisible());
  SetNumber(TEXT("CurrentMeleeWeapon"),1);Presentation->RefreshBrassKnuckles();check(Knuckle->bHiddenInGame);SetNumber(TEXT("CurrentMeleeWeapon"),0);SetBool(TEXT("HasWeaponInHand"),true);Presentation->RefreshBrassKnuckles();check(Knuckle->bHiddenInGame);SetBool(TEXT("HasWeaponInHand"),false);SetBool(TEXT("InCombat"),false);Presentation->RefreshBrassKnuckles();check(Knuckle->bHiddenInGame);
  SetBool(TEXT("InCombat"),true);auto* Weapon=Pawn->FindComponentByClass<UWeaponBase_ISX>();check(Weapon);Weapon->EquippedItem=GetMutableDefault<UItem>();Presentation->RefreshBrassKnuckles();check(Knuckle->bHiddenInGame);Weapon->EquippedItem=nullptr;Presentation->RefreshBrassKnuckles();check(!Knuckle->bHiddenInGame);
  FSlateBrush KeyBrush;check(UCommonInputPlatformSettings::Get()->TryGetInputBrush(KeyBrush,EKeys::SpaceBar,ECommonInputType::MouseAndKeyboard,NAME_None));check(KeyBrush.GetResourceObject()->GetPathName().Contains(TEXT("/Inventory/InventoryXBased/UMG/Textures/Controls/")));
  auto* ActionIconClass=LoadClass<UCommonActionWidget>(nullptr,TEXT("/NarrativeCommonUI/Widgets/WBP_NarrativeCommonActionWidget.WBP_NarrativeCommonActionWidget_C"));check(ActionIconClass);auto* NarrativeIcon=NewObject<UCommonActionWidget>(Action->WidgetTree,ActionIconClass);auto* Actions=LoadObject<UDataTable>(nullptr,TEXT("/NarrativeCommonUI/ControllerData/DT_NarrativeInputActions"));check(Actions);int32 IconCount=0;for(auto Name:Actions->GetRowNames()){FDataTableRowHandle Row;Row.DataTable=Actions;Row.RowName=Name;NarrativeIcon->SetInputAction(Row);const auto Brush=NarrativeIcon->GetIcon();if(auto* Resource=Brush.GetResourceObject()){UE_LOG(LogTemp,Display,TEXT("NARRATIVE ICON %s: %s"),*Name.ToString(),*Resource->GetPathName());check(Resource->GetPathName().Contains(TEXT("/Inventory/InventoryXBased/UMG/Textures/Controls/")));++IconCount;}}check(IconCount>0);
  check(Presentation->ResolveCombatState(false,true));check(!Presentation->ResolveCombatState(false,false));SetBool(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),true);check(!Presentation->ResolveCombatState(true,true));SetBool(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),false);
  auto* Enemy=World->SpawnActor<AActor>(Boss->GeneratedClass,FTransform::Identity,Spawn);auto* Feedback=Enemy->FindComponentByClass<UVTGEnemyFeedbackComponent>();check(Feedback && Feedback->bEndsSewerCombatOnDeath && Feedback->Source.Find(Enemy,TEXT("Health"),TEXT("MaxHealth")));Feedback->LastHealth=100;auto* HealthOwner=Feedback->Source.Get();auto* H=FindFProperty<FNumericProperty>(HealthOwner->GetClass(),TEXT("Health"));H->SetFloatingPointPropertyValue(H->ContainerPtrToValuePtr<void>(HealthOwner),0);Feedback->CheckHealth();check(Save->GetPersistentInt(TEXT("L3SewerBossDefeated"))==1);check(!Presentation->ResolveCombatState(true,true));check(Presentation->ResolveCombatState(true,false));check(!FindFProperty<FBoolProperty>(Pawn->GetClass(),TEXT("InCombat"))->GetPropertyValue_InContainer(Pawn));
  auto* Manager=World->SpawnActor<AVTGLevelManagerBase>();auto* Fade=Manager->FadeConfigured(8,2,3,true,true,100);check(Fade);for(const auto& Pair:{TPair<FName,double>(TEXT("TimeBeforeKill"),8),TPair<FName,double>(TEXT("HoldDurationUntilFade"),2),TPair<FName,double>(TEXT("FadeDuration"),3)}){auto* P=FindFProperty<FNumericProperty>(Fade->GetClass(),Pair.Key);check(P && P->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(Fade))==Pair.Value);}check(FindFProperty<FBoolProperty>(Fade->GetClass(),TEXT("ReverseFadeOnAppear"))->GetPropertyValue_InContainer(Fade));Fade->RemoveFromParent();
  Action->RemoveFromParent();Presentation->Hearts=nullptr;GI->RemoveLocalPlayer(Local);Local->PlayerController=nullptr;GI->Shutdown();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
  UE_LOG(LogTemp,Display,TEXT("HUD REGRESSION PASS: 15%% threshold, heal/death/exit, unarmed/melee/ranged knuckles, boss death persistent combat gate, cinematic lock, per-call fade parameters."));
 }
};

UVTGHUDUnificationCommandlet::UVTGHUDUnificationCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UVTGHUDUnificationCommandlet::Main(const FString& Params)
{
 using namespace HUDUnification;const bool Verify=Params.Contains(TEXT("VerifyOnly"));Controllers(Verify);
 auto* HUD=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Blueprints/Widget/WBP_Action"));check(HUD);Action(HUD,Verify);
 auto* L2=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight"));check(L2);if(!Verify)Backup(L2);bool Client=false;for(auto* N:L2->SimpleConstructionScript->GetAllNodes())if(auto* C=Cast<UVTGL2FightStateComponent>(N->ComponentTemplate)){if(!Verify)C->FemaleClientEndTransform.SetRotation(FRotator(0,-112,0).Quaternion());check(FMath::IsNearlyEqual(C->FemaleClientEndTransform.Rotator().Yaw,-112.,.01));Client=true;}check(Client);if(!Verify){Compile(L2);Save(L2);}
 auto* Boss=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_Boss_SeaMonster"));check(Boss);if(!Verify)Backup(Boss);bool Feedback=false;for(auto* N:Boss->SimpleConstructionScript->GetAllNodes())if(auto* C=Cast<UVTGEnemyFeedbackComponent>(N->ComponentTemplate)){if(!Verify)C->bEndsSewerCombatOnDeath=true;check(C->bEndsSewerCombatOnDeath);Feedback=true;}check(Feedback);if(!Verify){Compile(Boss);Save(Boss);}
 auto* Player=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(Player);if(!Params.Contains(TEXT("SkipPlayer")))PlayerPolicy(Player,Verify);
 for(auto* N:Player->SimpleConstructionScript->GetAllNodes())if(N->GetVariableName().ToString().Contains(TEXT("Brass")))UE_LOG(LogTemp,Display,TEXT("BRASS COMPONENT: %s"),*N->GetVariableName().ToString());
 auto* BPLM=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/BPLM"));check(BPLM && BPLM->GeneratedClass->IsChildOf(AVTGLevelManagerBase::StaticClass()));Compile(BPLM);check(BPLM->GeneratedClass->FindFunctionByName(TEXT("FadeConfigured")));
 if(Params.Contains(TEXT("Test")))FVTGHUDUnificationRegression::Run(Player,HUD,Boss);
 UE_LOG(LogTemp,Display,TEXT("HUD UNIFICATION ASSETS PASS"));return 0;
}
