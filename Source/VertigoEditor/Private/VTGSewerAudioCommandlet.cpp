#include "VTGSewerAudioCommandlet.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Audio/VTGPlayerAudioComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Sound/ReverbEffect.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

extern bool VTGTestSewerAudio(UBlueprint *BP);
namespace
{
const FString Root = TEXT("/Game/SFX/SewerAudio/");
const FString Steps = TEXT("/Game/SFX/FootstepAndFoleySounds/Footsteps/MonoVersions/Wavs/");
const FString Bonus = TEXT("/Game/SFX/FootstepAndFoleySounds/ProSoundCollectionBonusSounds/");
template <class T> T *Load(const FString &Path)
{
    auto *O = LoadObject<T>(nullptr, *Path);
    checkf(O, TEXT("Missing %s"), *Path);
    return O;
}
void Save(UObject *O)
{
    auto *P = O->GetOutermost();
    P->MarkPackageDirty();
    FSavePackageArgs A;
    A.TopLevelFlags = RF_Public | RF_Standalone;
    A.SaveFlags = SAVE_NoError;
    check(UPackage::SavePackage(
        P, nullptr, *FPackageName::LongPackageNameToFilename(P->GetName(), FPackageName::GetAssetPackageExtension()),
        A));
    UE_LOG(LogTemp, Display, TEXT("SEWER AUDIO Saved %s"), *P->GetName());
}
template <class T> T *Asset(const TCHAR *Name)
{
    const FString Path = Root + Name;
    if (auto *O = LoadObject<T>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
        return O;
    auto *O = NewObject<T>(CreatePackage(*Path), Name, RF_Public | RF_Standalone);
    FAssetRegistryModule::AssetCreated(O);
    return O;
}
USoundCue *Loop(const TCHAR *Name, const FString &Source)
{
    auto *C = Asset<USoundCue>(Name);
    if (!C->FirstNode)
    {
        auto *W = NewObject<USoundNodeWavePlayer>(C);
        W->SetSoundWave(Load<USoundWave>(Source));
        W->bLooping = true;
        C->FirstNode = W;
        C->AllNodes.Add(W);
    }
    C->VolumeMultiplier = 1;
    C->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
    C->PostEditChange();
    Save(C);
    return C;
}
UReverbEffect *Reverb(const TCHAR *Name, float Decay, float Delay, float Brightness)
{
    auto *R = Asset<UReverbEffect>(Name);
    R->DecayTime = Decay;
    R->ReflectionsDelay = Delay;
    R->LateDelay = FMath::Min(Delay * 2, .09f);
    R->Gain = .6f;
    R->GainHF = Brightness;
    R->DecayHFRatio = .65f;
    R->Density = .95f;
    R->Diffusion = .85f;
    R->ReflectionsGain = .7f;
    R->LateGain = 1.1f;
    Save(R);
    return R;
}
void ConfigureReverb(UVTGPlayerAudioComponent *C)
{
    // 2D beds otherwise inherit Master.Default2DReverbSendAmount == 0 (fully dry).
    auto *AmbientClass = Asset<USoundClass>(TEXT("SCL_SewerAmbience"));
    AmbientClass->Properties.bReverb = true;
    AmbientClass->Properties.bIsMusic = false;
    AmbientClass->Properties.Default2DReverbSendAmount = .3f;
    Save(AmbientClass);
    for (const TCHAR *Name : {TEXT("SC_SewerWaterLoop"), TEXT("SC_SewerPipeLoop")})
    {
        auto *Cue = Load<USoundCue>(Root + Name);
        Cue->SoundClassObject = AmbientClass;
        Save(Cue);
    }
    C->StepAttenuation->Attenuation.ManualReverbSendLevel = .65f;
    Save(C->StepAttenuation);
    C->AmbientAttenuation->Attenuation.ManualReverbSendLevel = .5f;
    Save(C->AmbientAttenuation);
    C->PipeWetLevel = .65f;
    C->CorridorWetLevel = .75f;
    C->ChamberWetLevel = .85f;
    C->PipeReverb = Reverb(TEXT("R_SewerPipe"), 1.35f, .012f, .62f);
    C->CorridorReverb = Reverb(TEXT("R_SewerCorridor"), 2.3f, .025f, .55f);
    C->ChamberReverb = Reverb(TEXT("R_SewerChamber"), 3.8f, .04f, .48f);
}
void AddNotify(UAnimSequence *A, FName Bone, float Time)
{
    auto *N = NewObject<UVTGFootstepNotify>(A, NAME_None, RF_Transactional);
    N->FootBone = Bone;
    auto &E = A->Notifies.AddDefaulted_GetRef();
    E.Notify = N;
    E.NotifyName = TEXT("VTG_Footstep");
    E.Link(A, Time);
    E.TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetAfter);
    E.Guid = FGuid::NewGuid();
    E.TriggerWeightThreshold = .25f;
}
float FootHeight(UAnimSequence *A, FName Bone, float Time)
{
    const auto &Ref = A->GetSkeleton()->GetReferenceSkeleton();
    int32 Index = Ref.FindBoneIndex(Bone);
    check(Index != INDEX_NONE);
    FTransform Transform = FTransform::Identity;
    while (Index != INDEX_NONE)
    {
        FTransform Local;
        A->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Index), double(Time), true);
        Transform = Transform * Local;
        Index = Ref.GetParentIndex(Index);
    }
    return Transform.GetLocation().Z;
}
void Footsteps(UAnimSequence *A)
{
    int32 Count = 0;
    bool Changed = false;
    for (auto &E : A->Notifies)
    {
        if (auto *Existing = Cast<UVTGFootstepNotify>(E.Notify))
        {
            if (Existing->FootBone.IsNone())
            {
                Existing->FootBone =
                    FootHeight(A, TEXT("foot_l"), E.GetTime()) < FootHeight(A, TEXT("foot_r"), E.GetTime())
                        ? TEXT("foot_l")
                        : TEXT("foot_r");
                Changed = true;
            }
            ++Count;
            continue;
        }
        if (!E.Notify || E.Notify->GetClass()->GetName() != TEXT("AN_FootStep_C"))
            continue;
        auto *Bone = FindFProperty<FNameProperty>(E.Notify->GetClass(), TEXT("BoneName"));
        auto *N = NewObject<UVTGFootstepNotify>(A, NAME_None, RF_Transactional);
        N->FootBone = Bone ? Bone->GetPropertyValue_InContainer(E.Notify) : FName(TEXT("foot_l"));
        if (N->FootBone.IsNone())
            N->FootBone = FootHeight(A, TEXT("foot_l"), E.GetTime()) < FootHeight(A, TEXT("foot_r"), E.GetTime())
                              ? TEXT("foot_l")
                              : TEXT("foot_r");
        E.Notify = N;
        E.NotifyName = TEXT("VTG_Footstep");
        E.TriggerWeightThreshold = .25f;
        ++Count;
        Changed = true;
    }
    if (!Count)
    {
        // Derive foot plant times from the authored bone motion; do not run a cadence timer.
        float LeftTime = 0, RightTime = 0, MinZ = BIG_NUMBER;
        const float Length = A->GetPlayLength();
        for (int32 I = 0; I < 120; ++I)
        {
            float T = Length * I / 120.f, Z = FootHeight(A, TEXT("foot_l"), T);
            if (Z < MinZ)
            {
                MinZ = Z;
                LeftTime = T;
            }
        }
        MinZ = BIG_NUMBER;
        for (int32 I = 0; I < 120; ++I)
        {
            float T = Length * I / 120.f, Gap = FMath::Abs(T - LeftTime) / Length;
            if (Gap < .25f || Gap > .75f)
                continue;
            float Z = FootHeight(A, TEXT("foot_r"), T);
            if (Z < MinZ)
            {
                MinZ = Z;
                RightTime = T;
            }
        }
        AddNotify(A, TEXT("foot_l"), LeftTime);
        AddNotify(A, TEXT("foot_r"), RightTime);
        Changed = true;
    }
    if (Changed)
    {
        A->SortNotifies();
        A->PostEditChange();
        Save(A);
    }
    for (auto &E : A->Notifies)
        if (auto *N = Cast<UVTGFootstepNotify>(E.Notify))
            UE_LOG(LogTemp, Display, TEXT("SEWER AUDIO plant %s %s %.3f"), *A->GetName(), *N->FootBone.ToString(),
                   E.GetTime());
}
} // namespace
UVTGSewerAudioCommandlet::UVTGSewerAudioCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}
int32 UVTGSewerAudioCommandlet::Main(const FString &Params)
{
    auto *BP = Load<UBlueprint>(TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));
    if (FParse::Param(*Params, TEXT("VerifyOnly")))
        return VTGTestSewerAudio(BP) ? 0 : 1;
    UVTGPlayerAudioComponent *C = nullptr;
    for (auto *N : BP->SimpleConstructionScript->GetAllNodes())
        if (N->GetVariableName() == TEXT("PlayerAudio"))
            C = CastChecked<UVTGPlayerAudioComponent>(N->ComponentTemplate);
    if (!C)
    {
        auto *N =
            BP->SimpleConstructionScript->CreateNode(UVTGPlayerAudioComponent::StaticClass(), TEXT("PlayerAudio"));
        BP->SimpleConstructionScript->AddNode(N);
        C = CastChecked<UVTGPlayerAudioComponent>(N->ComponentTemplate);
    }
    if (FParse::Param(*Params, TEXT("ReverbOnly")))
    {
        ConfigureReverb(C);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &Log);
        check(Log.NumErrors == 0);
        Save(BP);
        return VTGTestSewerAudio(BP) ? 0 : 1;
    }
    auto Fill = [&](auto &Array, const TCHAR *Surface)
    {
        Array.Empty();
        for (int32 I = 1; I <= 6; ++I)
            Array.Add(Load<USoundWave>(Steps + FString::Printf(TEXT("footstep_%s_walk_%02d"), Surface, I)));
    };
    Fill(C->ConcreteSteps, TEXT("concrete"));
    Fill(C->MetalSteps, TEXT("metal_low"));
    Fill(C->WaterSteps, TEXT("water"));
    Fill(C->WoodSteps, TEXT("wood"));
    auto *Att = Asset<USoundAttenuation>(TEXT("SA_Footsteps"));
    Att->Attenuation.bAttenuate = true;
    Att->Attenuation.bSpatialize = true;
    Att->Attenuation.AttenuationShapeExtents = FVector(150, 0, 0);
    Att->Attenuation.FalloffDistance = 1500;
    Att->Attenuation.bEnableReverbSend = true;
    Att->Attenuation.ReverbSendMethod = EReverbSendMethod::Manual;
    Att->Attenuation.ManualReverbSendLevel = .45f;
    Save(Att);
    C->StepAttenuation = Att;
    auto *Amb = Asset<USoundAttenuation>(TEXT("SA_PipeDetails"));
    Amb->Attenuation = Att->Attenuation;
    Amb->Attenuation.AttenuationShapeExtents = FVector(200, 0, 0);
    Amb->Attenuation.FalloffDistance = 1800;
    Amb->Attenuation.bEnableOcclusion = true;
    Amb->Attenuation.OcclusionVolumeAttenuation = .45f;
    Amb->Attenuation.OcclusionLowPassFilterFrequency = 1800;
    Save(Amb);
    C->AmbientAttenuation = Amb;
    C->WaterBed = Loop(TEXT("SC_SewerWaterLoop"),
                       Bonus + TEXT("StereoVersions/Wavs/river_stream_flowing_water_loop_02_short_version"));
    C->PipeBed =
        Loop(TEXT("SC_SewerPipeLoop"), TEXT("/Game/SFX/SciFi_SFX/Audio/Engine_hum/engine_background_hum_pipes"));
    C->PipeHiss = Load<USoundWave>(Bonus + TEXT("MonoVersions/Wavs/gas_leak_med_burst_02"));
    ConfigureReverb(C);
    TSet<UAnimSequence *> Animations;
    for (const TCHAR *Path :
         {TEXT("/Game/Characters/Sa/BlendSpace_Sa_Loco"), TEXT("/Game/Characters/Sa/Anm/BS_Sa_NoRunning"),
          TEXT("/Game/Characters/Sa/BlendSpace_Sa_Hammer"), TEXT("/Game/Characters/Sa/Anm/BS_Sa_Crouch")})
    {
        auto *BS = Load<UBlendSpace>(Path);
        for (const auto &Sample : BS->GetBlendSamples())
            if (Sample.Animation && FMath::Abs(Sample.SampleValue.Y) > 1)
                Animations.Add(Sample.Animation);
        BS->NotifyTriggerMode = ENotifyTriggerMode::HighestWeightedAnimation;
        Save(BS);
    }
    for (auto *A : Animations)
        Footsteps(A);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &Log);
    check(Log.NumErrors == 0);
    Save(BP);
    UE_LOG(LogTemp, Display, TEXT("SEWER AUDIO installed: %d locomotion animations"), Animations.Num());
    return VTGTestSewerAudio(BP) ? 0 : 1;
}
