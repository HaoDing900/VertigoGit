#include "VTGClinicFix.h"
#include "VTGNarrativeInsert.h"
#include "NarrativeComponent.h"
#include "Dialogue.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"
#include "Components/Overlay.h"
#include "Components/RichTextBlock.h"
#include "Components/Image.h"
#include "Slate/WidgetRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Widgets/SBoxPanel.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "NarrativeEvent.h"
#include "UObject/UnrealType.h"
#include "GameFramework/PlayerController.h"
#include "Quests/VTGEnsureQuestStateEvent.h"
#include "Quest.h"
#include "QuestSM.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Camera/PlayerCameraManager.h"

bool VTGClinicRuntimeTest()
{
 FEditorScriptExecutionGuard ScriptGuard;
 if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* GI=NewObject<UGameInstance>();W->SetGameInstance(GI);GEngine->GetWorldContextFromWorldChecked(W).OwningGameInstance=GI;
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(BP);
 auto* Sa=W->SpawnActor<ACharacter>(BP->GeneratedClass);check(Sa);
 auto* PC=W->SpawnActor<APlayerController>();W->AddController(PC);auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,0);PC->SetPlayer(Local);PC->Possess(Sa);
 if(!PC->PlayerCameraManager){PC->PlayerCameraManager=W->SpawnActor<APlayerCameraManager>();PC->PlayerCameraManager->InitializeFor(PC);}
 auto* Mesh=Sa->GetMesh();Mesh->SetAnimInstanceClass(UAnimInstance::StaticClass());Mesh->InitAnim(true);auto* Anim=Mesh->GetAnimInstance();check(Anim);
 auto* NC=Sa->FindComponentByClass<UNarrativeComponent>();check(NC);
 auto* D=NewObject<UDialogue>(W);D->OwningComp=NC;NC->CurrentDialogue=D;
 auto* Context=NewObject<UVTGClinicButtonEvent>(D);
 auto* Authored=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Characters/Sa/Anm/EmoteNGestures/ANM_Sa_Emote_TalkGesture_Montage"));check(Authored);
 auto* Clip=DuplicateObject<UAnimMontage>(Authored,GetTransientPackage());Clip->bEnableAutoBlendOut=false;
 auto Frames=[&](int32 N){for(int32 I=0;I<N;++I){++GFrameCounter;W->GetTimerManager().Tick(1.f/60.f);Mesh->TickAnimation(1.f/60.f,false);Mesh->RefreshBoneTransforms();}};
 UVTGNarrativeInsertLibrary::PlayNarrativeMontage(Sa,Clip,1.f,NAME_None,true,Context);
 check(Sa->FindComponentByClass<UVTGNarrativeMontageCleanup>());
 NC->OnDialogueFinished.Broadcast(D,false);Frames(40);check(Anim->Montage_IsActive(Clip));Frames(30);check(!Anim->Montage_IsActive(Clip));
 UE_LOG(LogTemp,Display,TEXT("CLINIC TEST PASS: Start montage survives grace period, blends away by 1.17s"));
 UVTGNarrativeInsertLibrary::PlayNarrativeMontage(Sa,Clip,1.f,NAME_None,true,Context);
 NC->OnDialogueFinished.Broadcast(D,false);Frames(10);
 Anim->Montage_Play(Clip);const int32 GameplayID=Anim->GetActiveInstanceForMontage(Clip)->GetInstanceID();Frames(60);
 check(Anim->GetActiveInstanceForMontage(Clip) && Anim->GetActiveInstanceForMontage(Clip)->GetInstanceID()==GameplayID);
 UE_LOG(LogTemp,Display,TEXT("CLINIC TEST PASS: newer gameplay instance of same montage is untouched"));
 UVTGNarrativeInsertLibrary::PlayNarrativeInsert(Context,TEXT("SaP"),Clip,true);
 NC->OnDialogueFinished.Broadcast(D,false);Frames(70);check(!Anim->Montage_IsActive(Clip));check(!Sa->ActorHasTag(TEXT("VTG.Narrative.Insert.Active")));
 UE_LOG(LogTemp,Display,TEXT("CLINIC TEST PASS: interrupt montage is removed without resurrecting its base"));
 UVTGNarrativeInsertLibrary::PlayNarrativeMontage(Sa,Clip,1.f,NAME_None,true,Context);NC->OnDialogueFinished.Broadcast(D,false);Frames(10);
 auto* D2=NewObject<UDialogue>(W);D2->OwningComp=NC;NC->CurrentDialogue=D2;auto* C2=NewObject<UVTGClinicButtonEvent>(D2);
 UVTGNarrativeInsertLibrary::PlayNarrativeMontage(Sa,Clip,1.f,NAME_None,true,C2);Frames(70);check(Anim->Montage_IsActive(Clip));
 NC->OnDialogueFinished.Broadcast(D2,false);Frames(70);check(!Anim->Montage_IsActive(Clip));
 UE_LOG(LogTemp,Display,TEXT("CLINIC TEST PASS: consecutive dialogue takes ownership and cancels old cleanup timer"));
 for(bool Interrupt:{false,true})
 {
  NC->CurrentDialogue=D;
  auto* EC=LoadClass<UNarrativeEvent>(nullptr,Interrupt?TEXT("/Game/Narrative/Events/Montage/NE_PlayMontage_InterruptRes.NE_PlayMontage_InterruptRes_C"):TEXT("/Game/Narrative/Events/Montage/NE_PlayMontage_Start.NE_PlayMontage_Start_C"));check(EC);
  auto* E=NewObject<UNarrativeEvent>(D,EC);
  FindFProperty<FNameProperty>(EC,TEXT("TargetIdentity"))->SetPropertyValue_InContainer(E,TEXT("SaP"));
  FindFProperty<FObjectPropertyBase>(EC,Interrupt?TEXT("InterruptMontage"):TEXT("TargetMontage"))->SetObjectPropertyValue_InContainer(E,Clip);
  FindFProperty<FBoolProperty>(EC,TEXT("Instant"))->SetPropertyValue_InContainer(E,true);
  E->ExecuteEvent(Sa,nullptr,NC);check(Anim->Montage_IsActive(Clip));NC->OnDialogueFinished.Broadcast(D,false);Frames(70);check(!Anim->Montage_IsActive(Clip));
  UE_LOG(LogTemp,Display,TEXT("CLINIC TEST PASS: actual %s Blueprint event cleans player montage on dialogue end"),Interrupt?TEXT("Interrupt"):TEXT("Start"));
 }
 auto* ManagerClass=LoadClass<AActor>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L5Clinic/BPLM_Clinic.BPLM_Clinic_C"));check(ManagerClass);
 auto* Manager=W->SpawnActor<AActor>(ManagerClass);check(Manager);
 auto* ButtonClass=LoadClass<AActor>(nullptr,TEXT("/Game/Blueprints/BP_0ISXInteractiveCollision.BP_0ISXInteractiveCollision_C"));check(ButtonClass);
 auto* Button=W->SpawnActor<AActor>(ButtonClass);check(Button);
 FindFProperty<FObjectPropertyBase>(ManagerClass,TEXT("ISXColi_L5Button"))->SetObjectPropertyValue_InContainer(Manager,Button);
 auto* CanInteract=FindFProperty<FBoolProperty>(ButtonClass,TEXT("CanInteract?"));check(CanInteract);CanInteract->SetPropertyValue_InContainer(Button,false);
 auto* ButtonEvent=NewObject<UVTGClinicButtonEvent>(D);ButtonEvent->ExecuteEvent(Sa,nullptr,NC);check(CanInteract->GetPropertyValue_InContainer(Button));
 auto* QEvent=NewObject<UVTGEnsureQuestStateEvent>(D);QEvent->Quest=LoadClass<UQuest>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L5Clinic/Q_L5Clinic.Q_L5Clinic_C"));QEvent->StateID=TEXT("QuestState_11");QEvent->ExecuteEvent(Sa,nullptr,NC);
 auto* Quest=NC->GetQuestInstance(QEvent->Quest);check(Quest && Quest->GetCurrentState() && Quest->GetCurrentState()->GetID()==TEXT("QuestState_11"));
 bool Has78=false;for(auto* Branch:Quest->GetCurrentState()->Branches)if(Branch->GetID()==TEXT("QuestBranch_78"))Has78=true;check(Has78);
 UE_LOG(LogTemp,Display,TEXT("CLINIC TEST PASS: disabled real button becomes interactable; runtime quest activates state 11 with branch 78"));
 auto* CameraClass=LoadClass<AActor>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/FixedCamera/BP_0FixedCamera.BP_0FixedCamera_C"));check(CameraClass);
 auto* Camera=W->SpawnActor<AActor>(CameraClass);check(Camera);
 FindFProperty<FObjectPropertyBase>(ManagerClass,TEXT("FixCam_Lobby"))->SetObjectPropertyValue_InContainer(Manager,Camera);
 auto MakeCameraDialogue=[&](){auto* C=NewObject<UDialogue>(W);C->OwningComp=NC;C->OwningPawn=Sa;C->OwningController=PC;C->bFreeMovement=true;NC->CurrentDialogue=C;return C;};
 for(float Blend:{0.f,.15f})
 {
  Camera->SetActorTickEnabled(true);PC->SetViewTarget(Camera);auto* C=MakeCameraDialogue();C->DialogueBlendOutTime=Blend;C->CaptureViewTargetTickState();Camera->SetActorTickEnabled(false);C->Deinitialize();check(Camera->IsActorTickEnabled());
  Camera->SetActorTickEnabled(false);PC->SetViewTarget(Camera);C=MakeCameraDialogue();C->DialogueBlendOutTime=Blend;C->CaptureViewTargetTickState();Camera->SetActorTickEnabled(true);C->Deinitialize();check(!Camera->IsActorTickEnabled());
 }
 UE_LOG(LogTemp,Display,TEXT("CAMERA TEST PASS: Narrative restores both initially enabled and initially disabled Tick with zero and nonzero blend"));
 Camera->SetActorTickEnabled(true);PC->SetViewTarget(Camera);auto* First=MakeCameraDialogue();First->CaptureViewTargetTickState();Camera->SetActorTickEnabled(false);First->Deinitialize();
 auto* Shot=W->SpawnActor<AActor>();PC->SetViewTarget(Shot);auto* Next=MakeCameraDialogue();Next->InheritViewTargetTickState(First);Next->CaptureViewTargetTickState();Camera->SetActorTickEnabled(false);First->Deinitialize();check(!Camera->IsActorTickEnabled());Frames(90);check(!Camera->IsActorTickEnabled());Next->Deinitialize();check(Camera->IsActorTickEnabled());
 UE_LOG(LogTemp,Display,TEXT("CAMERA TEST PASS: replacement inherits original camera state; repeated teardown and old timers cannot override it"));
 PC->SetViewTarget(Camera);auto* Destroyed=MakeCameraDialogue();Destroyed->CaptureViewTargetTickState();W->DestroyActor(Camera);Destroyed->Deinitialize();NC->CurrentDialogue=nullptr;
 UE_LOG(LogTemp,Display,TEXT("CAMERA TEST PASS: destroyed previous view target is skipped safely"));
 GI->RemoveLocalPlayer(Local);Local->PlayerController=nullptr;W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}

