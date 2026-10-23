#include "VTGMailReadingComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "UObject/UnrealType.h"

void UVTGMailReadingComponent::NotifyMailReadingStarted(AActor* Reader)
{
    auto* Mail = GetOrCreate(Reader);
    if (!Mail) return;
    ++Mail->ReadingGeneration;
}

void UVTGMailReadingComponent::CaptureMailCameraReturn(AActor* Reader)
{
    auto* Mail = GetOrCreate(Reader);
    if (!Mail) return;
    if (Mail->bProtectNoteCameraCleanup && Mail->bHasCameraSnapshot) return;
    if (const auto* AlreadyReading = FindFProperty<FBoolProperty>(Reader->GetClass(), TEXT("MailCamAlreadyIn?")))
        if (Mail->bHasCameraSnapshot && AlreadyReading->GetPropertyValue_InContainer(Reader)) return;
    const auto* Pawn = Cast<APawn>(Reader);
    auto* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    Mail->ReturnController = PC;
    Mail->ReturnViewTarget = PC ? PC->GetViewTarget() : nullptr;
    Mail->bHasCameraSnapshot = PC && IsValid(PC->GetViewTarget());
    Mail->bRestoredCamera = false;
    Mail->OriginalActiveCameras.Reset();
    TInlineComponentArray<UCameraComponent*> Cameras(Reader);
    for (auto* Camera : Cameras) if (Camera->IsActive()) Mail->OriginalActiveCameras.Add(Camera);
}

void UVTGMailReadingComponent::NotifyNoteMailDialogueStarted(AActor* Reader)
{
    auto* Mail = Find(Reader);
    if (!Mail || !Mail->bProtectNoteCameraCleanup) return;
    // The old deferred cleanup already cleared this flag before the next dialogue
    // started. Its replacement must own On Finish Reading Mail, or cancelling the
    // old camera tail would leave no cleanup when the new mail dialogue ends.
    if (auto* Reading = FindFProperty<FBoolProperty>(Reader->GetClass(), TEXT("IsReadingMail")))
    {
        Reading->SetPropertyValue_InContainer(Reader, true);
        ++Mail->ReadingGeneration;
    }
}

bool UVTGMailReadingComponent::ScheduleNoteMailCleanupStep(AActor* Reader, float Delay, FName Continuation, bool bFinalStep)
{
    auto* Mail = Find(Reader);
    if (!Mail || !Mail->bProtectNoteCameraCleanup) return false;
    auto* World = Mail->GetWorld();
    auto* Function = Reader->FindFunction(Continuation);
    if (!World || !Function || Function->ParmsSize != 0) return false;
    auto& Timers = World->GetTimerManager();
    Mail->CameraStepTimers.RemoveAll([&Timers](const auto& Handle) { return !Timers.IsTimerActive(Handle); });
    const uint64 Generation = Mail->ReadingGeneration;
    FTimerHandle Handle;
    Timers.SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Mail, [Mail, Generation, Continuation, bFinalStep]()
    {
        // Every invocation owns its captured generation; no shared BP token can be overwritten.
        if (Generation != Mail->ReadingGeneration) return;
        if (bFinalStep)
        {
            auto* PC = Mail->ReturnController.Get();
            auto* Owner = Mail->GetOwner();
            if (Mail->bRestoredCamera && PC && PC->GetViewTarget() == Mail->ReturnViewTarget.Get() && Owner)
            {
                TInlineComponentArray<UCameraComponent*> Cameras(Owner);
                for (auto* Camera : Cameras) Camera->SetActive(Mail->OriginalActiveCameras.Contains(Camera));
            }
            Mail->bProtectNoteCameraCleanup = false;
            Mail->bHasCameraSnapshot = false;
            Mail->bRestoredCamera = false;
        }
        if (auto* Owner = Mail->GetOwner())
            if (auto* Next = Owner->FindFunction(Continuation); Next && Next->ParmsSize == 0)
                Owner->ProcessEvent(Next, nullptr);
    }), FMath::Max(Delay, KINDA_SMALL_NUMBER), false);
    Mail->CameraStepTimers.Add(Handle);
    return true;
}

