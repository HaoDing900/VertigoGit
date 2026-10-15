#include "UI/VTGSequenceSkipSubsystem.h"
#include "UI/VTGSequenceSettings.h"
#include "UI/VTGHintLayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "UObject/UnrealType.h"
#include "UI/VTGDialogueHistorySubsystem.h"
#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequenceTickManager.h"
#include "NarrativeComponent.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

namespace SequenceSkip
{
    constexpr double HoldSeconds = .8;
    constexpr double FadeSeconds = .5;
    constexpr double RevealSeconds = .25;
    constexpr double HintSeconds = 4.;
    constexpr double PlaybackMultiplier = 20.;
    bool IsSkipKey(const FKey& Key) { return Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom; }
}

void UVTGSequenceSkipSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    TravelHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &ThisClass::BeforeTravel);
}
bool UVTGSequenceSkipSubsystem::HasActiveDialogue(const UWorld* World)
{
    if (!World) return false;
    for (TObjectIterator<UNarrativeComponent> It; It; ++It)
        if (!It->IsTemplate() && It->GetWorld() == World && IsValid(It->GetCurrentDialogue())) return true;
    return false;
}
bool UVTGSequenceSkipSubsystem::IsSkipAllowed(ULevelSequence* Sequence)
{
    const auto* Settings = Sequence ? Cast<UVTGSequenceSettings>(Sequence->GetAssetUserDataOfClass(UVTGSequenceSettings::StaticClass())) : nullptr;
    return Settings && Settings->bAllowSkip;
}
bool UVTGSequenceSkipSubsystem::CanOfferSkip() const
{
    const auto* History = GetGameInstance()->GetSubsystem<UVTGDialogueHistorySubsystem>();
    const auto* World = GetWorld();
    return World && !World->IsPaused() && (!History || !History->IsHistoryOpen()) && !HasActiveDialogue(World);
}
TArray<TWeakObjectPtr<ULevelSequencePlayer>> UVTGSequenceSkipSubsystem::FindTargets() const
{
    TArray<TWeakObjectPtr<ULevelSequencePlayer>> Result;
    if (!CanOfferSkip()) return Result;
    for (TActorIterator<ALevelSequenceActor> It(GetWorld()); It; ++It)
    {
        auto* Player = It->GetSequencePlayer();
        if (Player && Player->IsPlaying() && !Player->IsReversed() && Player->GetPlayRate() > 0 &&
            It->PlaybackSettings.LoopCount.Value == 0 && IsSkipAllowed(It->GetSequence()) && !It->ActorHasTag(TEXT("VTG.NoSkip")))
            Result.AddUnique(Player);
    }
    return Result;
}
void UVTGSequenceSkipSubsystem::NotifyActivity(const FKey& Key)
{
    NotifyActivityAt(Key, FPlatformTime::Seconds());
}
void UVTGSequenceSkipSubsystem::NotifyActivityAt(const FKey& Key, double Now)
{
    if (Phase != EPhase::Idle && Phase != EPhase::Holding) return;
    if (FindTargets().IsEmpty()) return;
    HintUntil = Now + SequenceSkip::HintSeconds;
    bHintVisible = true;
    auto* Local = GetGameInstance()->GetFirstGamePlayer();
    auto* Device = Local ? Local->GetSubsystem<UCommonInputSubsystem>() : nullptr;
    const bool Gamepad = Key.IsGamepadKey();
    const FKey PromptKey = Gamepad ? EKeys::Gamepad_FaceButton_Bottom : EKeys::SpaceBar;
    const bool Found = UCommonInputPlatformSettings::Get()->TryGetInputBrush(SkipKeyBrush, PromptKey,
        Gamepad ? ECommonInputType::Gamepad : ECommonInputType::MouseAndKeyboard,
        Device ? Device->GetCurrentGamepadName() : FName(TEXT("Generic")));
    if (!Found)
    {
        SkipKeyBrush.SetResourceObject(LoadObject<UTexture2D>(nullptr, Gamepad ?
            TEXT("/Game/Inventory/InventoryXBased/UMG/Textures/Controls/Gamepad/T_Gamepad_A") :
            TEXT("/Game/Inventory/InventoryXBased/UMG/Textures/Controls/PC/T_Space_Key_Dark")));
    }
    SkipKeyBrush.ImageSize = FVector2D(48,48);
    SkipKeyBrush.DrawAs = ESlateBrushDrawType::Image;
    ShowHint();
}
bool UVTGSequenceSkipSubsystem::HandleKey(const FKey& Key, bool bDown, bool bRepeat)
{
    return HandleKeyAt(Key, bDown, bRepeat, FPlatformTime::Seconds());
}
bool UVTGSequenceSkipSubsystem::HandleKeyAt(const FKey& Key, bool bDown, bool bRepeat, double Now)
{
    if (!bDown)
    {
        if (!bConsumeRelease || Key != HeldKey) return false;
        bConsumeRelease = false;
        HeldKey = EKeys::Invalid;
        if (Phase == EPhase::Holding)
        {
            Phase = EPhase::Idle;
            HoldProgress = 0;
            Targets.Reset();
            HintUntil = Now + SequenceSkip::HintSeconds;
        }
        return true;
    }
    if (!bRepeat) NotifyActivityAt(Key, Now);
    if (!SequenceSkip::IsSkipKey(Key)) return false;
    if (bConsumeRelease || Phase == EPhase::FadeToBlack || Phase == EPhase::FastForward) return true;
    if (bRepeat || Phase != EPhase::Idle) return false;
    Targets = FindTargets();
    if (Targets.IsEmpty()) return false;
    Phase = EPhase::Holding;
    bConsumeRelease = true;
    HeldKey = Key;
    HoldStart = Now;
    return true;
}

