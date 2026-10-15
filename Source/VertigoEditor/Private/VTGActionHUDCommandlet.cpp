#include "VTGActionHUDCommandlet.h"
#include "UI/VTGActionHUD.h"
#include "UI/VTGEnemyIdentityComponent.h"
#include "VTGGameInstanceBase.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/ProgressBar.h"
#include "Components/WidgetComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"

namespace ActionRepair
{
 const TCHAR* ActionPath=TEXT("/Game/Blueprints/Widget/WBP_Action");
 void Compile(UBlueprint* BP)
 {FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);checkf(Log.NumErrors==0,TEXT("Compile failed: %s"),*BP->GetName());}
 void Backup(UObject* O)
 {
  const FString File=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
  if(!IFileManager::Get().FileExists(*File))return;
  const FString Target=FPaths::ProjectDir()/TEXT("Scripts/ActionHUD/Backup")/(O->GetName()+TEXT(".uasset"));
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target),true);
  if(!IFileManager::Get().FileExists(*Target))check(IFileManager::Get().Copy(*Target,*File)==COPY_OK);
 }
 void Save(UObject* O)
 {O->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(O->GetOutermost(),O,*FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),A));}
 UTextBlock* Text(UWidgetTree* Tree,FName Name,const FText& Value,int Size)
 {auto* T=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);T->SetText(Value);auto Font=T->GetFont();Font.Size=Size;Font.OutlineSettings.OutlineSize=1;Font.OutlineSettings.OutlineColor=FLinearColor::Black;T->SetFont(Font);T->SetShadowOffset(FVector2D(1,2));T->SetShadowColorAndOpacity(FLinearColor(0,0,0,.85));T->SetColorAndOpacity(FSlateColor(FLinearColor(.95,.94,.9,1)));return T;}
 void Hint(UWidgetBlueprint* BP,UCanvasPanel* Root,const TCHAR* Action,const TCHAR* Label,float Y)
 {
  auto* T=Text(BP->WidgetTree,*FString::Printf(TEXT("%sLabel"),Action),NSLOCTEXT("VTG","DodgeHint","Dodge"),22);
  if(FString(Action)==TEXT("Punch"))T->SetText(NSLOCTEXT("VTG","PunchHint","Punch"));
  T->SetJustification(ETextJustify::Right);
  auto* TS=Root->AddChildToCanvas(T);TS->SetAnchors(FAnchors(1,1));TS->SetPosition(FVector2D(-300,Y));TS->SetSize(FVector2D(140,34));
  auto* Key=Text(BP->WidgetTree,*FString::Printf(TEXT("%sKey"),Action),FText::FromString(TEXT("—")),16);Key->SetJustification(ETextJustify::Center);
  auto* Badge=BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),*FString::Printf(TEXT("%sKeyBadge"),Action));Badge->SetBrushColor(FLinearColor(.06,.065,.075,.9));Badge->SetPadding(FMargin(10,4));Badge->SetContent(Key);
  auto* Slot=Root->AddChildToCanvas(Badge);Slot->SetAnchors(FAnchors(1,1));Slot->SetPosition(FVector2D(-145,Y));Slot->SetSize(FVector2D(105,32));
 }
 void Identity(UBlueprint* BP,const TCHAR* DefaultName)
 {
  FEdGraphPinType Type;Type.PinCategory=UEdGraphSchema_K2::PC_Text;
  if(!FindFProperty<FTextProperty>(BP->GeneratedClass,TEXT("EnemyDisplayName")))
  {check(FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("EnemyDisplayName"),Type));FBlueprintEditorUtils::SetBlueprintVariableCategory(BP,TEXT("EnemyDisplayName"),nullptr,FText::FromString(TEXT("Enemy UI")));FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP,TEXT("EnemyDisplayName"),false);}
  bool Found=false;
  for(auto* N:BP->SimpleConstructionScript->GetAllNodes())Found|=N->ComponentClass==UVTGEnemyIdentityComponent::StaticClass();
  if(!Found)BP->SimpleConstructionScript->AddNode(BP->SimpleConstructionScript->CreateNode(UVTGEnemyIdentityComponent::StaticClass(),TEXT("EnemyIdentity")));
  Compile(BP);
  auto* Name=FindFProperty<FTextProperty>(BP->GeneratedClass,TEXT("EnemyDisplayName"));check(Name);
  Name->SetPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject(),FText::FromString(DefaultName));Save(BP);
 }
}

