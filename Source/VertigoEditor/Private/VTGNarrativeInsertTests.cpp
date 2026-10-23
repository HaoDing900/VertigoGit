#include "VTGNarrativeInsert.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NarrativeEvent.h"
#include "UObject/UnrealType.h"

bool VTGTestNarrativeInsert()
{
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("NarrativeInsertTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Sa/BP_Cine_Sa"));
    auto* Talk=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L2Bar/ANM/Sa/CANM_Sa_SittingOnBarChair_Talk_casc_Montage"));
    auto* Negative=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L2Bar/ANM/Sa/CANM_Sa_SittingOnBarChair_Negative_casc_Montage"));
    if (!BP || !Talk || !Negative) return false;
    auto* Actor=W->SpawnActor<ACharacter>();
    Actor->Tags.Add(TEXT("SaC"));
    auto* Mesh=Actor->GetMesh();
    Mesh->SetSkeletalMesh(CastChecked<ACharacter>(BP->GeneratedClass->GetDefaultObject())->GetMesh()->GetSkeletalMeshAsset());
    Mesh->SetAnimInstanceClass(UAnimInstance::StaticClass()); Mesh->InitAnim(true);
    auto* Anim=Mesh->GetAnimInstance();
    auto* Base=DuplicateObject<UAnimMontage>(Talk,GetTransientPackage());
    auto* First=DuplicateObject<UAnimMontage>(Talk,GetTransientPackage());
    auto* Last=DuplicateObject<UAnimMontage>(Negative,GetTransientPackage());
    Base->bEnableAutoBlendOut=false; First->bEnableAutoBlendOut=false; Last->bEnableAutoBlendOut=true;
    int32 Fail=0;
    auto Expect=[&](bool OK,const TCHAR* Message){if(!OK)++Fail; UE_LOG(LogTemp,Display,TEXT("INSERT %s: %s"),OK?TEXT("PASS"):TEXT("FAIL"),Message);};
    auto Frames=[&](int32 Count){for(int32 I=0;I<Count;++I){Mesh->TickAnimation(1.f/30.f,false);Mesh->RefreshBoneTransforms();}};
    auto* EventBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/Events/Montage/NE_PlayMontage_InterruptRes"));
    auto Trigger=[&](UAnimMontage* Clip, bool Instant=false)
    {
        auto* Event=NewObject<UNarrativeEvent>(Actor,EventBP->GeneratedClass);
        FindFProperty<FNameProperty>(Event->GetClass(),TEXT("TargetIdentity"))->SetPropertyValue_InContainer(Event,TEXT("SaC"));
        FindFProperty<FObjectPropertyBase>(Event->GetClass(),TEXT("InterruptMontage"))->SetObjectPropertyValue_InContainer(Event,Clip);
        FindFProperty<FBoolProperty>(Event->GetClass(),TEXT("Instant"))->SetPropertyValue_InContainer(Event,Instant);
        Event->ExecuteEvent(Actor,nullptr,nullptr);
    };
    Anim->Montage_Play(Base,.8f); Anim->Montage_SetPosition(Base,.4f);
    Trigger(First);
    Expect(Anim->Montage_IsPlaying(First),TEXT("first insert starts over base"));
    Frames(150);
    Expect(Anim->Montage_IsActive(First),TEXT("auto blend out off holds old insert"));
    Trigger(Last);
    Expect(Anim->Montage_IsPlaying(Last),TEXT("new event replaces held insert"));
    Frames(15);
    Expect(Actor->ActorHasTag(TEXT("VTG.Narrative.Insert.Active")),TEXT("old delayed callback does not clear new ownership"));
    Trigger(Last);
    Expect(Anim->Montage_IsPlaying(Last) && Anim->Montage_GetPosition(Last)<.05f,TEXT("same montage can restart"));
    Frames(410);
    Expect(Anim->Montage_IsActive(Base) && !Anim->Montage_IsActive(First),TEXT("final completion restores original base, not previous insert"));
    Expect(FMath::IsNearlyEqual(Anim->Montage_GetPlayRate(Base),.8f),TEXT("base playback rate preserved"));
    Expect(!Actor->ActorHasTag(TEXT("VTG.Narrative.Insert.Active")),TEXT("final completion clears ownership"));
    Trigger(First);
    Anim->Montage_Stop(0.f,First); Frames(2);
    Expect(!Actor->ActorHasTag(TEXT("VTG.Narrative.Insert.Active")),TEXT("external interruption clears ownership"));
    const float AuthoredBlend=Last->BlendIn.GetBlendTime();
    Anim->Montage_Play(Base,.8f); Anim->Montage_SetPosition(Base,.4f); Frames(15);
    Trigger(Last,true);
    auto* InstantInstance=Anim->GetActiveInstanceForMontage(Last);
    Expect(InstantInstance && FMath::IsNearlyEqual(InstantInstance->GetWeight(),1.f),TEXT("Instant insert reaches full weight inside the event call"));
    Expect(!Anim->Montage_IsActive(Base),TEXT("Instant insert removes the old same-group montage immediately"));
    Trigger(Last,true);
    Expect(Anim->Montage_IsPlaying(Last) && Actor->ActorHasTag(TEXT("VTG.Narrative.Insert.Active")),TEXT("Instant same-asset replacement retains insert ownership"));
    Frames(410);
    Expect(Anim->Montage_IsActive(Base) && FMath::IsNearlyEqual(Anim->Montage_GetPlayRate(Base),.8f),TEXT("Instant insert still restores the original base and rate"));
    Expect(FMath::IsNearlyEqual(Last->BlendIn.GetBlendTime(),AuthoredBlend),TEXT("Instant does not modify authored montage blend settings"));
    auto* StartBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/Events/Montage/NE_PlayMontage_Start"));
    auto* StartEvent=NewObject<UNarrativeEvent>(Actor,StartBP->GeneratedClass);
    FindFProperty<FNameProperty>(StartEvent->GetClass(),TEXT("TargetIdentity"))->SetPropertyValue_InContainer(StartEvent,TEXT("SaC"));
    FindFProperty<FObjectPropertyBase>(StartEvent->GetClass(),TEXT("TargetMontage"))->SetObjectPropertyValue_InContainer(StartEvent,Last);
    auto* InstantProperty=FindFProperty<FBoolProperty>(StartEvent->GetClass(),TEXT("Instant"));
    Expect(InstantProperty && !InstantProperty->GetPropertyValue_InContainer(StartEvent),TEXT("Start Instant defaults off for existing events"));
    InstantProperty->SetPropertyValue_InContainer(StartEvent,true);
    StartEvent->ExecuteEvent(Actor,nullptr,nullptr);
    InstantInstance=Anim->GetActiveInstanceForMontage(Last);
    Expect(InstantInstance && FMath::IsNearlyEqual(InstantInstance->GetWeight(),1.f) && !Anim->Montage_IsActive(Base),TEXT("actual Start event hard-cuts into the selected montage"));
    Expect(FMath::IsNearlyEqual(Last->BlendIn.GetBlendTime(),AuthoredBlend),TEXT("Start leaves the montage asset unchanged"));
    W->DestroyWorld(false); GEngine->DestroyWorldContext(W);
    return Fail==0;
}
