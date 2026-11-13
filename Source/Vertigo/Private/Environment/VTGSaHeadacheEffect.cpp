#include "Environment/VTGSaHeadacheEffect.h"
#include "Components/AudioComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundWave.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

AVTGSaHeadacheEffect::AVTGSaHeadacheEffect()
{
    PrimaryActorTick.bCanEverTick=false;
    HeadachePostProcess=CreateDefaultSubobject<UPostProcessComponent>(TEXT("HeadachePostProcess"));
    SetRootComponent(HeadachePostProcess);
    HeadachePostProcess->bUnbound=true;
    HeadachePostProcess->Priority=100.f;
    HeadachePostProcess->BlendWeight=0.f;
    HeadacheAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("HeadacheAudio"));
    HeadacheAudio->SetupAttachment(RootComponent);
    HeadacheAudio->bAutoActivate=false;
    HeadacheAudio->bAutoDestroy=false;
    HeadacheAudio->bAllowSpatialization=false;
    HeadacheAudio->bIsUISound=false;
    HeadacheAudio->bStopWhenOwnerDestroyed=true;
    HeadacheAudio->EnvelopeFollowerAttackTime=60;
    HeadacheAudio->EnvelopeFollowerReleaseTime=180;
    HeadacheTimeline=CreateDefaultSubobject<UTimelineComponent>(TEXT("HeadacheTimeline"));
    HeadacheTimeline->SetLooping(false);
    IntensityCurve=CreateDefaultSubobject<UCurveFloat>(TEXT("HeadacheIntensityEnvelope"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/VTG/Materials/PostProcess/Headache/M_PP_Headache"));
    static ConstructorHelpers::FObjectFinder<USoundWave> Sound(TEXT("/Game/SFX/Character/SFX_SaHeadache1"));
    HeadacheMaterial=Material.Object;
    HeadacheSound=Sound.Object;
}

void AVTGSaHeadacheEffect::InitializePlayback()
{
    if (!bTimelineReady)
    {
        // Keep the original attack/peak timing, then retain a quieter tail until 5 s.
        // Build at playback initialization so old serialized Blueprint curve defaults cannot restore the short tail.
        IntensityCurve->FloatCurve.Reset();
        for(const auto& Key:{FVector2D(0,0),FVector2D(.066,.35),FVector2D(.176,1),
                            FVector2D(.286,.92),FVector2D(.44,.32),FVector2D(.84,.18),FVector2D(1,0)})
        {
            auto Handle=IntensityCurve->FloatCurve.AddKey(Key.X,Key.Y);
            IntensityCurve->FloatCurve.SetKeyInterpMode(Handle,RCIM_Cubic);
            IntensityCurve->FloatCurve.SetKeyTangentMode(Handle,RCTM_Auto);
        }
        IntensityCurve->FloatCurve.AutoSetTangents();
        FOnTimelineFloat Update; Update.BindUFunction(this,TEXT("UpdateHeadache"));
        FOnTimelineEvent Finished; Finished.BindUFunction(this,TEXT("FinishHeadache"));
        HeadacheTimeline->AddInterpFloat(IntensityCurve,Update);
        HeadacheTimeline->SetTimelineFinishedFunc(Finished);
        HeadacheTimeline->SetTimelineLengthMode(TL_TimelineLength);
        HeadacheTimeline->SetTimelineLength(1.f);
        HeadacheAudio->OnAudioSingleEnvelopeValue.AddUniqueDynamic(this,&AVTGSaHeadacheEffect::ReceiveHeadacheEnvelope);
        bTimelineReady=true;
    }
    if (!HeadacheMID && HeadacheMaterial)
    {
        HeadacheMID=UMaterialInstanceDynamic::Create(HeadacheMaterial,this);
        HeadacheMID->SetScalarParameterValue(TEXT("HeadacheIntensity"),0.f);
        HeadachePostProcess->AddOrUpdateBlendable(HeadacheMID,1.f);
    }
}
void AVTGSaHeadacheEffect::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoPlayOnBeginPlay)
    {
        // Preserve the old Blueprint's existing per-instance strength setting.
        float Intensity=1.f;
        if (auto* Legacy=FindFProperty<FDoubleProperty>(GetClass(),TEXT("BlendWeightIntensity")))
            Intensity=(float)Legacy->GetPropertyValue_InContainer(this);
        PlayHeadacheVFX(Intensity,HeadacheDuration);
    }
}
void AVTGSaHeadacheEffect::PlayHeadacheVFX(float Intensity,float Duration)
{
    if (!GetWorld() || !FMath::IsFinite(Intensity) || !FMath::IsFinite(Duration)) return;
    if (Intensity<=0.f || Duration<=0.f) { StopHeadacheVFX(); return; }
    InitializePlayback();
    if (!HeadacheMID) return;
    RestartIntensity=CurrentHeadacheIntensity;
    PeakIntensity=FMath::Clamp(Intensity,0.f,1.f);
    HeadacheDuration=FMath::Max(Duration,.1f);
    AudioEnvelope=1.f;
    GetWorldTimerManager().ClearTimer(AudioFadeTimer);
    GetWorldTimerManager().ClearTimer(AudioEndFadeTimer);
    HeadacheAudio->Stop();
    HeadacheAudio->SetSound(HeadacheSound);
    if (HeadacheSound && HeadacheAudioVolume>0.f)
    {
        HeadacheAudio->FadeIn(.06f,FMath::Clamp(HeadacheAudioVolume,0.f,1.f),0.f);
        GetWorldTimerManager().SetTimer(AudioFadeTimer,this,&AVTGSaHeadacheEffect::StartAudioFade,HeadacheDuration*.286f,false);
        GetWorldTimerManager().SetTimer(AudioEndFadeTimer,this,&AVTGSaHeadacheEffect::FinishAudioFade,HeadacheDuration*.84f,false);
    }
    HeadacheTimeline->SetPlayRate(1.f/HeadacheDuration);
    HeadacheTimeline->PlayFromStart();
    UpdateHeadache(0.f);
}
void AVTGSaHeadacheEffect::UpdateHeadache(float Shape)
{
    if (!HeadacheMID) return;
    float Phase=HeadacheTimeline->GetPlaybackPosition();
    float Carry=1.f-FMath::SmoothStep(0.f,.176f,Phase);
    float SoundMod=FMath::Lerp(1.f,AudioEnvelope,FMath::Clamp(HeadacheEnvelopeInfluence,0.f,.25f));
    float Target=PeakIntensity*Shape*SoundMod;
    CurrentHeadacheIntensity=FMath::Clamp(RestartIntensity>SMALL_NUMBER?FMath::Lerp(Target,RestartIntensity,Carry):Target,0.f,1.f);
    if(IsRunningCommandlet())UE_LOG(LogTemp,Display,TEXT("HEADACHE_SHAPE shape=%.3f phase=%.3f peak=%.3f value=%.3f keys=%d"),Shape,Phase,PeakIntensity,CurrentHeadacheIntensity,IntensityCurve->FloatCurve.GetNumKeys());
    HeadacheMID->SetScalarParameterValue(TEXT("HeadacheIntensity"),CurrentHeadacheIntensity);
    HeadachePostProcess->BlendWeight=CurrentHeadacheIntensity>SMALL_NUMBER?1.f:0.f;
}
void AVTGSaHeadacheEffect::ReceiveHeadacheEnvelope(const USoundWave*,float EnvelopeValue)
{
    // The waveform's RMS peaks around 0.56. The audio mixer's smoothed follower drives only a small modulation.
    AudioEnvelope=FMath::Clamp(EnvelopeValue*2.f,0.f,1.f);
}
void AVTGSaHeadacheEffect::StartAudioFade()
{
    // AdjustVolume keeps playback alive: 1.43 s starts a reduction to 32%, not a stop.
    HeadacheAudio->AdjustVolume(FMath::Max(.01f,HeadacheDuration*.154f),FMath::Clamp(HeadacheAudioVolume,0.f,1.f)*.32f, EAudioFaderCurve::SCurve);
}
void AVTGSaHeadacheEffect::FinishAudioFade()
{
    HeadacheAudio->FadeOut(FMath::Max(.01f,HeadacheDuration*.16f),0.f, EAudioFaderCurve::SCurve);
}
void AVTGSaHeadacheEffect::FinishHeadache() { StopHeadacheVFX(); }
void AVTGSaHeadacheEffect::StopHeadacheVFX()
{
    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(AudioFadeTimer);
        GetWorldTimerManager().ClearTimer(AudioEndFadeTimer);
    }
    HeadacheTimeline->Stop();
    HeadacheTimeline->Deactivate();
    HeadacheAudio->Stop();
    CurrentHeadacheIntensity=0.f; RestartIntensity=0.f;
    if (HeadacheMID) HeadacheMID->SetScalarParameterValue(TEXT("HeadacheIntensity"),0.f);
    HeadachePostProcess->BlendWeight=0.f;
}
void AVTGSaHeadacheEffect::EndPlay(const EEndPlayReason::Type Reason)
{
    StopHeadacheVFX();
    Super::EndPlay(Reason);
}
AVTGSaHeadacheEffect* AVTGSaHeadacheEffect::PlaySaHeadache(UObject* Context,TSubclassOf<AVTGSaHeadacheEffect> EffectClass,
    float Intensity,float Duration,float AudioVolume,float EnvelopeInfluence)
{
    UWorld* World=GEngine?GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull):nullptr;
    if (!World || !EffectClass) return nullptr;
    AVTGSaHeadacheEffect* Effect=nullptr;
    for (TActorIterator<AVTGSaHeadacheEffect> It(World,EffectClass);It;++It)
        if (IsValid(*It)) { Effect=*It; break; }
    if (!Effect && Intensity>0.f && Duration>0.f)
    {
        Effect=World->SpawnActorDeferred<AVTGSaHeadacheEffect>(EffectClass,FTransform::Identity,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Effect) return nullptr;
        Effect->bAutoPlayOnBeginPlay=false;
        Effect->FinishSpawning(FTransform::Identity);
    }
    if (Effect)
    {
        Effect->HeadacheAudioVolume=AudioVolume;
        Effect->HeadacheEnvelopeInfluence=EnvelopeInfluence;
        Effect->PlayHeadacheVFX(Intensity,Duration);
    }
    return Effect;
}