struct FVTGActionHUDRegression
{
 static void Run(UBlueprint* Player,UWidgetBlueprint* Action,UWidgetBlueprint* Bar)
 {
  const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
  auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("ActionHUDRegression"),nullptr,true,ERHIFeatureLevel::Num,&Init);
  auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(World);auto* GI=NewObject<UVTGGameInstanceBase>();World->SetGameInstance(GI);Context.OwningGameInstance=GI;
  FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Pawn=World->SpawnActor<APawn>(Player->GeneratedClass,FTransform::Identity,Spawn);auto* PC=World->SpawnActor<APlayerController>();auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,0);PC->SetPlayer(Local);PC->Possess(Pawn);if(!UGameplayStatics::GetPlayerController(World,0))World->AddController(PC);
  auto* Presentation=Pawn->FindComponentByClass<UVTGPlayerPresentationComponent>();check(Presentation && Presentation->ActionHUDClass==Action->GeneratedClass && Presentation->ActionHUDZOrder<0);
  auto* Combat=FindFProperty<FBoolProperty>(Pawn->GetClass(),TEXT("InCombat"));check(Combat);
  Combat->SetPropertyValue_InContainer(Pawn,false);Presentation->TickComponent(.016,LEVELTICK_All,nullptr);check(Presentation->Hearts && Presentation->Hearts->GetVisibility()==ESlateVisibility::Collapsed);
  auto* Instance=Presentation->Hearts.Get();
  for(int I=0;I<3;++I){Combat->SetPropertyValue_InContainer(Pawn,true);Presentation->TickComponent(.016,LEVELTICK_All,nullptr);check(Presentation->Hearts==Instance && Instance->GetVisibility()==ESlateVisibility::HitTestInvisible);Combat->SetPropertyValue_InContainer(Pawn,false);Presentation->TickComponent(.016,LEVELTICK_All,nullptr);check(Instance->GetVisibility()==ESlateVisibility::Collapsed);}
  check(Instance->GetWidgetFromName(TEXT("DodgeKey")) && Instance->GetWidgetFromName(TEXT("PunchKey")));
  auto* EnemyBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/NPC/StreetGoons/BP_Enm_StreetGoon1Misto"));auto* Enemy=World->SpawnActor<AActor>(EnemyBP->GeneratedClass,FTransform::Identity,Spawn);check(Enemy);
  auto* Identity=Enemy->FindComponentByClass<UVTGEnemyIdentityComponent>();check(Identity);
  UWidgetComponent* HealthBar=nullptr;for(UActorComponent* C:Enemy->GetComponents())if(auto* W=Cast<UWidgetComponent>(C);W && W->GetWidgetClass()==Bar->GeneratedClass)HealthBar=W;check(HealthBar);HealthBar->InitWidget();
  auto* Name=FindFProperty<FTextProperty>(Enemy->GetClass(),TEXT("EnemyDisplayName"));check(Name);Name->SetPropertyValue_InContainer(Enemy,FText::FromString(TEXT("CUSTOM NAME")));Identity->RefreshName();
  auto* Text=Cast<UTextBlock>(HealthBar->GetUserWidgetObject()->GetWidgetFromName(TEXT("EnemyName")));check(Text && Text->GetText().ToString()==TEXT("CUSTOM NAME") && HealthBar->GetDrawSize().Y>=40);
  for(const TCHAR* Path : {TEXT("/Game/Characters/NPC/StreetGoons/BP_Enm_StreetGoon2Nara"),TEXT("/Game/Characters/NPC/StreetGoons/BP_Enm_SteetGoon3_Brosky"),TEXT("/Game/Characters/NPC/GenericOnes/DrunkBarFightGuy/BP_Enm_BarFighter"),TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_Egg")})
  {
   auto* BP=LoadObject<UBlueprint>(nullptr,Path);check(BP);auto* Actor=World->SpawnActor<AActor>(BP->GeneratedClass,FTransform::Identity,Spawn);check(Actor);
   auto* IdentityComp=Actor->FindComponentByClass<UVTGEnemyIdentityComponent>();check(IdentityComp);
   UWidgetComponent* WidgetComp=nullptr;for(UActorComponent* C:Actor->GetComponents())if(auto* W=Cast<UWidgetComponent>(C);W && W->GetWidgetClass()==Bar->GeneratedClass)WidgetComp=W;
   checkf(WidgetComp,TEXT("Missing overhead bar on %s"),Path);WidgetComp->InitWidget();IdentityComp->RefreshName();
   auto* NameText=Cast<UTextBlock>(WidgetComp->GetUserWidgetObject()->GetWidgetFromName(TEXT("EnemyName")));check(NameText && !NameText->GetText().IsEmpty());
   UE_LOG(LogTemp,Display,TEXT("ENEMY NAME PASS: %s = %s"),Path,*NameText->GetText().ToString());
  }
  Instance->RemoveFromParent();Presentation->Hearts=nullptr;
  GI->RemoveLocalPlayer(Local);Local->PlayerController=nullptr;
  World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
  UE_LOG(LogTemp,Display,TEXT("ACTION HUD PASS: real player creates one WBP_Action; three combat enter/exit cycles reuse/collapse it; layer below Narrative; both hints exist; inherited goon instance name updates its existing overhead bar."));
 }
};

