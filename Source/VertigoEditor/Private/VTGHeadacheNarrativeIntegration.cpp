#include "Environment/VTGSaHeadacheEffect.h"
#include "NarrativeEvent.h"
#include "Components/AudioComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/TimelineComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/TimelineTemplate.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Editor.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace HeadacheNarrative
{
UEdGraphPin* Pin(UEdGraphNode* Node,const TCHAR* Name)
{
    auto* P=Node->FindPin(Name);checkf(P,TEXT("Missing %s on %s"),Name,*Node->GetClass()->GetName());return P;
}
void Link(UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BP)
{
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Pin(A,AP),Pin(B,BP)));
}
template<class T>T* Add(UEdGraph* G,int X,int Y)
{
    auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;return N;
}
void Compile(UBlueprint* BP)
{
    FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Results);
    checkf(!Results.NumErrors,TEXT("%s compile errors=%d"),*BP->GetName(),Results.NumErrors);
}
void Save(UBlueprint* BP)
{
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args));
}
void Backup(UBlueprint* BP,const TCHAR* Suffix=TEXT(".before_narrative_integration.uasset"))
{
    FString Source=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    FString Dest=FPaths::ProjectDir()/TEXT("Scripts/HeadachePostProcess/Backup")/(BP->GetName()+Suffix);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest),true);
    if(!IFileManager::Get().FileExists(*Dest))check(IFileManager::Get().Copy(*Dest,*Source,false)==COPY_OK);
}
void EmptyEvents(UBlueprint* BP)
{
    for(UEdGraph* G:BP->UbergraphPages){auto Nodes=G->Nodes;for(UEdGraphNode* N:Nodes)FBlueprintEditorUtils::RemoveNode(BP,N,true);}
    auto Timelines=BP->Timelines;for(UTimelineTemplate* T:Timelines)FBlueprintEditorUtils::RemoveTimeline(BP,T,true);
}
void Scalar(UBlueprint* BP,FName Name,const TCHAR* Value,const TCHAR* Min,const TCHAR* Max,const TCHAR* Tip)
{
    bool Found=false;for(const auto& V:BP->NewVariables)if(V.VarName==Name)Found=true;
    if(!Found){FEdGraphPinType Type;Type.PinCategory=UEdGraphSchema_K2::PC_Real;Type.PinSubCategory=UEdGraphSchema_K2::PC_Double;check(FBlueprintEditorUtils::AddMemberVariable(BP,Name,Type,Value));}
    FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP,Name,false);
    FBlueprintEditorUtils::SetBlueprintVariableCategory(BP,Name,nullptr,FText::FromString(TEXT("Headache")));
    FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,Name,nullptr,TEXT("ClampMin"),Min);
    FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,Name,nullptr,TEXT("ClampMax"),Max);
    FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,Name,nullptr,TEXT("ToolTip"),Tip);
}
void Verify(UBlueprint* EffectBP,UBlueprint* EventBP)
{
    FEditorScriptExecutionGuard ScriptGuard;
    check(EffectBP->GeneratedClass->IsChildOf(AVTGSaHeadacheEffect::StaticClass()));
    check(EffectBP->Timelines.Num()==0);
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("HeadacheNarrativeVerification"),nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Event=NewObject<UNarrativeEvent>(World,EventBP->GeneratedClass);
    auto* EventStrength=FindFProperty<FDoubleProperty>(EventBP->GeneratedClass,TEXT("Blend Weight Intensity"));check(EventStrength);
    EventStrength->SetPropertyValue_InContainer(Event,.85);
    // Execute the real Blueprint Narrative event, rather than calling only the helper.
    Event->ExecuteEvent(nullptr,nullptr,nullptr);
    int Count=0;AVTGSaHeadacheEffect* FromEvent=nullptr;
    for(TActorIterator<AVTGSaHeadacheEffect> It(World);It;++It){++Count;FromEvent=*It;}
    check(Count==1 && FromEvent && FromEvent->HeadacheTimeline->IsPlaying());
    // The isolated commandlet world has no game mode to dispatch BeginPlay. Actor
    // ProcessEvent intentionally suppresses Timeline delegates before that point.
    if(!FromEvent->HasActorBegunPlay())FromEvent->DispatchBeginPlay();
    TSubclassOf<AVTGSaHeadacheEffect> EffectClass=EffectBP->GeneratedClass.Get();
    auto* Effect=AVTGSaHeadacheEffect::PlaySaHeadache(World,EffectClass,.8f,2.2f,.65f,.12f);check(Effect==FromEvent);
    auto* Again=AVTGSaHeadacheEffect::PlaySaHeadache(World,EffectClass,.7f,3.f,.5f,.12f);check(Again==Effect);
    TArray<UPostProcessComponent*> PP;Effect->GetComponents(PP);check(PP.Num()==1);
    TArray<UAudioComponent*> Audio;Effect->GetComponents(Audio);check(Audio.Num()==1);
    check(!Effect->PrimaryActorTick.bCanEverTick);
    check(PP[0]->Settings.WeightedBlendables.Array.Num()==1);
    check(Effect->HeadacheAudio->Sound==Effect->HeadacheSound);
    check(Effect->HeadacheTimeline->IsPlaying());
    Effect->HeadacheTimeline->SetPlaybackPosition(.176f,false,true);
    float Loud=Effect->CurrentHeadacheIntensity;check(Loud>.6f && Loud<=.71f);
    struct {const USoundWave* Wave;float Value;} Envelope{nullptr,0.f};
    Effect->ProcessEvent(Effect->FindFunctionChecked(TEXT("ReceiveHeadacheEnvelope")),&Envelope);
    Effect->HeadacheTimeline->SetPlaybackPosition(.176f,false,true);
    float Quiet=Effect->CurrentHeadacheIntensity;check(Quiet<Loud && Quiet>Loud*.7f);
    // A retrigger keeps the current visual level instead of flashing to zero.
    Effect->HeadacheTimeline->SetPlaybackPosition(.286f,false,true);
    float AtReduction=Effect->CurrentHeadacheIntensity;check(AtReduction>0.f);
    Effect->HeadacheTimeline->SetPlaybackPosition(.44f,false,true);
    float Tail=Effect->CurrentHeadacheIntensity;check(Tail>0.f && Tail<AtReduction);
    float Before=Tail;Effect->PlayHeadacheVFX(.9f,5.f);check(FMath::IsNearlyEqual(Before,Effect->CurrentHeadacheIntensity));
    Effect->HeadacheTimeline->TickComponent(2.3f,LEVELTICK_All,nullptr);
    check(Effect->HeadacheTimeline->IsPlaying() && Effect->CurrentHeadacheIntensity>0.f);
    // Override during the tail: crossing the old 5 s deadline must not end the new round.
    Effect->PlayHeadacheVFX(.8f,5.f);
    Effect->HeadacheTimeline->TickComponent(3.f,LEVELTICK_All,nullptr);
    check(Effect->HeadacheTimeline->IsPlaying() && Effect->CurrentHeadacheIntensity>0.f);
    Effect->HeadacheTimeline->TickComponent(2.1f,LEVELTICK_All,nullptr);
    check(!Effect->HeadacheTimeline->IsPlaying());
    check(Effect->CurrentHeadacheIntensity==0.f && PP[0]->BlendWeight==0.f);
    check(!PP[0]->Settings.bOverride_AutoExposureBias && !PP[0]->Settings.bOverride_MotionBlurAmount);
    Effect->PlayHeadacheVFX(0.f,2.2f);check(PP[0]->BlendWeight==0.f);
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    UE_LOG(LogTemp,Display,TEXT("HEADACHE_NARRATIVE_VERIFY_PASS loud=%.3f quiet=%.3f; real event, reuse, envelope, retrigger, cleanup verified"),Loud,Quiet);
}
}

