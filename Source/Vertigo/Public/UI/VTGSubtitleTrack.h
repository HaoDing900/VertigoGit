#pragma once
#include "CoreMinimal.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"
#include "Fonts/SlateFontInfo.h"
#include "VTGSubtitleTrack.generated.h"
UCLASS()
class VERTIGO_API UMovieSceneVTGSubtitleSection : public UMovieSceneSection
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Subtitle", meta=(MultiLine=true)) FText Text;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Subtitle") FText Speaker;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Subtitle") bool bShowSpeaker=false;
};
UCLASS()
class VERTIGO_API UMovieSceneVTGSubtitleTrack : public UMovieSceneTrack
{
    GENERATED_BODY()
public:
    UMovieSceneVTGSubtitleTrack(const FObjectInitializer& ObjectInitializer);
    UPROPERTY(EditAnywhere,Category="Subtitle Style",meta=(ToolTip="Use this track's style even when the global subtitle style is enabled.")) bool bOverrideGlobalStyle=false;
    UPROPERTY(EditAnywhere, Category="Subtitle Style") FSlateFontInfo SubtitleFont;
    UPROPERTY(EditAnywhere, Category="Subtitle Style") FSlateFontInfo SpeakerFont;
    UPROPERTY(EditAnywhere, Category="Subtitle Style") FLinearColor TextColor=FLinearColor::White;
    UPROPERTY(EditAnywhere, Category="Subtitle Style") bool bOutlineEnabled=true;
    UPROPERTY(EditAnywhere, Category="Subtitle Style", meta=(ClampMin="0",ClampMax="16",EditCondition="bOutlineEnabled")) int32 OutlineSize=2;
    UPROPERTY(EditAnywhere, Category="Subtitle Style", meta=(EditCondition="bOutlineEnabled")) FLinearColor OutlineColor=FLinearColor::Black;
    virtual bool SupportsType(TSubclassOf<UMovieSceneSection> Type) const override {return Type==UMovieSceneVTGSubtitleSection::StaticClass();}
    virtual UMovieSceneSection* CreateNewSection() override;
    virtual void RemoveAllAnimationData() override {Sections.Empty();}
    virtual bool HasSection(const UMovieSceneSection& Section) const override {return Sections.Contains(&Section);}
    virtual void AddSection(UMovieSceneSection& Section) override {Sections.Add(&Section);}
    virtual void RemoveSection(UMovieSceneSection& Section) override {Sections.Remove(&Section);}
    virtual void RemoveSectionAt(int32 Index) override {Sections.RemoveAt(Index);}
    virtual bool IsEmpty() const override {return Sections.IsEmpty();}
    virtual const TArray<UMovieSceneSection*>& GetAllSections() const override {return Sections;}
#if WITH_EDITORONLY_DATA
    virtual FText GetDisplayName() const override {return NSLOCTEXT("VTG","Subtitles","Subtitles / 字幕");}
#endif
private:
    UPROPERTY() TArray<UMovieSceneSection*> Sections;
};
