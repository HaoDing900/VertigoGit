#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Audio/VTGPlayerAudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "UObject/UnrealType.h"

bool VTGTestSewerAudio(UBlueprint *BP)
{
    int32 Fail = 0;
    auto Expect = [&](bool OK, const TCHAR *Message)
    {
        Fail += !OK;
        UE_LOG(LogTemp, Display, TEXT("AUDIO TEST %s: %s"), OK ? TEXT("PASS") : TEXT("FAIL"), Message);
    };
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    auto Init = UWorld::InitializationValues()
                    .AllowAudioPlayback(false)
                    .RequiresHitProxies(false)
                    .CreatePhysicsScene(true)
                    .CreateNavigation(false)
                    .CreateAISystem(false)
                    .ShouldSimulatePhysics(false)
                    .SetTransactional(false);
    auto *W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SewerAudioTest"), nullptr, true, ERHIFeatureLevel::Num,
                                  &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto *Player = W->SpawnActor<ACharacter>(BP->GeneratedClass, FTransform(FVector(0, 0, 110)), Spawn);
    auto *Audio = Player->FindComponentByClass<UVTGPlayerAudioComponent>();
    Expect(Audio != nullptr, TEXT("real player Blueprint contains audio component"));
    if (!Audio)
    {
        GEngine->DestroyWorldContext(W);
        W->DestroyWorld(false);
        return false;
    }
    Expect(Audio->ConcreteSteps.Num() == 6 && Audio->MetalSteps.Num() == 6 && Audio->WaterSteps.Num() == 6 &&
               Audio->WoodSteps.Num() == 6,
           TEXT("24 serialized footstep variants available to cooker"));
    for (auto *Sound : {Audio->WaterBed.Get(), Audio->PipeBed.Get()})
    {
        auto *Cue = Cast<USoundCue>(Sound);
        auto *Wave = Cue ? Cast<USoundNodeWavePlayer>(Cue->FirstNode) : nullptr;
        auto *Class = Cue ? Cue->GetSoundClass() : nullptr;
        Expect(Class && Class->Properties.bReverb && Class->Properties.Default2DReverbSendAmount > 0,
               TEXT("ambient bed actually sends to reverb rather than inheriting dry Master class"));
        Expect(Wave && Wave->GetSoundWave() && Wave->bLooping, TEXT("ambient bed has a continuous looping wave"));
    }
    auto Set = [&](FName Name, bool Value)
    {
        auto *P = FindFProperty<FBoolProperty>(Player->GetClass(), Name);
        check(P);
        P->SetPropertyValue_InContainer(Player, Value);
    };
    Set(TEXT("IsAttacking"), false);
    Set(TEXT("IsDodging?"), false);
    Set(TEXT("IsDead?"), false);
    Set(TEXT("HidePlayerNCam"), false);
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Player->GetCharacterMovement()->Velocity = FVector(150, 0, 0);
    Expect(Audio->CanPlayFootstep(), TEXT("normal walking permits animation-timed steps"));
    Player->GetCharacterMovement()->Velocity = FVector::ZeroVector;
    Expect(!Audio->CanPlayFootstep(), TEXT("stationary player does not sound like walking"));
    Player->GetCharacterMovement()->Velocity = FVector(150, 0, 0);
    for (FName Name :
         {FName(TEXT("IsAttacking")), FName(TEXT("IsDodging?")), FName(TEXT("IsDead?")), FName(TEXT("HidePlayerNCam"))})
    {
        Set(Name, true);
        Expect(!Audio->CanPlayFootstep(), *FString::Printf(TEXT("footsteps suppressed during %s"), *Name.ToString()));
        Set(Name, false);
    }
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    Expect(!Audio->CanPlayFootstep(), TEXT("falling does not trigger footsteps"));
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    auto Box = [&](FVector Position, FVector Extent)
    {
        auto *A = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Position), Spawn);
        auto *B = NewObject<UBoxComponent>(A);
        A->SetRootComponent(B);
        B->SetBoxExtent(Extent);
        B->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        B->SetCollisionObjectType(ECC_WorldStatic);
        B->SetCollisionResponseToAllChannels(ECR_Block);
        B->RegisterComponent();
        B->SetWorldLocation(Position);
        return B;
    };
    auto *Floor = Box(FVector(0, 0, -10), FVector(3000, 3000, 10));
    Expect(Audio->PlayFootstep(NAME_None), TEXT("untagged floor uses concrete fallback rather than silence"));
    Expect(!Audio->PlayFootstep(NAME_None), TEXT("simultaneous blend notifications produce only one footstep"));
    const FVector Listener(0, 0, 100);
    Expect(Audio->ProbeSpace(Listener) == 0, TEXT("open geometry is dry"));
    auto *Left = Box(FVector(-210, 0, 120), FVector(10, 2500, 120));
    auto *Right = Box(FVector(210, 0, 120), FVector(10, 2500, 120));
    auto *Ceiling = Box(FVector(0, 0, 250), FVector(2500, 2500, 10));
    Expect(Audio->ProbeSpace(Listener) == 1, TEXT("400cm enclosed pipe detected by real geometry traces"));
    Left->SetWorldLocation(FVector(-410, 0, 120));
    Right->SetWorldLocation(FVector(410, 0, 120));
    Expect(Audio->ProbeSpace(Listener) == 2, TEXT("800cm corridor gets medium reverb"));
    Left->SetWorldLocation(FVector(-1510, 0, 120));
    Right->SetWorldLocation(FVector(1510, 0, 120));
    Ceiling->SetWorldLocation(FVector(0, 0, 1200));
    Expect(Audio->ProbeSpace(Listener) == 3, TEXT("large chamber gets long reverb"));
    int32 Plants = 0, Missing = 0, Old = 0;
    TSet<UAnimSequence *> Checked;
    for (const TCHAR *Path :
         {TEXT("/Game/Characters/Sa/BlendSpace_Sa_Loco"), TEXT("/Game/Characters/Sa/Anm/BS_Sa_NoRunning"),
          TEXT("/Game/Characters/Sa/BlendSpace_Sa_Hammer"), TEXT("/Game/Characters/Sa/Anm/BS_Sa_Crouch")})
    {
        auto *BS = LoadObject<UBlendSpace>(nullptr, Path);
        check(BS);
        Expect(BS->NotifyTriggerMode == ENotifyTriggerMode::HighestWeightedAnimation,
               TEXT("locomotion blend only dispatches dominant animation notifications"));
        for (const auto &S : BS->GetBlendSamples())
            if (S.Animation && FMath::Abs(S.SampleValue.Y) > 1 && !Checked.Contains(S.Animation))
            {
                Checked.Add(S.Animation);
                int32 Count = 0;
                for (const auto &E : S.Animation->Notifies)
                {
                    if (auto *N = Cast<UVTGFootstepNotify>(E.Notify))
                    {
                        ++Count;
                        ++Plants;
                        Missing +=
                            S.Animation->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(N->FootBone) == INDEX_NONE;
                    }
                    Old += E.Notify && E.Notify->GetClass()->GetName() == TEXT("AN_FootStep_C");
                }
                Missing += Count < 2;
            }
    }
    Expect(Plants > 0 && Missing == 0 && Old == 0,
           TEXT("all normal/hammer/crouch locomotion has valid foot bones and no duplicate legacy footsteps"));
    UE_LOG(LogTemp, Display, TEXT("AUDIO TEST %d animations, %d foot plants, %d failures"), Checked.Num(), Plants,
           Fail);
    GEngine->DestroyWorldContext(W);
    W->DestroyWorld(false);
    return Fail == 0;
}
