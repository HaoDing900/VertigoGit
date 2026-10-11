#include "UI/VTGSubtitleSubsystem.h"
#include "UI/VTGSubtitleTrack.h"
#include "UI/VTGSubtitleStyleSettings.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Kismet/GameplayStatics.h"

void UVTGSubtitleWidget::SetLine(const UMovieSceneVTGSubtitleSection* Line)
{
    if(const auto* Track=Line?Cast<UMovieSceneVTGSubtitleTrack>(Line->GetOuter()):nullptr)
    {
        const auto* Global=GetDefault<UVTGSubtitleStyleSettings>();
        const bool UseGlobal=Global->bUseGlobalStyle && !Track->bOverrideGlobalStyle;
        auto Apply=[&](UTextBlock* Text,const FSlateFontInfo& Base)
        {
            if(!Text)return;auto Font=Base;const bool Outline=UseGlobal?Global->bOutlineEnabled:Track->bOutlineEnabled;
            Font.OutlineSettings.OutlineSize=Outline?FMath::Clamp(UseGlobal?Global->OutlineSize:Track->OutlineSize,0,16):0;
            Font.OutlineSettings.OutlineColor=UseGlobal?Global->OutlineColor:Track->OutlineColor;Text->SetFont(Font);Text->SetColorAndOpacity(FSlateColor(UseGlobal?Global->TextColor:Track->TextColor));
        };
        Apply(SubtitleText,UseGlobal?Global->SubtitleFont:Track->SubtitleFont);Apply(SpeakerText,UseGlobal?Global->SpeakerFont:Track->SpeakerFont);
    }
    SetVisibility(Line && !Line->Text.IsEmpty()?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    if(SubtitleText)SubtitleText->SetText(Line?Line->Text:FText::GetEmpty());
    if(SpeakerText){SpeakerText->SetText(Line?Line->Speaker:FText::GetEmpty());SpeakerText->SetVisibility(Line && Line->bShowSpeaker && !Line->Speaker.IsEmpty()?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);}
}
const UMovieSceneVTGSubtitleSection* UVTGSubtitleSubsystem::Resolve(ULevelSequence* Sequence,FFrameTime Time)
{
    if(!Sequence || !Sequence->GetMovieScene())return nullptr;
    const UMovieSceneVTGSubtitleSection* Result=nullptr;
    for(auto* Track:Sequence->GetMovieScene()->GetTracks())if(auto* Subtitles=Cast<UMovieSceneVTGSubtitleTrack>(Track))
        if(!Subtitles->IsEvalDisabled())for(auto* Section:Subtitles->GetAllSections())
            if(auto* Line=Cast<UMovieSceneVTGSubtitleSection>(Section);Line && Line->IsActive() && Line->GetRange().Contains(Time.FloorToFrame()) && !Line->Text.IsEmpty())
                if(!Result || (Line->HasStartFrame() && (!Result->HasStartFrame() || Line->GetInclusiveStartFrame()>Result->GetInclusiveStartFrame())))Result=Line;
    return Result;
}
void UVTGSubtitleSubsystem::Tick(float)
{
    auto Eligible=[](ULevelSequencePlayer* P){if(!P || (!P->IsPlaying() && !P->IsPaused()))return false;auto* S=Cast<ULevelSequence>(P->GetSequence());if(!S || !S->GetMovieScene())return false;for(auto* T:S->GetMovieScene()->GetTracks())if(Cast<UMovieSceneVTGSubtitleTrack>(T))return true;return false;};
    if(!Eligible(ActivePlayer.Get())){ActivePlayer.Reset();for(TActorIterator<ALevelSequenceActor> It(GetWorld());It;++It)if(Eligible(It->GetSequencePlayer())){ActivePlayer=It->GetSequencePlayer();break;}}
    const auto* Line=ActivePlayer.IsValid()?Resolve(Cast<ULevelSequence>(ActivePlayer->GetSequence()),FFrameRate::TransformTime(ActivePlayer->GetCurrentTime().Time,ActivePlayer->GetCurrentTime().Rate,ActivePlayer->GetSequence()->GetMovieScene()->GetTickResolution())):nullptr;
    if(Line && !Widget)
        if(auto* PC=UGameplayStatics::GetPlayerController(GetWorld(),0))
            if(auto Class=WidgetClass.LoadSynchronous()){Widget=CreateWidget<UVTGSubtitleWidget>(PC,Class);if(Widget)Widget->AddToViewport(-1);}
    if(Widget)Widget->SetLine(Line);
    if(!ActivePlayer.IsValid() && Widget){Widget->RemoveFromParent();Widget=nullptr;}
}
void UVTGSubtitleSubsystem::Deinitialize(){if(Widget)Widget->RemoveFromParent();Widget=nullptr;ActivePlayer.Reset();Super::Deinitialize();}
