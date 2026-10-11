#include "UI/VTGSubtitleTrack.h"
#include "UI/VTGSubtitleSubsystem.h"
#include "UI/VTGSubtitleStyleSettings.h"
#include "UI/VTGSequenceSettings.h"
#include "UI/VTGSequenceSkipSubsystem.h"
#include "VTGSequenceInspector.h"
#include "Misc/AutomationTest.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "WidgetBlueprint.h"
#include "Internationalization/Text.h"
#include "VTGSubtitlePreview.h"
#include "ISequencerModule.h"
#include "ISequencer.h"
#include "Modules/ModuleManager.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGSubtitleTest,"Vertigo.Sequence.Subtitles",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGSubtitleTest::RunTest(const FString&)
{
    auto* Seq=NewObject<ULevelSequence>();Seq->Initialize();auto* Scene=Seq->GetMovieScene();Scene->SetTickResolutionDirectly(FFrameRate(30000,1));Scene->SetDisplayRate(FFrameRate(30,1));Scene->SetPlaybackRange(0,90000);
    auto* Track=Scene->AddTrack<UMovieSceneVTGSubtitleTrack>();
    auto* UniqueA=Track->CreateNewSection();auto* UniqueB=Track->CreateNewSection();
    const auto KeyA=FTextInspector::GetKey(CastChecked<UMovieSceneVTGSubtitleSection>(UniqueA)->Text);const auto KeyB=FTextInspector::GetKey(CastChecked<UMovieSceneVTGSubtitleSection>(UniqueB)->Text);
    TestTrue(TEXT("New subtitle lines have distinct localization identities"),KeyA && KeyB && *KeyA!=*KeyB);
    TArray<UMovieSceneVTGSubtitleSection*> Lines;
    for(int32 I=0;I<3;++I){auto* Line=CastChecked<UMovieSceneVTGSubtitleSection>(Track->CreateNewSection());Line->Text=FText::FromString(FString::Printf(TEXT("%d"),I+1));Line->SetRange(TRange<FFrameNumber>(I*30000,(I+1)*30000));Track->AddSection(*Line);Lines.Add(Line);}
    TestTrue(TEXT("First inclusive frame"),UVTGSubtitleSubsystem::Resolve(Seq,0)==Lines[0]);
    TestTrue(TEXT("Fractional final frame stays first line"),UVTGSubtitleSubsystem::Resolve(Seq,FFrameTime(29999,.9f))==Lines[0]);
    TestTrue(TEXT("Boundary selects second line"),UVTGSubtitleSubsystem::Resolve(Seq,30000)==Lines[1]);
    TestTrue(TEXT("Third line"),UVTGSubtitleSubsystem::Resolve(Seq,60000)==Lines[2]);
    TestNull(TEXT("Exclusive end clears subtitle"),UVTGSubtitleSubsystem::Resolve(Seq,90000));
    Lines[1]->SetIsActive(false);TestNull(TEXT("Inactive section creates gap"),UVTGSubtitleSubsystem::Resolve(Seq,30000));Lines[1]->SetIsActive(true);
    TestTrue(TEXT("No overlap warning for adjacent lines"),VTGSequenceInspector::Inspect(Seq).IsEmpty());
    Lines[1]->SetRange(TRange<FFrameNumber>(29000,60000));TestFalse(TEXT("Overlap reported"),VTGSequenceInspector::Inspect(Seq).IsEmpty());Lines[1]->SetRange(TRange<FFrameNumber>(30000,60000));
    auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
    ALevelSequenceActor* Actor=nullptr;auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(World,Seq,FMovieSceneSequencePlaybackSettings(),Actor);TestNotNull(TEXT("Player with custom subtitle track evaluates"),Player);
    auto* Settings=NewObject<UVTGSequenceSettings>(Seq);Settings->bAllowSkip=true;Seq->AddAssetUserData(Settings);Player->Play();
    int SeenMask=0;for(int32 I=0;I<100 && Player->IsPlaying();++I){const FFrameTime Time=FFrameRate::TransformTime(Player->GetCurrentTime().Time,Player->GetCurrentTime().Rate,Scene->GetTickResolution());auto* Line=UVTGSubtitleSubsystem::Resolve(Seq,Time);if(Line)SeenMask|=1<<Lines.IndexOfByKey(Line);UVTGSequenceSkipSubsystem::AdvanceFrame(Player);}
    TestEqual(TEXT("Fast-forward passes through all subtitle intervals"),SeenMask,7);TestFalse(TEXT("Fast-forward ends normally"),Player->IsPlaying());
    Player->Play();Player->Pause();auto Before=Player->GetCurrentTime();TestTrue(TEXT("Paused time preserves subtitle"),UVTGSubtitleSubsystem::Resolve(Seq,Before.ConvertTo(Scene->GetTickResolution()))==Lines[0]);Player->Stop();TestFalse(TEXT("Stopped player ineligible for display"),Player->IsPlaying()||Player->IsPaused());
    auto* BP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Widget/Subtitles/WBP_SequenceSubtitles.WBP_SequenceSubtitles"));if(TestNotNull(TEXT("Sample subtitle widget asset"),BP))
    {
        auto* Widget=NewObject<UVTGSubtitleWidget>(World,BP->GeneratedClass);Widget->Initialize();auto* Speaker=CastChecked<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("SpeakerText")));auto* Text=CastChecked<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("SubtitleText")));
        Lines[0]->Speaker=FText::FromString(TEXT("Sa"));Widget->SetLine(Lines[0]);TestEqual(TEXT("Speaker hidden by default"),Speaker->GetVisibility(),ESlateVisibility::Collapsed);
        Lines[0]->bShowSpeaker=true;Widget->SetLine(Lines[0]);TestEqual(TEXT("Explicit speaker displayed"),Speaker->GetVisibility(),ESlateVisibility::HitTestInvisible);
        Lines[0]->Speaker=FText::GetEmpty();Widget->SetLine(Lines[0]);TestEqual(TEXT("Empty speaker hidden even if enabled"),Speaker->GetVisibility(),ESlateVisibility::Collapsed);
        TestTrue(TEXT("Widget renders selected line"),Text->GetText().EqualTo(Lines[0]->Text));Widget->SetLine(nullptr);TestTrue(TEXT("Cleanup clears text"),Text->GetText().IsEmpty());TestEqual(TEXT("No subtitle hides widget"),Widget->GetVisibility(),ESlateVisibility::Collapsed);
        Track->bOverrideGlobalStyle=true;Track->SubtitleFont.Size=42;Track->OutlineSize=4;Track->bOutlineEnabled=true;Widget->SetLine(Lines[0]);TestTrue(TEXT("Track font size used by actual widget"),FMath::IsNearlyEqual(Text->GetFont().Size,42.f));TestEqual(TEXT("Outline width applied"),Text->GetFont().OutlineSettings.OutlineSize,4);
        Track->bOutlineEnabled=false;Widget->SetLine(Lines[0]);TestEqual(TEXT("Outline toggle disables outline"),Text->GetFont().OutlineSettings.OutlineSize,0);
        auto* Global=GetMutableDefault<UVTGSubtitleStyleSettings>();const auto OldFont=Global->SubtitleFont;const bool OldEnabled=Global->bUseGlobalStyle;
        Global->bUseGlobalStyle=true;Global->SubtitleFont.Size=48;Track->bOverrideGlobalStyle=false;Widget->SetLine(Lines[0]);TestTrue(TEXT("Global style controls existing track"),FMath::IsNearlyEqual(Text->GetFont().Size,48.f));
        auto* OtherTrack=NewObject<UMovieSceneVTGSubtitleTrack>(Scene);auto* OtherLine=CastChecked<UMovieSceneVTGSubtitleSection>(OtherTrack->CreateNewSection());Widget->SetLine(OtherLine);TestTrue(TEXT("Another track shares global style"),FMath::IsNearlyEqual(Text->GetFont().Size,48.f));
        Global->SubtitleFont.Size=50;Widget->SetLine(Lines[0]);TestTrue(TEXT("Global edits apply live"),FMath::IsNearlyEqual(Text->GetFont().Size,50.f));
        Track->bOverrideGlobalStyle=true;Widget->SetLine(Lines[0]);TestTrue(TEXT("Explicit per-track override preserved"),FMath::IsNearlyEqual(Text->GetFont().Size,42.f));
        Track->bOverrideGlobalStyle=false;Global->bUseGlobalStyle=false;Widget->SetLine(Lines[0]);TestTrue(TEXT("Global switch off restores local style"),FMath::IsNearlyEqual(Text->GetFont().Size,42.f));
        Global->SubtitleFont=OldFont;Global->bUseGlobalStyle=OldEnabled;
    }
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGSubtitlePreviewTest,"Vertigo.Sequence.SubtitleEditorPreview",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGSubtitlePreviewTest::RunTest(const FString&)
{
    auto* Sequence=LoadObject<ULevelSequence>(nullptr,TEXT("/Game/LevelNSequence/In_Level/TestSeq.TestSeq"));if(!TestNotNull(TEXT("TestSeq"),Sequence))return false;
    FSequencerInitParams Params;Params.RootSequence=Sequence;
    auto Sequencer=FModuleManager::LoadModuleChecked<ISequencerModule>(TEXT("Sequencer")).CreateSequencer(Params);
    VTGSubtitlePreview::Show(Sequencer);
    Sequencer->SetLocalTimeDirectly(0,false);VTGSubtitlePreview::Update(Sequencer);TestEqual(TEXT("Editor preview shows first subtitle"),VTGSubtitlePreview::DisplayedText().ToString(),FString(TEXT("11111")));
    Sequencer->SetLocalTimeDirectly(40000,false);VTGSubtitlePreview::Update(Sequencer);TestEqual(TEXT("Scrubbing updates preview"),VTGSubtitlePreview::DisplayedText().ToString(),FString(TEXT("22222")));
    VTGSubtitlePreview::SetEnabled(false);VTGSubtitlePreview::Update(Sequencer);TestTrue(TEXT("Preview switch clears display"),VTGSubtitlePreview::DisplayedText().IsEmpty());
    VTGSubtitlePreview::SetEnabled(true);VTGSubtitlePreview::Update(Sequencer);TestEqual(TEXT("Re-enable restores current line"),VTGSubtitlePreview::DisplayedText().ToString(),FString(TEXT("22222")));
    VTGSubtitlePreview::Release(Sequencer);TestTrue(TEXT("Sequencer close clears preview"),VTGSubtitlePreview::DisplayedText().IsEmpty());
    return true;
}
#endif


