#include "UI/VTGSubtitleTrack.h"
#include "Styling/CoreStyle.h"
UMovieSceneVTGSubtitleTrack::UMovieSceneVTGSubtitleTrack(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
    SubtitleFont=FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),30);
    SpeakerFont=FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),22);
}
UMovieSceneSection* UMovieSceneVTGSubtitleTrack::CreateNewSection()
{
    auto* Section=NewObject<UMovieSceneVTGSubtitleSection>(this,NAME_None,RF_Transactional);
    Section->Text=FText::ChangeKey(TEXT("VTG.Subtitles"),FGuid::NewGuid().ToString(),FText::FromString(TEXT("字幕文本")));
    return Section;
}