bool UVTGSequenceSkipSubsystem::AdvanceFrame(ULevelSequencePlayer* Player)
{
    if (!IsValid(Player) || !IsSkipAllowed(Cast<ULevelSequence>(Player->GetSequence())) || !Player->IsPlaying() || Player->IsReversed() ||
        !Player->GetWorld() || Player->GetWorld()->IsPaused() || HasActiveDialogue(Player->GetWorld())) return false;
    const FQualifiedFrameTime Current = Player->GetCurrentTime();
    const FFrameTime Next = FMath::Min(Current.Time + FFrameTime(1), Player->GetEndTime().Time);
    // Play sweeps events and retains normal OnFinished / restore-state handling.
    Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(Next, EUpdatePositionMethod::Play));
    if (IsValid(Player) && Player->GetWorld()) UMovieSceneSequenceTickManager::Get(Player->GetWorld())->RunLatentActions();
    return IsValid(Player) && Player->IsPlaying() && !HasActiveDialogue(Player->GetWorld()) && Player->GetCurrentTime().Time > Current.Time;
}
void UVTGSequenceSkipSubsystem::TickInput(bool bFocused)
{
    TickAt(bFocused, FPlatformTime::Seconds());
    UpdateHintLayer();
}
void UVTGSequenceSkipSubsystem::TickAt(bool bFocused, double Now)
{
    if (!bFocused) { bConsumeRelease = false; HeldKey = EKeys::Invalid; Cancel(); return; }
    if (Phase == EPhase::Idle && !bHintVisible) return;
    if (!CanOfferSkip()) { bConsumeRelease = false; HeldKey = EKeys::Invalid; Cancel(); return; }
    if (Phase == EPhase::Reveal)
    {
        BlackOpacity = RevealOpacity * (1.f - FMath::Clamp(float((Now - FadeStart) / SequenceSkip::RevealSeconds),0.f,1.f));
        if (BlackOpacity <= 0) Cancel();
        return;
    }
    if (Phase == EPhase::Idle)
    {
        if (Now >= HintUntil || FindTargets().IsEmpty()) Cancel();
        return;
    }
    const auto Eligible = FindTargets();
    Targets.RemoveAll([&](const auto& P) { return !Eligible.Contains(P); });
    if (Targets.IsEmpty()) { BeginReveal(Now); return; }
    if (Phase == EPhase::Holding)
    {
        HoldProgress = FMath::Clamp(float((Now - HoldStart) / SequenceSkip::HoldSeconds),0.f,1.f);
        if (HoldProgress < 1.f) return;
        Phase = EPhase::FadeToBlack;
        FadeStart = Now;
        bHintVisible = false;
        // The confirmation starts the visual transition; acceleration waits until fully black.
        return;
    }
    if (Phase == EPhase::FadeToBlack)
    {
        BlackOpacity = FMath::Clamp(float((Now - FadeStart) / SequenceSkip::FadeSeconds),0.f,1.f);
        if (BlackOpacity < 1.f) return;
        Phase = EPhase::FastForward;
        LastAdvanceTime = Now;
        FrameCredits.Reset();
        return;
    }
    const double Delta = FMath::Clamp(Now - LastAdvanceTime,0.,.25);
    LastAdvanceTime = Now;
    for (const auto& Target : Targets)
        if (auto* Player = Target.Get())
        {
            // Sequencer still advances at its authored rate. Add 19x frame evaluation for 20x total.
            // Do not change global dilation or sequence play rates that event tracks may depend on.
            double& Credit = FrameCredits.FindOrAdd(Target);
            Credit = FMath::Min(240.,Credit + Delta * Player->GetCurrentTime().Rate.AsDecimal() *
                Player->GetPlayRate() * (SequenceSkip::PlaybackMultiplier - 1.));
        }
    const double Deadline = FPlatformTime::Seconds() + .006;
    for (int32 Frame = 0; Frame < 240 && !Targets.IsEmpty(); ++Frame)
    {
        bool Advanced = false;
        for (int32 Index = Targets.Num()-1; Index >= 0; --Index)
        {
            auto Target = Targets[Index];
            double& Credit = FrameCredits.FindChecked(Target);
            if (Credit < 1.) continue;
            Credit -= 1.;
            Advanced = true;
            UWorld* PlaybackWorld = GetWorld();
            const bool Progressed = AdvanceFrame(Target.Get());
            // An event can synchronously travel and clear this subsystem while it is evaluating.
            if (Phase != EPhase::FastForward) return;
            if (!GetWorld() || GetWorld() != PlaybackWorld || !CanOfferSkip()) { Cancel(); return; }
            if (!Progressed && Targets.IsValidIndex(Index) && Targets[Index] == Target) Targets.RemoveAt(Index);
        }
        if (!Advanced || FPlatformTime::Seconds() >= Deadline) break;
    }
    if (Targets.IsEmpty()) BeginReveal(Now);
}
void UVTGSequenceSkipSubsystem::BeginReveal(double Now)
{
    Targets.Reset();
    FrameCredits.Reset();
    bHintVisible = false;
    HoldProgress = 0;
    if (BlackOpacity <= 0) { Cancel(); return; }
    Phase = EPhase::Reveal;
    FadeStart = Now;
    RevealOpacity = BlackOpacity;
}
void UVTGSequenceSkipSubsystem::Cancel()
{
    Phase = EPhase::Idle;
    bHintVisible = false;
    BlackOpacity = HoldProgress = 0;
    Targets.Reset();
    FrameCredits.Reset();
    HideHint();
}
void UVTGSequenceSkipSubsystem::BeforeTravel(const FString&)
{
    bConsumeRelease = false;
    HeldKey = EKeys::Invalid;
    Cancel();
}
void UVTGSequenceSkipSubsystem::Deinitialize()
{
    FCoreUObjectDelegates::PreLoadMap.Remove(TravelHandle);
    BeforeTravel({});
    Super::Deinitialize();
}
TSharedRef<SWidget> UVTGSequenceSkipSubsystem::MakeOverlay()
{
    // Only the cinematic blackout lives above all HUDs. The prompt itself belongs to WBP_HintLayer.
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding(0)
        .BorderBackgroundColor_Lambda([this](){return FLinearColor(0,0,0,BlackOpacity);});
}
void UVTGSequenceSkipSubsystem::UpdateHintLayer()
{
    if(HintLayer.IsValid() && (HintLayer->GetWorld()!=GetWorld() || !HintLayer->IsInViewport()))
    {
        HintLayer->UpdateSkipHint(false,0,SkipKeyBrush);
        HintLayer.Reset();
    }
    if(!HintLayer.IsValid() && bHintVisible)
    {
        auto* PC=GetGameInstance()->GetFirstLocalPlayerController();
        auto* Pawn=PC?PC->GetPawn():nullptr;
        if(Pawn)if(auto* Prop=FindFProperty<FObjectPropertyBase>(Pawn->GetClass(),TEXT("HintLayerRef")))
            if(auto* Existing=Cast<UVTGHintLayer>(Prop->GetObjectPropertyValue_InContainer(Pawn));Existing && Existing->IsInViewport())HintLayer=Existing;
        if(!HintLayer.IsValid())
            for(TObjectIterator<UVTGHintLayer> It;It;++It)
                if(!It->IsTemplate() && It->GetWorld()==GetWorld() && It->IsInViewport() && It->GetOwningPlayer()==PC){HintLayer=*It;break;}
        if(!HintLayer.IsValid() && PC)
            if(auto* Class=LoadClass<UVTGHintLayer>(nullptr,TEXT("/Game/Blueprints/Widget/WBP_HintLayer.WBP_HintLayer_C")))
                if(auto* Created=CreateWidget<UVTGHintLayer>(PC,Class))
                {
                    Created->AddToViewport();HintLayer=Created;
                    if(Pawn)if(auto* Prop=FindFProperty<FObjectPropertyBase>(Pawn->GetClass(),TEXT("HintLayerRef")))Prop->SetObjectPropertyValue_InContainer(Pawn,Created);
                }
    }
    if(auto* Layer=HintLayer.Get())Layer->UpdateSkipHint(bHintVisible,HoldProgress,SkipKeyBrush);
}
void UVTGSequenceSkipSubsystem::ShowHint()
{
    UpdateHintLayer();
    if (Hint) return;
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    if (!Viewport) return;
    Hint = MakeOverlay();
    Hint->SetVisibility(EVisibility::HitTestInvisible);
    HintViewport = Viewport;
    Viewport->AddViewportWidgetContent(Hint.ToSharedRef(),9500);
}
void UVTGSequenceSkipSubsystem::HideHint()
{
    UpdateHintLayer();
    if (Hint && HintViewport.IsValid()) HintViewport->RemoveViewportWidgetContent(Hint.ToSharedRef());
    Hint.Reset();
    HintViewport.Reset();
}