bool UVTGMailReadingComponent::RestoreNoteMailCamera(AActor* Reader)
{
    auto* Mail = Find(Reader);
    if (!Mail || !Mail->bProtectNoteCameraCleanup || !Mail->bHasCameraSnapshot) return false;
    auto* PC = Mail->ReturnController.Get();
    const auto* Pawn = Cast<APawn>(Reader);
    if (!PC || !Pawn || Pawn->GetController() != PC || PC->GetViewTarget() != Reader) return true;
    // GetViewTarget remains the outgoing actor during a blend. A new external
    // pending target is already a takeover even before the blend completes.
    if (PC->PlayerCameraManager && PC->PlayerCameraManager->PendingViewTarget.Target &&
        PC->PlayerCameraManager->PendingViewTarget.Target != Reader) return true;
    auto* Target = Mail->ReturnViewTarget.Get();
    if (!IsValid(Target)) Target = Reader;
    Mail->ReturnViewTarget = Target;
    if (Target == Reader)
    {
        TInlineComponentArray<UCameraComponent*> Cameras(Reader);
        for (auto* Camera : Cameras) Camera->SetActive(Mail->OriginalActiveCameras.Contains(Camera));
    }
    PC->SetViewTargetWithBlend(Target, 1.f);
    Mail->bRestoredCamera = true;
    return true;
}

bool UVTGMailReadingComponent::CanRestoreMailViewTarget(AActor* Reader)
{
    const auto* Pawn = Cast<APawn>(Reader);
    const auto* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    // A later scene may already own the camera when the terminal's delayed cleanup runs.
    return Controller && Controller->GetViewTarget() == Reader;
}

UVTGMailReadingComponent* UVTGMailReadingComponent::Find(AActor* Reader)
{
    return Reader ? Reader->FindComponentByClass<UVTGMailReadingComponent>() : nullptr;
}

UVTGMailReadingComponent* UVTGMailReadingComponent::GetOrCreate(AActor* Reader)
{
    if (!IsValid(Reader)) return nullptr;
    if (auto* Existing = Find(Reader)) return Existing;
    auto* Component = NewObject<UVTGMailReadingComponent>(Reader);
    Reader->AddInstanceComponent(Component);
    Component->RegisterComponent();
    return Component;
}

void UVTGMailReadingComponent::SetHold(UObject* Source, bool bHold)
{
    if (!Source) return;
    if (bHold) Holds.Add(Source);
    else Holds.Remove(Source);
    ResumeIfReady();
}

bool UVTGMailReadingComponent::DeferMailReadingFinish(AActor* Reader, FName CleanupEvent)
{
    auto* Component = Find(Reader);
    if (!Component || CleanupEvent.IsNone()) return false;
    for (auto It = Component->Holds.CreateIterator(); It; ++It) if (!It->IsValid()) It.RemoveCurrent();
    if (!Component->Holds.Num()) return false;
    // Merely opening an unrelated Note must not change ordinary mail/camera behavior.
    Component->bProtectNoteCameraCleanup = true;
    Component->PendingCleanup = CleanupEvent;
    return true;
}

void UVTGMailReadingComponent::NoteOpened(UObject* Note)
{
    if (Holds.Num()) ActiveNote = Note;
}

void UVTGMailReadingComponent::NoteClosed(UObject* Note)
{
    if (ActiveNote.Get() != Note) return;
    ActiveNote.Reset();
    Holds.Empty();
    ResumeIfReady();
}

void UVTGMailReadingComponent::ResumeIfReady()
{
    for (auto It = Holds.CreateIterator(); It; ++It) if (!It->IsValid()) It.RemoveCurrent();
    if (Holds.Num() || PendingCleanup.IsNone()) return;
    const FName Cleanup = PendingCleanup;
    PendingCleanup = NAME_None; // Clear before dispatch: repeated close signals must be harmless.
    if (auto* Reader = GetOwner())
        if (auto* Function = Reader->FindFunction(Cleanup); Function && Function->ParmsSize == 0)
            Reader->ProcessEvent(Function, nullptr);
}

void UVTGMailReadingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (auto* World = GetWorld())
        for (auto& Handle : CameraStepTimers) World->GetTimerManager().ClearTimer(Handle);
    CameraStepTimers.Empty();
    bProtectNoteCameraCleanup = false;
    Holds.Empty();
    ActiveNote.Reset();
    PendingCleanup = NAME_None;
    Super::EndPlay(EndPlayReason);
}
