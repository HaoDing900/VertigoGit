#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "VTGSubtitleSubsystem.generated.h"
class ULevelSequence;
class ULevelSequencePlayer;
class UMovieSceneVTGSubtitleSection;
class UTextBlock;
UCLASS()
class VERTIGO_API UVTGSubtitleWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetLine(const UMovieSceneVTGSubtitleSection* Line);
protected:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> SubtitleText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> SpeakerText;
};
UCLASS()
class VERTIGO_API UVTGSubtitleSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override {return Type==EWorldType::Game || Type==EWorldType::PIE;}
    virtual TStatId GetStatId() const override {RETURN_QUICK_DECLARE_CYCLE_STAT(UVTGSubtitleSubsystem,STATGROUP_Tickables);}
    virtual void Tick(float DeltaTime) override;
    virtual void Deinitialize() override;
    static const UMovieSceneVTGSubtitleSection* Resolve(ULevelSequence* Sequence,FFrameTime Time);
private:
    TWeakObjectPtr<ULevelSequencePlayer> ActivePlayer;
    UPROPERTY(Transient) TObjectPtr<UVTGSubtitleWidget> Widget;
    UPROPERTY() TSoftClassPtr<UVTGSubtitleWidget> WidgetClass= TSoftClassPtr<UVTGSubtitleWidget>(FSoftObjectPath(TEXT("/Game/Widget/Subtitles/WBP_SequenceSubtitles.WBP_SequenceSubtitles_C")));
};
