#include "UI/VTGSubtitleTrack.h"
#include "VTGSubtitlePreview.h"
#include "ISequencerModule.h"
#include "ISequencer.h"
#include "MovieSceneTrackEditor.h"
#include "ISequencerSection.h"
#include "SequencerSectionPainter.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MovieScene.h"
#include "ScopedTransaction.h"
#include "Modules/ModuleManager.h"
namespace VTGSubtitleEditor
{
FDelegateHandle Handle;
class FSection : public ISequencerSection
{
public:
    UMovieSceneSection& Section;
    FSection(UMovieSceneSection& InSection):Section(InSection){}
    virtual UMovieSceneSection* GetSectionObject() override {return &Section;}
    virtual FText GetSectionTitle() const override {return CastChecked<UMovieSceneVTGSubtitleSection>(&Section)->Text;}
    virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override {return Painter.PaintSectionBackground(FLinearColor(.12,.3,.45));}
};
class FEditor : public FMovieSceneTrackEditor
{
public:
    FEditor(TSharedRef<ISequencer> S):FMovieSceneTrackEditor(S){}
    virtual ~FEditor() override {VTGSubtitlePreview::Release(GetSequencer());}
    static TSharedRef<ISequencerTrackEditor> Create(TSharedRef<ISequencer> S){return MakeShared<FEditor>(S);}
    virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override {return Type==UMovieSceneVTGSubtitleTrack::StaticClass();}
    virtual void Tick(float) override {VTGSubtitlePreview::Update(GetSequencer());}
    virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& S,UMovieSceneTrack&,FGuid) override {return MakeShared<FSection>(S);}
    void AddLine(UMovieSceneVTGSubtitleTrack* Track)
    {
        auto S=GetSequencer();if(!S.IsValid() || !Track)return;
        const FScopedTransaction Transaction(NSLOCTEXT("VTG","AddSubtitle","Add subtitle"));Track->Modify();
        auto* Line=CastChecked<UMovieSceneVTGSubtitleSection>(Track->CreateNewSection());
        const FFrameNumber Start=S->GetLocalTime().Time.FloorToFrame();const int32 Length=S->GetFocusedMovieSceneSequence()->GetMovieScene()->GetTickResolution().AsFrameNumber(2).Value;
        Line->SetRange(TRange<FFrameNumber>(Start,Start+Length));Track->AddSection(*Line);S->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
    }
    virtual void BuildAddTrackMenu(FMenuBuilder& Menu) override
    {
        Menu.AddMenuEntry(NSLOCTEXT("VTG","SubtitleTrack","Subtitles / 字幕"),NSLOCTEXT("VTG","SubtitleTrackTip","添加按 Sequence 时间显示的字幕轨道"),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([this](){auto S=GetSequencer();if(!S.IsValid())return;const FScopedTransaction Transaction(NSLOCTEXT("VTG","AddSubtitleTrack","Add subtitle track"));auto* Scene=S->GetFocusedMovieSceneSequence()->GetMovieScene();Scene->Modify();auto* Track=Scene->AddTrack<UMovieSceneVTGSubtitleTrack>();AddLine(Track);})));
    }
    virtual void BuildTrackContextMenu(FMenuBuilder& Menu,UMovieSceneTrack* Track) override
    {if(auto* T=Cast<UMovieSceneVTGSubtitleTrack>(Track)){
      Menu.AddMenuEntry(NSLOCTEXT("VTG","AddSubtitleLine","Add subtitle at playhead / 添加字幕"),FText::GetEmpty(),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([this,T](){AddLine(T);})) );
      Menu.AddMenuEntry(NSLOCTEXT("VTG","PreviewSubtitles","字幕预览与样式"),FText::GetEmpty(),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([this](){if(auto S=GetSequencer())VTGSubtitlePreview::Show(S.ToSharedRef());})));
      Menu.AddMenuEntry(NSLOCTEXT("VTG","GlobalSubtitleStyle","全局字幕样式（独立窗口）"),FText::GetEmpty(),FSlateIcon(),FUIAction(FExecuteAction::CreateStatic(&VTGSubtitlePreview::ShowGlobalStyle)));
    }}
};
void Startup(){VTGSubtitlePreview::Startup();if(!IsRunningCommandlet())Handle=FModuleManager::LoadModuleChecked<ISequencerModule>(TEXT("Sequencer")).RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FEditor::Create));}
void Shutdown(){VTGSubtitlePreview::Shutdown();if(Handle.IsValid() && FModuleManager::Get().IsModuleLoaded(TEXT("Sequencer")))FModuleManager::GetModuleChecked<ISequencerModule>(TEXT("Sequencer")).UnRegisterTrackEditor(Handle);Handle.Reset();}
}