int32 VTGIntegrateHeadacheNarrative(const FString& Params)
{
    using namespace HeadacheNarrative;
    auto* EffectBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/Events/CharacterStatus/PP_Sa_Headache.PP_Sa_Headache"));
    auto* EventBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/Events/CharacterStatus/NE_PP_SaHeadache.NE_PP_SaHeadache"));check(EffectBP && EventBP);
    if(Params.Contains(TEXT("ExtendTail")))
    {
        Backup(EffectBP,TEXT(".before_five_second_tail.uasset"));Backup(EventBP,TEXT(".before_five_second_tail.uasset"));
        Compile(EffectBP);Compile(EventBP);
        EffectBP->GeneratedClass->GetDefaultObject<AVTGSaHeadacheEffect>()->HeadacheDuration=5.f;
        auto* Duration=FindFProperty<FDoubleProperty>(EventBP->GeneratedClass,TEXT("HeadacheDuration"));check(Duration);
        Duration->SetPropertyValue_InContainer(EventBP->GeneratedClass->GetDefaultObject(),5.0);
        for(auto& V:EventBP->NewVariables)if(V.VarName==TEXT("HeadacheDuration"))V.DefaultValue=TEXT("5.0");
        FBlueprintEditorUtils::SetBlueprintVariableMetaData(EventBP,TEXT("HeadacheDuration"),nullptr,TEXT("ToolTip"),
            TEXT("Total duration. Default 5s: reduce at 1.43s, retain a quiet tail, fade out fully at 5s. Retriggers replace playback."));
        Verify(EffectBP,EventBP);Save(EffectBP);Save(EventBP);
        UE_LOG(LogTemp,Display,TEXT("HEADACHE_FIVE_SECOND_TAIL_SAVED"));return 0;
    }
    if(Params.Contains(TEXT("VerifyOnly"))){Compile(EffectBP);Compile(EventBP);Verify(EffectBP,EventBP);return 0;}
    Backup(EffectBP);Backup(EventBP);
    EmptyEvents(EffectBP);
    // Remove leaves before roots: removing a root first strands children in AllNodes.
    TFunction<void(USCS_Node*)> RemoveTree=[&](USCS_Node* N)
    {
        auto Children=N->GetChildNodes();for(USCS_Node* Child:Children)RemoveTree(Child);
        EffectBP->SimpleConstructionScript->RemoveNode(N,false);
    };
    auto Roots=EffectBP->SimpleConstructionScript->GetRootNodes();for(USCS_Node* Root:Roots)RemoveTree(Root);
    EffectBP->ParentClass=AVTGSaHeadacheEffect::StaticClass();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(EffectBP);
    FBlueprintEditorUtils::RefreshAllNodes(EffectBP);Compile(EffectBP);
    auto* CDO=EffectBP->GeneratedClass->GetDefaultObject<AVTGSaHeadacheEffect>();
    CDO->HeadachePostProcess->BlendWeight=0.f;
    CDO->PrimaryActorTick.bCanEverTick=false;
    CDO->HeadacheDuration=5.f;CDO->HeadacheAudioVolume=.65f;CDO->HeadacheEnvelopeInfluence=.12f;

    EmptyEvents(EventBP);
    Scalar(EventBP,TEXT("Blend Weight Intensity"),TEXT("1.0"),TEXT("0"),TEXT("1"),TEXT("Peak headache strength. Existing Narrative event values are preserved."));
    Scalar(EventBP,TEXT("HeadacheDuration"),TEXT("5.0"),TEXT("0.1"),TEXT("30"),TEXT("Total duration. Default 5s: reduce at 1.43s, retain a quiet tail, fade out fully at 5s."));
    Scalar(EventBP,TEXT("HeadacheAudioVolume"),TEXT("0.65"),TEXT("0"),TEXT("1"),TEXT("Volume of SFX_SaHeadache1 only; 0 mutes it."));
    Scalar(EventBP,TEXT("HeadacheEnvelopeInfluence"),TEXT("0.12"),TEXT("0"),TEXT("0.25"),TEXT("Subtle intensity modulation by the live, smoothed sound envelope. 0 disables syncing."));
    for(FName Old:{FName(TEXT("CamShakeType")),FName(TEXT("Spawn Transform"))})
        FBlueprintEditorUtils::SetBlueprintVariableCategory(EventBP,Old,nullptr,FText::FromString(TEXT("Legacy (unused)")));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(EventBP);Compile(EventBP);
    UEdGraph* G=EventBP->UbergraphPages[0];
    auto* Event=Add<UK2Node_Event>(G,0,0);
    Event->EventReference.SetExternalMember(TEXT("ExecuteEvent"),UNarrativeEvent::StaticClass());
    Event->bOverrideFunction=true;Event->AllocateDefaultPins();
    auto* Call=Add<UK2Node_CallFunction>(G,520,0);
    Call->SetFromFunction(AVTGSaHeadacheEffect::StaticClass()->FindFunctionByName(TEXT("PlaySaHeadache")));Call->AllocateDefaultPins();
    Link(Event,TEXT("then"),Call,TEXT("execute"));
    GetDefault<UEdGraphSchema_K2>()->TrySetDefaultObject(*Pin(Call,TEXT("EffectClass")),EffectBP->GeneratedClass);
    int Y=220;
    for(const auto& Pair:{TPair<FName,FName>(TEXT("Blend Weight Intensity"),TEXT("Intensity")),
                         TPair<FName,FName>(TEXT("HeadacheDuration"),TEXT("Duration")),
                         TPair<FName,FName>(TEXT("HeadacheAudioVolume"),TEXT("AudioVolume")),
                         TPair<FName,FName>(TEXT("HeadacheEnvelopeInfluence"),TEXT("EnvelopeInfluence"))})
    {
        auto* Get=Add<UK2Node_VariableGet>(G,180,Y);Get->VariableReference.SetSelfMember(Pair.Key);Get->AllocateDefaultPins();
        Link(Get,*Pair.Key.ToString(),Call,*Pair.Value.ToString());Y+=130;
    }
    Call->NodeComment=TEXT("Reuse PP_Sa_Headache; new blood-red PP + specified sound. No camera shake or latent Delay. Sound fades out with the effect.");Call->bCommentBubbleVisible=true;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(EventBP);Compile(EventBP);
    auto* DefaultStrength=FindFProperty<FDoubleProperty>(EventBP->GeneratedClass,TEXT("Blend Weight Intensity"));check(DefaultStrength);
    DefaultStrength->SetPropertyValue_InContainer(EventBP->GeneratedClass->GetDefaultObject(),1.0);
    Verify(EffectBP,EventBP);Save(EffectBP);Save(EventBP);
    UE_LOG(LogTemp,Display,TEXT("HEADACHE_NARRATIVE_INTEGRATION_SAVED"));return 0;
}