static void RenderPreview(UWidgetBlueprint* Action,UWidgetBlueprint* Bar,const FString& Output)
{
 if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
 auto* HUD=NewObject<UVTGActionHUD>(GetTransientPackage(),Action->GeneratedClass);HUD->Initialize();HUD->Full=LoadObject<UTexture2D>(nullptr,TEXT("/Game/2DArt/UI/Action/UI_SaHealth_Filled"));HUD->Empty=LoadObject<UTexture2D>(nullptr,TEXT("/Game/2DArt/UI/Action/UI_SaHealth_Empty"));HUD->Health=68;HUD->MaxHealth=100;
 CastChecked<UTextBlock>(HUD->GetWidgetFromName(TEXT("DodgeKey")))->SetText(FText::FromString(TEXT("Space")));CastChecked<UTextBlock>(HUD->GetWidgetFromName(TEXT("PunchKey")))->SetText(FText::FromString(TEXT("LMB")));
 auto* Enemy=NewObject<UUserWidget>(GetTransientPackage(),Bar->GeneratedClass);Enemy->Initialize();CastChecked<UTextBlock>(Enemy->GetWidgetFromName(TEXT("EnemyName")))->SetText(FText::FromString(TEXT("Misto")));CastChecked<UProgressBar>(Enemy->GetWidgetFromName(TEXT("ProgressBar_Instant")))->SetPercent(.65);CastChecked<UProgressBar>(Enemy->GetWidgetFromName(TEXT("ProgressBar_CatchUp")))->SetPercent(.85);
 auto* Box=NewObject<USizeBox>();Box->SetWidthOverride(220);Box->SetHeightOverride(40);Box->SetContent(Enemy);
 auto Root=SNew(SOverlay)+SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.025,.035,.045,1))]+SOverlay::Slot()[HUD->TakeWidget()]+SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Box->TakeWidget()];
 if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
 FWidgetRenderer Renderer(true);auto* Target=Renderer.DrawWidget(Root,FVector2D(1920,1080));FlushRenderingCommands();TArray<FColor> Pixels;check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1920,1080,Pixels,PNG);check(FFileHelper::SaveArrayToFile(PNG,*Output));
}

