#include "VTGMailReadingComponent.h"
#include "UI/VTGSequenceDialogue.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "NarrativeComponent.h"
#include "Dialogue.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

bool VTGVerifyMailSequencePolish()
{
    auto Expect=[](bool OK,const TCHAR* Text){checkf(OK,TEXT("%s"),Text);UE_LOG(LogTemp,Display,TEXT("MAIL_SEQUENCE_TEST PASS %s"),Text);};
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("MailSequencePolishTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    auto* Pawn=W->SpawnActor<APawn>();auto* PC=W->SpawnActor<APlayerController>();PC->Possess(Pawn);
    if(!PC->PlayerCameraManager){FActorSpawnParameters Spawn;Spawn.Owner=PC;PC->PlayerCameraManager=W->SpawnActor<APlayerCameraManager>(APlayerCameraManager::StaticClass(),Spawn);PC->PlayerCameraManager->InitializeFor(PC);}
    auto* Follow=NewObject<UCameraComponent>(Pawn,TEXT("Player_FollowCamera"));Pawn->AddInstanceComponent(Follow);Follow->RegisterComponent();Follow->SetActive(true);
    auto* Fixed=W->SpawnActor<AActor>();auto* Other=W->SpawnActor<AActor>();
    PC->SetViewTarget(Pawn);
    Expect(UVTGMailReadingComponent::GetMailCameraBlendTime(Pawn,Pawn,true)==1.f,TEXT("Third-person to mail keeps the existing one-second blend"));
    UVTGMailReadingComponent::CaptureMailCameraReturn(Pawn);Follow->SetActive(false);
    Expect(UVTGMailReadingComponent::GetMailCameraBlendTime(Pawn,Pawn,false)==1.f,TEXT("Mail to captured third-person keeps its blend even while Follow is inactive"));
    Expect(UVTGMailReadingComponent::GetMailCameraBlendTime(Pawn,Fixed,false)==0,TEXT("Mail to fixed camera cuts immediately"));
    PC->SetViewTarget(Fixed);Follow->SetActive(true);UVTGMailReadingComponent::CaptureMailCameraReturn(Pawn);
    Expect(UVTGMailReadingComponent::GetMailCameraBlendTime(Pawn,Pawn,true)==0 && UVTGMailReadingComponent::GetMailCameraBlendTime(Pawn,Pawn,false)==0,TEXT("External camera entry and exit both cut, regardless of Follow active flag"));
    PC->SetViewTarget(Pawn);PC->SetViewTargetWithBlend(Other,1.f);
    Expect(UVTGMailReadingComponent::GetMailCameraBlendTime(Pawn,Pawn,true)==0,TEXT("Pending external camera takeover never receives a mail blend"));
    auto* N=NewObject<UNarrativeComponent>(Pawn);Pawn->AddInstanceComponent(N);N->RegisterComponent();
    auto* Current=FindFProperty<FObjectPropertyBase>(N->GetClass(),TEXT("CurrentDialogue"));check(Current);
    auto Begin=[&](){auto* D=NewObject<UDialogue>(N);D->bCanBeExited=false;D->OwningComp=N;Current->SetObjectPropertyValue_InContainer(N,D);return D;};
    auto* D=Begin();N->TryExitDialogue();Expect(N->GetCurrentDialogue()==D,TEXT("Normal exit continues to respect Can Be Exited=false"));
    N->ForceExitDialogue();Expect(!N->GetCurrentDialogue() && !D->OwningComp,TEXT("Force Exit bypasses permission and runs real dialogue deinitialization"));
    N->ForceExitDialogue();Expect(!N->GetCurrentDialogue(),TEXT("Force Exit without dialogue is harmless"));
    auto* Seq=NewObject<ULevelSequence>(W);Seq->Initialize();Seq->GetMovieScene()->SetPlaybackRange(0,240000);
    ALevelSequenceActor* Actor=nullptr;auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(W,Seq,FMovieSceneSequencePlaybackSettings(),Actor);check(Player);
    Expect(Actor->FindComponentByClass<UVTGSequenceDialogueGate>() && !UVTGSequenceDialogueGate::ShouldEndDialogue(Actor),TEXT("Runtime-created sequence automatically has a default-OFF entry gate before Play"));
    D=Begin();Player->Play();Expect(N->GetCurrentDialogue()==D,TEXT("Default-OFF sequence preserves active dialogue"));Player->Stop();
    Actor->Tags.Add(UVTGSequenceDialogueGate::EndDialogueTag);
    Begin();Player->Play();Expect(!N->GetCurrentDialogue(),TEXT("Starting Sequence force exits the active non-exitable dialogue"));
    D=Begin();Player->Pause();Player->Play();Expect(N->GetCurrentDialogue()==D,TEXT("New dialogue may run during Sequence; pause/resume does not repeatedly exit it"));
    Player->Stop();Player->Play();Expect(!N->GetCurrentDialogue(),TEXT("Fresh playback after Stop applies the entry rule again"));
    Player->Stop();Actor->Tags.Add(UVTGSequenceDialogueGate::KeepDialogueTag);D=Begin();Player->Play();
    Expect(N->GetCurrentDialogue()==D,TEXT("Per-instance OFF switch preserves active dialogue"));
    Player->Stop();Actor->Tags.Remove(UVTGSequenceDialogueGate::KeepDialogueTag);Actor->Tags.Add(TEXT("Narrative.DialogueShot"));Player->Play();
    Expect(N->GetCurrentDialogue()==D,TEXT("Narrative's own camera shot does not interrupt its parent dialogue"));
    Player->Stop();N->ForceExitDialogue();
    Actor->Tags.Remove(TEXT("Narrative.DialogueShot"));
    // Asset defaults must survive CreateLevelSequencePlayer, unlike tags on a placed actor.
    auto* AssetDefault=NewObject<ULevelSequence>(W);AssetDefault->Initialize();AssetDefault->GetMovieScene()->SetPlaybackRange(0,240000);
    Expect(!UVTGSequenceDialogueGate::GetSequenceDefault(AssetDefault),TEXT("Asset entry default is OFF"));
    UVTGSequenceDialogueGate::SetSequenceDefault(AssetDefault,true);
    Player=ULevelSequencePlayer::CreateLevelSequencePlayer(W,AssetDefault,FMovieSceneSequencePlaybackSettings(),Actor);check(Player);
    Expect(Actor->Tags.IsEmpty() && UVTGSequenceDialogueGate::ShouldEndDialogue(Actor),TEXT("Runtime-created actor inherits the Sequence entry default without instance tags"));
    Begin();Player->Play();Expect(!N->GetCurrentDialogue(),TEXT("Asset-level opt-in ends non-exitable dialogue"));Player->Stop();
    Actor->Tags.Add(UVTGSequenceDialogueGate::KeepDialogueTag);D=Begin();Player->Play();
    Expect(N->GetCurrentDialogue()==D,TEXT("Instance OFF overrides enabled asset default"));Player->Stop();N->ForceExitDialogue();
    Actor->Tags.Remove(UVTGSequenceDialogueGate::KeepDialogueTag);Actor->Tags.Add(TEXT("Narrative.DialogueShot"));D=Begin();Player->Play();
    Expect(N->GetCurrentDialogue()==D,TEXT("Asset entry default preserves Narrative-owned shot protection"));Player->Stop();N->ForceExitDialogue();
    Actor->Tags.Remove(TEXT("Narrative.DialogueShot"));UVTGSequenceDialogueGate::SetSequenceDefault(AssetDefault,false);
    Expect(!UVTGSequenceDialogueGate::ShouldEndDialogue(Actor),TEXT("Disabling asset default is reflected in existing players"));
    // Fresh player ensures Play actually evaluates frame zero (a resumed player may already have evaluated it).
    Player=ULevelSequencePlayer::CreateLevelSequencePlayer(W,Seq,FMovieSceneSequencePlaybackSettings(),Actor);check(Player);
    Actor->Tags.Add(UVTGSequenceDialogueGate::EndDialogueTag);
    UDialogue* EntryDialogue=Begin();UDialogue* FirstFrameDialogue=nullptr;bool HadExitedBeforeFirstFrame=false;
    auto UpdateHandle=Player->OnSequenceUpdated().AddLambda([&](const UMovieSceneSequencePlayer&,FFrameTime,FFrameTime)
    {
        if(!Player->IsPlaying() || FirstFrameDialogue)return;
        HadExitedBeforeFirstFrame=N->GetCurrentDialogue()!=EntryDialogue;
        FirstFrameDialogue=Begin();
    });
    Player->Play();Player->OnSequenceUpdated().Remove(UpdateHandle);
    UE_LOG(LogTemp,Display,TEXT("MAIL_SEQUENCE_FIRST_FRAME before=%d new=%d survives=%d"),HadExitedBeforeFirstFrame,FirstFrameDialogue!=nullptr,N->GetCurrentDialogue()==FirstFrameDialogue);
    Expect(HadExitedBeforeFirstFrame && FirstFrameDialogue && N->GetCurrentDialogue()==FirstFrameDialogue,TEXT("Old dialogue exits before first evaluation; a new dialogue opened on the first frame survives OnPlay"));
    Player->Stop();N->ForceExitDialogue();
    // Use the actual player's reflected unread flag to exercise duplicate-notification suppression.
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(BP);
    FActorSpawnParameters Spawn;Spawn.bDeferConstruction=true;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Sa=W->SpawnActor<AActor>(BP->GeneratedClass,FTransform::Identity,Spawn);check(Sa);
    auto* Unread=FindFProperty<FBoolProperty>(Sa->GetClass(),TEXT("HasNewMail"));check(Unread);
    auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/SFX/Character/Ting_Coins.Ting_Coins"));check(Sound);
    Unread->SetPropertyValue_InContainer(Sa,false);
    Expect(UVTGMailReadingComponent::NotifyNewMail(Sa,TEXT("MailA"),true,Sound),TEXT("First new mail requests Ting_Coins once"));
    Unread->SetPropertyValue_InContainer(Sa,true);
    Expect(!UVTGMailReadingComponent::NotifyNewMail(Sa,TEXT("MailA"),true,Sound),TEXT("Repeated update of same unread mail stays silent"));
    Expect(UVTGMailReadingComponent::NotifyNewMail(Sa,TEXT("MailB"),true,Sound),TEXT("A different new mail still notifies while another is unread"));
    Expect(!UVTGMailReadingComponent::NotifyNewMail(Sa,TEXT("MailB"),false,Sound),TEXT("Clearing or restoring read mail does not notify"));
    Unread->SetPropertyValue_InContainer(Sa,false);
    Expect(UVTGMailReadingComponent::NotifyNewMail(Sa,TEXT("MailB"),true,Sound),TEXT("A fresh unread transition can notify again"));
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
