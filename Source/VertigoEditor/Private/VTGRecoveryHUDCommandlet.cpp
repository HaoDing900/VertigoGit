#include "VTGRecoveryHUDCommandlet.h"
#include "UI/VTGActionHUD.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "UObject/UnrealType.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Styling/CoreStyle.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Misc/FileHelper.h"

struct FVTGRecoveryHUDRegression
{
 static void Run(const FString& Preview)
 {
  TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);
  if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
  auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
  auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("RecoveryHUDTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(W);auto* GI=NewObject<UGameInstance>();W->SetGameInstance(GI);Context.OwningGameInstance=GI;
  FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Class=LoadClass<ACharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C"));check(Class);auto* Player=W->SpawnActor<ACharacter>(Class,FTransform::Identity,Spawn);auto* PC=W->SpawnActor<APlayerController>();auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,0);PC->SetPlayer(Local);PC->Possess(Player);
  Player->GetMesh()->SetAnimInstanceClass(UAnimInstance::StaticClass());Player->GetMesh()->InitAnim(true);auto* Anim=Player->GetMesh()->GetAnimInstance();check(Anim);
  auto* HUDClass=LoadClass<UVTGActionHUD>(nullptr,TEXT("/Game/Blueprints/Widget/WBP_Action.WBP_Action_C"));check(HUDClass);auto* HUD=CreateWidget<UVTGActionHUD>(PC,HUDClass);check(HUD);
  auto Set=[&](const TCHAR* Name,bool Value){auto* P=FindFProperty<FBoolProperty>(Player->GetClass(),Name);check(P);P->SetPropertyValue_InContainer(Player,Value);};
  Set(TEXT("HidePlayerNCam"),false);Set(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),false);Set(TEXT("InCombat"),true);Set(TEXT("StoryMode?"),false);Set(TEXT("IsDead?"),false);Set(TEXT("HasWeaponInHand"),false);Set(TEXT("IsDodging?"),false);
  HUD->RefreshRecovery(Player,0);auto SlateHUD=HUD->TakeWidget();check(HUD->RecoveryBox && HUD->RecoveryBox->GetVisibility()==ESlateVisibility::Collapsed);
  auto* M=HUD->RecoveryMontage.LoadSynchronous();check(M);auto Start=[&](){Anim->Montage_Play(M);Set(TEXT("IsAttacking"),true);Player->GetCharacterMovement()->SetMovementMode(MOVE_None);HUD->RefreshRecovery(Player,0);for(int I=0;I<24;++I){Player->GetMesh()->TickAnimation(1.f/120.f,false);Player->GetMesh()->ConditionallyDispatchQueuedAnimEvents();}};
  Start();Anim->Montage_SetPosition(M,.25f);HUD->RefreshRecovery(Player,.01f);check(HUD->RecoveryBox->GetVisibility()==ESlateVisibility::Collapsed);
  Anim->Montage_SetPosition(M,.37f);HUD->RefreshRecovery(Player,.01f);UE_LOG(LogTemp,Display,TEXT("RECOVERY diagnostic visible=%d fraction=%f start=%f instance=%d interrupted=%d position=%f attacking=%d move=%d"),HUD->RecoveryBox->IsVisible(),HUD->RecoveryBar->GetPercent(),HUD->RecoveryStart,HUD->RecoveryInstanceID,HUD->bInterrupted,Anim->Montage_GetPosition(M),FindFProperty<FBoolProperty>(Player->GetClass(),TEXT("IsAttacking"))->GetPropertyValue_InContainer(Player),(int)Player->GetCharacterMovement()->MovementMode);check(HUD->RecoveryBox->IsVisible() && HUD->RecoveryBar->GetPercent()<.02f);
  Anim->Montage_SetPosition(M,.9f);HUD->RefreshRecovery(Player,.01f);const float Mid=HUD->RecoveryBar->GetPercent();check(Mid>.45f && Mid<.55f);
  Anim->Montage_SetPlayRate(M,2);HUD->RefreshRecovery(Player,.4f);check(FMath::IsNearlyEqual(Mid,HUD->RecoveryBar->GetPercent()));
  UE_LOG(LogTemp,Display,TEXT("RECOVERY TEST PASS damage-end start; real montage-position progress; clock delta/play-rate changes cannot fake progress"));
  // Natural blend-out removes the montage from the active map before its final frame.
  Anim->Montage_SetPlayRate(M,1);Anim->Montage_SetPosition(M,1.28f);
  for(int I=0;I<3;++I){Player->GetMesh()->TickAnimation(1.f/120.f,false);Player->GetMesh()->ConditionallyDispatchQueuedAnimEvents();HUD->RefreshRecovery(Player,1.f/120.f);}
  check(HUD->RecoveryBox->IsVisible() && HUD->RecoveryBar->GetPercent()>.8f && HUD->RecoveryBar->GetPercent()<1.f);
  for(int I=0;I<40;++I){Player->GetMesh()->TickAnimation(1.f/120.f,false);Player->GetMesh()->ConditionallyDispatchQueuedAnimEvents();}check(HUD->bPendingReady);
  Start();Anim->Montage_SetPosition(M,.9f);HUD->RefreshRecovery(Player,0);check(HUD->RecoveryBox->IsVisible());Anim->Montage_Stop(.1f,M);Player->GetMesh()->TickAnimation(.01f,false);Player->GetMesh()->ConditionallyDispatchQueuedAnimEvents();HUD->RefreshRecovery(Player,.01f);check(!HUD->RecoveryBox->IsVisible());
  UE_LOG(LogTemp,Display,TEXT("RECOVERY TEST PASS natural blend-out stays visible; interrupted attack hides immediately"));
  Start();Anim->Montage_SetPosition(M,.9f);HUD->RefreshRecovery(Player,0);check(HUD->RecoveryBox->IsVisible());Set(TEXT("IsDodging?"),true);HUD->RefreshRecovery(Player,0);check(!HUD->RecoveryBox->IsVisible());Set(TEXT("IsDodging?"),false);
  Set(TEXT("IsDead?"),true);HUD->RefreshRecovery(Player,0);check(!HUD->RecoveryBox->IsVisible());Set(TEXT("IsDead?"),false);
  Set(TEXT("HidePlayerNCam"),true);HUD->RefreshRecovery(Player,0);check(!HUD->RecoveryBox->IsVisible());Set(TEXT("HidePlayerNCam"),false);
  UE_LOG(LogTemp,Display,TEXT("RECOVERY TEST PASS dodge, death and cinematic visibility cancel presentation"));
  // Verify the full pulse waits for BOTH attack and movement locks to clear.
  Anim->Montage_Stop(0,M);Player->GetMesh()->ConditionallyDispatchQueuedAnimEvents();HUD->bPendingReady=true;HUD->bInterrupted=false;Set(TEXT("IsAttacking"),false);HUD->RefreshRecovery(Player,0);check(!HUD->RecoveryBox->IsVisible());
  Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);HUD->RefreshRecovery(Player,0);check(HUD->RecoveryBox->IsVisible() && HUD->RecoveryBar->GetPercent()==1);HUD->RefreshRecovery(Player,.3f);check(!HUD->RecoveryBox->IsVisible());
  UE_LOG(LogTemp,Display,TEXT("RECOVERY TEST PASS full pulse only after actual unlock; fades out; no gameplay state modified by HUD"));
  HUD->RecoveryMontage.Reset();HUD->RefreshRecovery(Player,0);check(!HUD->RecoveryBox->IsVisible());HUD->RecoveryMontage=M;
  if(!Preview.IsEmpty())
  {
   HUD->Health=68;HUD->MaxHealth=100;HUD->Full=LoadObject<UTexture2D>(nullptr,TEXT("/Game/2DArt/UI/Action/UI_SaHealth_Filled"));HUD->Empty=LoadObject<UTexture2D>(nullptr,TEXT("/Game/2DArt/UI/Action/UI_SaHealth_Empty"));
   CastChecked<UTextBlock>(HUD->GetWidgetFromName(TEXT("DodgeKey")))->SetText(FText::FromString(TEXT("Space")));CastChecked<UTextBlock>(HUD->GetWidgetFromName(TEXT("PunchKey")))->SetText(FText::FromString(TEXT("LMB")));HUD->ShowRecovery(.55f,1);
   auto Root=SNew(SOverlay)+SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.025,.035,.045,1))]+SOverlay::Slot()[HUD->TakeWidget()];
   if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();FWidgetRenderer Renderer(true);auto* Target=Renderer.DrawWidget(Root,FVector2D(1920,1080));FlushRenderingCommands();check(Target && Target->GameThread_GetRenderTargetResource());TArray<FColor> Pixels;check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1920,1080,Pixels,PNG);check(FFileHelper::SaveArrayToFile(PNG,*Preview));
  }
  HUD->NativeDestruct();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);
 }
};
UVTGRecoveryHUDCommandlet::UVTGRecoveryHUDCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGRecoveryHUDCommandlet::Main(const FString& Params){FString Preview;FParse::Value(*Params,TEXT("Preview="),Preview);FVTGRecoveryHUDRegression::Run(Preview);UE_LOG(LogTemp,Display,TEXT("RECOVERY_HUD_SUCCESS"));return 0;}