UVTGActionHUDCommandlet::UVTGActionHUDCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UVTGActionHUDCommandlet::Main(const FString& Params)
{
 using namespace ActionRepair;
 if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
 const bool Verify=Params.Contains(TEXT("VerifyOnly"));
 auto* Player=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));auto* Bar=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Widget/EnemyHealthBar/WBP_EnemyHealthBar"));check(Player && Bar);
 auto* Action=LoadObject<UWidgetBlueprint>(nullptr,ActionPath);
 if(!Verify)
 {
  check(!Action);auto* Package=CreatePackage(ActionPath);Action=CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(UVTGActionHUD::StaticClass(),Package,TEXT("WBP_Action"),BPTYPE_Normal,UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass()));FAssetRegistryModule::AssetCreated(Action);
  auto* Canvas=Action->WidgetTree->ConstructWidget<UCanvasPanel>();Action->WidgetTree->RootWidget=Canvas;Hint(Action,Canvas,TEXT("Dodge"),TEXT("Dodge"),-145);Hint(Action,Canvas,TEXT("Punch"),TEXT("Punch"),-102);Compile(Action);
  auto* CDO=CastChecked<UVTGActionHUD>(Action->GeneratedClass->GetDefaultObject());CDO->DodgeAction=LoadObject<UInputAction>(nullptr,TEXT("/Game/ThirdPerson/Input/Actions/IA_Dodge"));CDO->PunchAction=LoadObject<UInputAction>(nullptr,TEXT("/Game/ThirdPerson/Input/Actions/IA_Fire"));check(CDO->DodgeAction && CDO->PunchAction);Save(Action);
  Backup(Player);bool Found=false;for(auto* N:Player->SimpleConstructionScript->GetAllNodes())if(auto* C=Cast<UVTGPlayerPresentationComponent>(N->ComponentTemplate)){C->ActionHUDClass=Action->GeneratedClass;C->ActionHUDZOrder=-10;Found=true;}check(Found);Compile(Player);Save(Player);
  Backup(Bar);auto* Root=CastChecked<UCanvasPanel>(Bar->WidgetTree->RootWidget);
  auto* Name=Text(Bar->WidgetTree,TEXT("EnemyName"),NSLOCTEXT("VTG","EnemyNameFallback","Enemy"),16);Name->SetJustification(ETextJustify::Center);auto* S=Root->AddChildToCanvas(Name);S->SetAnchors(FAnchors(0,0,1,0));S->SetOffsets(FMargin(0,0,0,24));
  for(const TCHAR* N:{TEXT("ProgressBar_CatchUp"),TEXT("ProgressBar_Instant")}){auto* W=Bar->WidgetTree->FindWidget(N);check(W);auto* Slot=CastChecked<UCanvasPanelSlot>(W->Slot);Slot->SetAnchors(FAnchors(0,0,1,0));Slot->SetOffsets(FMargin(0,27,0,7));}
  Compile(Bar);Save(Bar);
  auto* Base=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AI/AIBehaviorSystem/Blueprint/AI/BP_AI_Base"));auto* Egg=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_Egg"));check(Base && Egg);Backup(Base);Identity(Base,TEXT("Enemy"));Backup(Egg);Identity(Egg,TEXT("Sea Monster Egg"));
  const TPair<const TCHAR*,const TCHAR*> Enemies[]={
   {TEXT("/Game/Characters/NPC/StreetGoons/BP_Enm_StreetGoon1Misto"),TEXT("Misto")},{TEXT("/Game/Characters/NPC/StreetGoons/BP_Enm_StreetGoon2Nara"),TEXT("Nara")},{TEXT("/Game/Characters/NPC/StreetGoons/BP_Enm_SteetGoon3_Brosky"),TEXT("Brosky")},{TEXT("/Game/Characters/NPC/GenericOnes/DrunkBarFightGuy/BP_Enm_BarFighter"),TEXT("Bar Fighter")}};
  for(auto Pair:Enemies){auto* BP=LoadObject<UBlueprint>(nullptr,Pair.Key);check(BP);Backup(BP);Compile(BP);auto* NameProp=FindFProperty<FTextProperty>(BP->GeneratedClass,TEXT("EnemyDisplayName"));check(NameProp);NameProp->SetPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject(),FText::FromString(Pair.Value));Save(BP);}
  // L2 used a standalone widget displaying a static enemy name. The Action HUD now owns combat UI.
  auto* L2=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight"));check(L2);Backup(L2);
  for(UEdGraph* G:L2->UbergraphPages){auto Nodes=G->Nodes;for(UEdGraphNode* N:Nodes)if(UEdGraphNode* Create=N;Create->GetClass()->GetName()==TEXT("K2Node_CreateWidget") && Create->FindPin(TEXT("Class"))->DefaultObject && Create->FindPin(TEXT("Class"))->DefaultObject->GetPathName()==TEXT("/Game/Widget/Texture/WBP_BattleModeHUD.WBP_BattleModeHUD_C")){
   auto* Out=Create->FindPinChecked(TEXT("then"));check(Out->LinkedTo.Num()==1);auto* Add=CastChecked<UK2Node_CallFunction>(Out->LinkedTo[0]->GetOwningNode());check(Add->FunctionReference.GetMemberName()==TEXT("AddToViewport") && Add->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty());FBlueprintEditorUtils::RemoveNode(L2,Add,true);FBlueprintEditorUtils::RemoveNode(L2,Create,true);
  }}Compile(L2);Save(L2);
 }
 check(Action);Compile(Action);Compile(Player);Compile(Bar);FVTGActionHUDRegression::Run(Player,Action,Bar);
 FString Output;if(FParse::Value(*Params,TEXT("Preview="),Output))RenderPreview(Action,Bar,Output);
 UE_LOG(LogTemp,Display,TEXT("ACTION ASSETS PASS: WBP_Action, player HUD class, enemy name slot and all requested enemy defaults saved/compiled."));return 0;
}

