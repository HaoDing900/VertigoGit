#include "VTGMailReadingComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

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
    Holds.Empty();
    ActiveNote.Reset();
    PendingCleanup = NAME_None;
    Super::EndPlay(EndPlayReason);
}
