#include "Audio/VTGSpatialLoopComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "NarrativeComponent.h"
#include "Dialogue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "UObject/UnrealType.h"

bool VTGVerifySpatialAudio()
{
    auto Expect=[](bool OK,const TCHAR* Message){checkf(OK,TEXT("%s"),Message);UE_LOG(LogTemp,Display,TEXT("SPATIAL PASS %s"),Message);};
    auto* Station=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L3PoliceStation/BPLM_PoliceStation"));
    auto* Drone=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Drone_IPHA/BP_Drone_IPHA_NPC"));
    auto* Anim=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L3PoliceStation/ANM/Bello/ANM_Bello_FixCar1"));
    auto* Repair=LoadObject<USoundWave>(nullptr,TEXT("/Game/SFX/0InLevel/Station/METLMvmt___Tinkering_Antique_Lock_344_Audio_Antique_Small_Metals"));
    auto* Hum=LoadObject<USoundWave>(nullptr,TEXT("/Game/SFX/IPHA/Electric_Hum_Loop_E"));
    check(Station && Drone && Anim && Repair && Hum);
    for(auto* BP:{Station,Drone}) {FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);Expect(Log.NumErrors==0,TEXT("Affected Blueprint compiles"));}
    Expect(!Anim->Notifies.ContainsByPredicate([&](const FAnimNotifyEvent& E){auto* N=Cast<UAnimNotify_PlaySound>(E.Notify);return N && N->Sound==Repair;}),TEXT("Repair sound removed from animation"));
    auto* Graph=FBlueprintEditorUtils::FindEventGraph(Station);
    UK2Node_CustomEvent* Idle=nullptr; UK2Node_CallFunction* Setup=nullptr; int32 Pauses=0;
    for(UEdGraphNode* Node:Graph->Nodes)
    {
        if(auto* E=Cast<UK2Node_CustomEvent>(Node)) if(E->CustomFunctionName==TEXT("L3_SetBelloIdle")) Idle=E;
        if(auto* C=Cast<UK2Node_CallFunction>(Node))
        {
            if(C->FunctionReference.GetMemberName()==TEXT("SetBelloRepairLoop")) Setup=C;
            if(C->FunctionReference.GetMemberName()==TEXT("SuspendBelloRepairForDialogue"))
            {
                auto* Exec=C->FindPinChecked(TEXT("execute"));check(Exec->LinkedTo.Num()==1);
                auto* Begin=CastChecked<UK2Node_CallFunction>(Exec->LinkedTo[0]->GetOwningNode());
                Expect(Begin->FunctionReference.GetMemberName()==TEXT("BeginDialogue"),TEXT("Bello dialogue mute follows actual BeginDialogue"));
                ++Pauses;
            }
        }
    }
    Expect(Idle && Setup && Idle->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Setup->FindPinChecked(TEXT("execute"))) && Pauses==2,TEXT("Bello idle and both dialogue routes wired"));
    auto* Helped=CastChecked<UK2Node_VariableGet>(Setup->FindPinChecked(TEXT("Helped"))->LinkedTo[0]->GetOwningNode());
    Expect(Helped->VariableReference.GetMemberName()==TEXT("L3_Bello_Helped"),TEXT("Bello loop uses the real helped flag"));
    UVTGSpatialLoopComponent* Template=nullptr;
    for(auto* N:Drone->SimpleConstructionScript->GetAllNodes()) if(N->GetVariableName()==TEXT("ElectricHum")) Template=CastChecked<UVTGSpatialLoopComponent>(N->ComponentTemplate);
    Expect(Template && Template->Sound==Hum && Template->bReactToMovement && Template->bLoopEnabled,TEXT("Drone inherits configured speed-reactive hum"));
    auto* BelloAtt=CastChecked<USoundAttenuation>(Setup->FindPinChecked(TEXT("Attenuation"))->DefaultObject);
    for(auto* Att:{BelloAtt,Template->Attenuation.Get()}) Expect(Att && Att->Attenuation.bSpatialize && Att->Attenuation.bAttenuate && Att->Attenuation.FalloffDistance>0,TEXT("Local spatial attenuation configured"));
    const bool OriginalLoop=Hum->bLooping;
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("SpatialAudioTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    auto* Owner=W->SpawnActor<AActor>();
    auto* Root=NewObject<USceneComponent>(Owner);Owner->SetRootComponent(Root);Root->RegisterComponent();
    UVTGSpatialAudioLibrary::SetBelloRepairLoop(Owner,false,Repair,BelloAtt);
    auto* Loop=Owner->FindComponentByClass<UVTGSpatialLoopComponent>();check(Loop); Loop->BeginPlay();
    auto* FirstAudio=Loop->Audio.Get();
    UVTGSpatialAudioLibrary::SetBelloRepairLoop(Owner,false,Repair,BelloAtt);
    Expect(Loop->bLoopEnabled && Loop->Audio==FirstAudio && Loop->Audio->GetAttachParent()==Root,TEXT("Bello loop follows actor and repeated idle calls do not stack components"));
    auto* Narrative=NewObject<UNarrativeComponent>(Owner);
    auto* Dialogue=NewObject<UDialogue>(Narrative);
    auto* Other=NewObject<UDialogue>(Narrative);
    FindFProperty<FObjectPropertyBase>(Narrative->GetClass(),TEXT("CurrentDialogue"))->SetObjectPropertyValue_InContainer(Narrative,Dialogue);
    Loop->SuspendForDialogue(Narrative);
    Expect(Loop->MutedDialogue==Dialogue,TEXT("Bello loop suspends for exact dialogue instance"));
    Narrative->OnDialogueFinished.Broadcast(Other,false);
    Expect(Loop->MutedDialogue==Dialogue,TEXT("Unrelated dialogue cannot resume repair sound"));
    Narrative->OnDialogueFinished.Broadcast(Dialogue,false);
    Expect(!Loop->MutedDialogue.IsValid() && Loop->bLoopEnabled,TEXT("Unhelped Bello resumes after dialogue"));
    Loop->SuspendForDialogue(Narrative);
    UVTGSpatialAudioLibrary::SetBelloRepairLoop(Owner,true,Repair,BelloAtt);
    Narrative->OnDialogueFinished.Broadcast(Dialogue,false);
    Expect(!Loop->bLoopEnabled,TEXT("Helped Bello stays silent when dialogue ends"));
    auto* Moving=NewObject<UVTGSpatialLoopComponent>(Owner);
    Moving->Sound=Hum;Moving->Attenuation=Template->Attenuation;Moving->bReactToMovement=true;
    Moving->RegisterComponent();Moving->BeginPlay();
    Expect(Moving->Audio && !Moving->Audio->bIsUISound && Moving->Audio->bAllowSpatialization && Moving->LoopCue->IsLooping() && Hum->bLooping==OriginalLoop,TEXT("Drone uses spatial looping playback without changing source wave"));
    Moving->UpdateMovement(300.f,.05f);
    Expect(Moving->MovementBlend>0 && Moving->MovementBlend<1,TEXT("Movement transitions smoothly, no instantaneous pitch jump"));
    for(int32 I=0;I<100;++I) Moving->UpdateMovement(300.f,.05f);
    Expect(FMath::IsNearlyEqual(Moving->Audio->PitchMultiplier,1.3f,.001f) && FMath::IsNearlyEqual(Moving->Audio->VolumeMultiplier,.13f,.001f),TEXT("Moving drone raises pitch/playback rate and volume"));
    for(int32 I=0;I<100;++I) Moving->UpdateMovement(0.f,.05f);
    Expect(FMath::IsNearlyEqual(Moving->Audio->PitchMultiplier,.95f,.001f) && FMath::IsNearlyEqual(Moving->Audio->VolumeMultiplier,.08f,.001f),TEXT("Stationary drone returns to quiet idle hum"));
    Expect(UVTGSpatialLoopComponent::MovementAmount(5,10,300)==0 && UVTGSpatialLoopComponent::MovementAmount(600,10,300)==1,TEXT("Dead zone ignores small idle drift; fast movement clamps safely"));
    Moving->LastLocation=Owner->GetActorLocation();Owner->SetActorLocation(FVector(15,0,0));Moving->TickComponent(.05f,LEVELTICK_All,nullptr);
    Expect(Moving->MovementBlend>.1f,TEXT("Actual position movement drives audio without Character velocity (Follow/GoTo)"));
    Moving->EndPlay(EEndPlayReason::Destroyed);Loop->EndPlay(EEndPlayReason::Destroyed);
    Expect(!Moving->Audio && !Loop->Audio,TEXT("Actor teardown releases both looping components"));
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
    return true;
}


