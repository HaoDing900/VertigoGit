#include "VTGSubtitleSampleCommandlet.h"
#include "UI/VTGSubtitleTrack.h"
#include "UI/VTGSubtitleSubsystem.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
UVTGSubtitleSampleCommandlet::UVTGSubtitleSampleCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGSubtitleSampleCommandlet::Main(const FString& Params)
{
    auto* Seq=LoadObject<ULevelSequence>(nullptr,TEXT("/Game/LevelNSequence/In_Level/TestSeq.TestSeq"));check(Seq);auto* Scene=Seq->GetMovieScene();
    if(Params.Contains(TEXT("LocalizeSample")))
    {
        const FString File=FPackageName::LongPackageNameToFilename(Seq->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/SubtitlePreview/TestSeq.before_localization_keys.uasset");check(!IFileManager::Get().FileExists(*Backup));check(IFileManager::Get().Copy(*Backup,*File,false)==COPY_OK);
        int32 Index=0;for(auto* T:Scene->GetTracks())if(auto* Track=Cast<UMovieSceneVTGSubtitleTrack>(T))for(auto* S:Track->GetAllSections())if(auto* Line=Cast<UMovieSceneVTGSubtitleSection>(S)){Line->Modify();Line->Text=FText::ChangeKey(TEXT("VTG.Subtitles.TestSeq"),FString::Printf(TEXT("Line%02d"),++Index),Line->Text);}
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(Seq->GetOutermost(),Seq,*File,Args));UE_LOG(LogTemp,Display,TEXT("SUBTITLE LOCALIZATION KEYS SAVED %d"),Index);return 0;
    }
    if(Params.Contains(TEXT("Verify")))
    {
        int Count=0;for(auto* T:Scene->GetTracks())if(auto* Track=Cast<UMovieSceneVTGSubtitleTrack>(T))for(auto* S:Track->GetAllSections())if(auto* Line=Cast<UMovieSceneVTGSubtitleSection>(S)){++Count;const auto* Found=UVTGSubtitleSubsystem::Resolve(Seq,FFrameTime(Line->GetInclusiveStartFrame()));check(Found==Line);check(!Line->bShowSpeaker);UE_LOG(LogTemp,Display,TEXT("SUBTITLE SAMPLE %s start=%d end=%d PASS"),*Line->Text.ToString(),Line->GetInclusiveStartFrame().Value,Line->GetExclusiveEndFrame().Value);}check(Count==3);return 0;
    }
    const FString File=FPackageName::LongPackageNameToFilename(Seq->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/Subtitles/TestSeq.before_subtitles.uasset");check(!IFileManager::Get().FileExists(*Backup));check(IFileManager::Get().Copy(*Backup,*File,false)==COPY_OK);
    for(auto* T:Scene->GetTracks())check(!Cast<UMovieSceneVTGSubtitleTrack>(T));
    Scene->Modify();auto* Track=Scene->AddTrack<UMovieSceneVTGSubtitleTrack>();
    const int32 Begin=Scene->GetPlaybackRange().GetLowerBoundValue().Value;const int32 End=Scene->GetPlaybackRange().GetUpperBoundValue().Value;check(End-Begin>=3);
    for(int32 I=0;I<3;++I){auto* Line=CastChecked<UMovieSceneVTGSubtitleSection>(Track->CreateNewSection());Line->Text=FText::FromString(I==0?TEXT("11111"):I==1?TEXT("22222"):TEXT("33333"));Line->SetRange(TRange<FFrameNumber>(FFrameNumber(Begin+(End-Begin)*I/3),FFrameNumber(Begin+(End-Begin)*(I+1)/3)));Track->AddSection(*Line);}
    auto* Package=CreatePackage(TEXT("/Game/Widget/Subtitles/WBP_SequenceSubtitles"));
    auto* BP=CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(UVTGSubtitleWidget::StaticClass(),Package,TEXT("WBP_SequenceSubtitles"),BPTYPE_Normal,UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass()));
    auto* Root=BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("Root"));BP->WidgetTree->RootWidget=Root;
    auto* Box=BP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("Lines"));auto* Slot=Root->AddChildToCanvas(Box);Slot->SetAnchors(FAnchors(.15f,.9f,.85f,.9f));Slot->SetAlignment(FVector2D(0,1));Slot->SetOffsets(FMargin(0,0,0,0));Slot->SetAutoSize(true);
    for(const TCHAR* Name:{TEXT("SpeakerText"),TEXT("SubtitleText")}){auto* Text=BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);Text->bIsVariable=true;auto Font=Text->GetFont();Font.Size=Name==FString(TEXT("SpeakerText"))?22:30;Font.OutlineSettings.OutlineSize=2;Text->SetFont(Font);Text->SetJustification(ETextJustify::Center);Text->SetAutoWrapText(true);Box->AddChildToVerticalBox(Text);}
    FKismetEditorUtilities::CompileBlueprint(BP);FAssetRegistryModule::AssetCreated(BP);
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(Seq->GetOutermost(),Seq,*File,Args));
    const FString WidgetFile=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());IFileManager::Get().MakeDirectory(*FPaths::GetPath(WidgetFile),true);check(UPackage::SavePackage(Package,BP,*WidgetFile,Args));UE_LOG(LogTemp,Display,TEXT("SUBTITLE SAMPLE SAVED 3 lines"));return 0;
}
