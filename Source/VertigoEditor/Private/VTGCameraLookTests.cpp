#include "Animation/VTGCameraLookAnimInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

bool VTGTestCameraLookPose()
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    auto Init = UWorld::InitializationValues()
                    .AllowAudioPlayback(false)
                    .RequiresHitProxies(false)
                    .CreatePhysicsScene(true)
                    .CreateNavigation(false)
                    .CreateAISystem(false)
                    .ShouldSimulatePhysics(false)
                    .SetTransactional(false);
    auto *W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CameraLookTest"), nullptr, true, ERHIFeatureLevel::Num,
                                  &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto *BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));
    check(BP);
    auto *P = W->SpawnActor<ACharacter>(BP->GeneratedClass, FTransform(FVector(0, 0, 100)), Spawn);
    auto *PC = W->SpawnActor<APlayerController>();
    PC->Possess(P);
    if (!PC->PlayerCameraManager)
        PC->PlayerCameraManager = W->SpawnActor<APlayerCameraManager>();
    PC->PlayerCameraManager->InitializeFor(PC);
    PC->SetViewTarget(P);
    auto *Mesh = P->GetMesh();
    Mesh->InitAnim(true);
    auto *A = Cast<UVTGCameraLookAnimInstance>(Mesh->GetAnimInstance());
    check(A);
    auto Set = [&](FName Name, bool Value)
    {
        auto *Prop = FindFProperty<FBoolProperty>(P->GetClass(), Name);
        check(Prop);
        Prop->SetPropertyValue_InContainer(P, Value);
    };
    for (FName Name : {FName(TEXT("IsAttacking")), FName(TEXT("IsDodging?")), FName(TEXT("IsDead?")),
                       FName(TEXT("HidePlayerNCam")), FName(TEXT("bIsLooking")), FName(TEXT("FixedCameraLevel?"))})
        Set(Name, false);
    int Fail = 0;
    auto Expect = [&](bool OK, const TCHAR *Text)
    {
        Fail += !OK;
        UE_LOG(LogTemp, Display, TEXT("HEAD POSE %s: %s"), OK ? TEXT("PASS") : TEXT("FAIL"), Text);
    };
    auto SetCamera = [&](FRotator R)
    {
        FMinimalViewInfo POV;
        POV.Rotation = R;
        POV.Location = P->GetPawnViewLocation();
        PC->PlayerCameraManager->FillCameraCache(POV);
    };
    auto Frames = [&](int Count)
    {
        for (int I = 0; I < Count; ++I)
        {
            // Advance look smoothing while keeping the authored idle pose at
            // one animation time; idle head motion must not affect comparison.
            A->NativeUpdateAnimation(1.f / 60.f);
            Mesh->TickAnimation(0.f, false);
            Mesh->RefreshBoneTransforms();
        }
    };
    SetCamera(FRotator::ZeroRotator);
    Frames(60);
    const FQuat Baseline = Mesh->GetSocketQuaternion(TEXT("head_x"));
    const FTransform Body = P->GetActorTransform();
    SetCamera(FRotator(20, 45, 0));
    Frames(90);
    const FQuat Look = Mesh->GetSocketQuaternion(TEXT("head_x"));
    const FRotator BoneDelta = (Look * Baseline.Inverse()).RotateVector(P->GetActorForwardVector()).Rotation();
    UE_LOG(LogTemp, Display, TEXT("HEAD POSE measured world head delta=%s neck=%s head=%s"), *BoneDelta.ToString(),
           *A->CameraNeckOffset.ToString(), *A->CameraHeadOffset.ToString());
    Expect(BoneDelta.Yaw > 25 && BoneDelta.Yaw < 65 && BoneDelta.Pitch > 8 && BoneDelta.Pitch < 38,
           TEXT("actual final head bone follows camera right/up through existing graph and post process"));
    Expect(P->GetActorTransform().Equals(Body), TEXT("camera head look does not rotate or move the character body"));
    // Fixed-camera mode can keep the pawn as view target, so the existing
    // view-target guard alone is insufficient. Exercise the authored state.
    Set(TEXT("FixedCameraLevel?"), true);
    Frames(90);
    Expect(A->CameraNeckOffset.IsNearlyZero(.05f) && A->CameraHeadOffset.IsNearlyZero(.05f),
           TEXT("fixed-camera state returns camera head and neck offsets to neutral"));
    Expect(Mesh->GetSocketQuaternion(TEXT("head_x")).Equals(Baseline, .01f),
           TEXT("actual head bone returns to its animation pose in fixed camera"));
    SetCamera(FRotator(-20, -45, 0));
    Frames(90);
    Expect(A->CameraNeckOffset.IsNearlyZero(.05f) && A->CameraHeadOffset.IsNearlyZero(.05f),
           TEXT("camera motion cannot turn the head while fixed-camera state is active"));
    Set(TEXT("FixedCameraLevel?"), false);
    SetCamera(FRotator(20, 45, 0));
    Frames(90);
    Expect(Mesh->GetSocketQuaternion(TEXT("head_x")).Equals(Look, .01f),
           TEXT("leaving fixed camera restores camera head tracking"));
    for (FName Name : {FName(TEXT("IsAttacking")), FName(TEXT("IsDodging?")), FName(TEXT("bIsLooking")),
                       FName(TEXT("HidePlayerNCam"))})
    {
        Set(Name, true);
        for (int I = 0; I < 90; ++I)
            A->NativeUpdateAnimation(1.f / 60.f);
        Expect(A->CameraNeckOffset.IsNearlyZero(.05f) && A->CameraHeadOffset.IsNearlyZero(.05f),
               *FString::Printf(TEXT("camera influence yields to %s"), *Name.ToString()));
        Set(Name, false);
        for (int I = 0; I < 90; ++I)
            A->NativeUpdateAnimation(1.f / 60.f);
    }
    W->DestroyWorld(false);
    GEngine->DestroyWorldContext(W);
    return Fail == 0;
}