void VTGClinicOverlayPreview()
{
 if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
 auto* Class=LoadClass<UUserWidget>(nullptr,TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay.BP_Narrative3Overlay_C"));check(Class);
 auto* UI=NewObject<UUserWidget>(GetTransientPackage(),Class);UI->Initialize();
 auto* Text=CastChecked<URichTextBlock>(UI->GetWidgetFromName(TEXT("Line_Side")));
 auto* Image=CastChecked<UImage>(UI->GetWidgetFromName(TEXT("DialogueBox_Side")));
 Text->SetVisibility(ESlateVisibility::Visible);Image->SetVisibility(ESlateVisibility::Visible);
 auto* Panel=CastChecked<UOverlay>(Text->GetParent());
 const TArray<FString> Lines={TEXT("帮我按一下门旁边的按钮。"),TEXT("角落有个按钮，就在门旁边的墙上。帮我按一下就能启动机器了。检查之前请先把随身物品放好，我们随后就开始。"),TEXT("Please press the button beside the door. The examination can begin once the machine is ready. This longer line must remain inside its background as it wraps.")};
 for(int32 I=0;I<Lines.Num();++I)
 {
  Text->SetText(FText::FromString(Lines[I]));
  auto Root=SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Panel->TakeWidget()];
  FWidgetRenderer Renderer(true);auto* Target=Renderer.DrawWidget(Root,FVector2D(420,420));FlushRenderingCommands();
  Renderer.DrawWidget(Target,Root,FVector2D(420,420),.016f);FlushRenderingCommands();
  auto TG=Text->GetCachedGeometry();auto BG=Image->GetCachedGeometry();
  const auto Min=BG.AbsoluteToLocal(TG.GetAbsolutePosition());const auto Max=BG.AbsoluteToLocal(TG.LocalToAbsolute(TG.GetLocalSize()));
  check(Min.X>=23 && Min.Y>=15 && Max.X<=BG.GetLocalSize().X-23 && Max.Y<=BG.GetLocalSize().Y-15);
  TArray<FColor> Pixels;check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(420,420,Pixels,PNG);
  const FString File=FPaths::ProjectDir()/TEXT("Scripts/ClinicFix")/FString::Printf(TEXT("side-preview-%d.png"),I);check(FFileHelper::SaveArrayToFile(PNG,*File));
  UE_LOG(LogTemp,Display,TEXT("CLINIC PREVIEW PASS %d: text=(%.0f,%.0f) background=(%.0f,%.0f)"),I,TG.GetLocalSize().X,TG.GetLocalSize().Y,BG.GetLocalSize().X,BG.GetLocalSize().Y);
 }
}
