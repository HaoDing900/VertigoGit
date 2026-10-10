#include "UI/VTGSequenceSkipSubsystem.h"
#include "UI/VTGDialogueHistorySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequenceTickManager.h"
#include "NarrativeComponent.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

bool UVTGSequenceSkipSubsystem::HasActiveDialogue(const UWorld* World)
{
    if (!World) return false;
    // A dialogue may be owned by the level manager, player, or another actor.
    // Do not rely on the dialogue widget already being visible this frame.
    for (TObjectIterator<UNarrativeComponent> It; It; ++It)
        if (!It->IsTemplate() && It->GetWorld() == World && IsValid(It->GetCurrentDialogue()))
            return true;
    return false;
}
bool UVTGSequenceSkipSubsystem::HandleEscape(bool bDown, bool bRepeat)
{
    if (!bDown)
    {
        if (!bConsumeRelease) return false;
        bConsumeRelease = false;
        const bool bShortPress = bHolding && !bSkipping;
        if (!bSkipping) Cancel();
        // Preserve the dialogue pause action on a short press.
        if (bShortPress)
            GetGameInstance()->GetSubsystem<UVTGDialogueHistorySubsystem>()->HandleEscape();
        return true;
    }
    if (bConsumeRelease) return true;
    auto* History = GetGameInstance()->GetSubsystem<UVTGDialogueHistorySubsystem>();
    UWorld* World = GetWorld();
    if (bRepeat || !World || World->IsPaused() || History->IsHistoryOpen() || HasActiveDialogue(World) || bSkipping) return false;
    Targets.Reset();
    for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
    {
        auto* Player = It->GetSequencePlayer();
        // Infinite/finite looping background sequences are not story cutscenes.
        if (Player && Player->IsPlaying() && !Player->IsReversed() &&
            It->PlaybackSettings.LoopCount.Value == 0 && !It->ActorHasTag(TEXT("VTG.NoSkip")))
            Targets.AddUnique(Player);
    }
    if (Targets.IsEmpty()) return false;
    bHolding = bConsumeRelease = true;
    HoldStart = FPlatformTime::Seconds();
    ShowHint();
    return true;
}

bool UVTGSequenceSkipSubsystem::AdvanceFrame(ULevelSequencePlayer* Player)
{
    if (!IsValid(Player) || !Player->IsPlaying() || Player->IsReversed() ||
        !Player->GetWorld() || Player->GetWorld()->IsPaused() || HasActiveDialogue(Player->GetWorld())) return false;
    const FQualifiedFrameTime Current = Player->GetCurrentTime();
    const FFrameTime Next = FMath::Min(Current.Time + FFrameTime(1), Player->GetEndTime().Time);
    // Play sweeps the range. Jump/GoToEndAndStop would drop event tracks.
    // Reaching EndTime invokes the engine's normal OnFinished/restore-state path.
    Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(Next, EUpdatePositionMethod::Play));
    // Pause/Stop requested by event tracks are queued until evaluation ends.
    // Drain that queue before advancing another authored frame.
    if (IsValid(Player) && Player->GetWorld())
        UMovieSceneSequenceTickManager::Get(Player->GetWorld())->RunLatentActions();
    return IsValid(Player) && Player->IsPlaying() && !HasActiveDialogue(Player->GetWorld()) &&
           Player->GetCurrentTime().Time > Current.Time;
}

void UVTGSequenceSkipSubsystem::TickInput(bool bFocused)
{
    if (!bHolding && !bSkipping) return;
    if (!bFocused) { bConsumeRelease = false; Cancel(); return; }
    if (!GetWorld() || GetWorld()->IsPaused() || HasActiveDialogue(GetWorld())) { Cancel(); return; }
    Targets.RemoveAll([this](const auto& P) { return !P.IsValid() || P->GetWorld() != GetWorld() || !P->IsPlaying(); });
    if (Targets.IsEmpty()) { Cancel(); return; }
    if (!bSkipping)
    {
        if (FPlatformTime::Seconds() - HoldStart < .8) return;
        bSkipping = true;
    }
    // Budget work so long sequences cannot freeze the application. Evaluate each
    // authored frame so event-side Pause/Stop/level changes stop the skip safely.
    const double Deadline = FPlatformTime::Seconds() + .006;
    for (int32 Frame = 0; Frame < 240 && !Targets.IsEmpty(); ++Frame)
    {
        for (int32 Index = Targets.Num() - 1; Index >= 0; --Index)
        {
            auto* Player = Targets[Index].Get();
            UWorld* PlaybackWorld = GetWorld();
            if (!AdvanceFrame(Player)) Targets.RemoveAt(Index);
            if (!GetWorld() || GetWorld() != PlaybackWorld || GetWorld()->IsPaused() || HasActiveDialogue(GetWorld())) { Cancel(); return; }
        }
        if (FPlatformTime::Seconds() >= Deadline) break;
    }
    if (Targets.IsEmpty()) Cancel();
}
void UVTGSequenceSkipSubsystem::Cancel()
{
    bHolding = bSkipping = false;
    Targets.Reset();
    HideHint();
}
void UVTGSequenceSkipSubsystem::Deinitialize()
{
    Cancel();
    Super::Deinitialize();
}
void UVTGSequenceSkipSubsystem::ShowHint()
{
    if (Hint) return;
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    if (!Viewport) return;
    Hint = SNew(SOverlay) + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(40)
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(0,0,0,.7f)).Padding(16)
        [SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(bSkipping ? TEXT("Skipping...") : TEXT("Hold Esc to skip")); })
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 20))]];
    Hint->SetVisibility(EVisibility::HitTestInvisible);
    HintViewport = Viewport;
    Viewport->AddViewportWidgetContent(Hint.ToSharedRef(), 9500);
}
void UVTGSequenceSkipSubsystem::HideHint()
{
    if (Hint && HintViewport.IsValid()) HintViewport->RemoveViewportWidgetContent(Hint.ToSharedRef());
    Hint.Reset();
    HintViewport.Reset();
}
