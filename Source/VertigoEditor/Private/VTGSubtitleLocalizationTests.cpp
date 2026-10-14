#include "VTGManualLocalization.h"
#include "VTGSubtitleLocalizationDetails.h"
#include "UI/VTGSubtitleTrack.h"
#include "UI/VTGSubtitleSubsystem.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "PropertyEditorModule.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "ISinglePropertyView.h"
#include "PropertyHandle.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Internationalization/TextLocalizationManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGSubtitleInlineLocalizationTest,"Vertigo.Localization.SubtitleInline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGSubtitleInlineLocalizationTest::RunTest(const FString&)
{
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("Automation/SubtitleLocalization")/FGuid::NewGuid().ToString();
    VTGManualLocalization::SetTestStorageDirectory(Directory);
    ON_SCOPE_EXIT {VTGManualLocalization::SetTestStorageDirectory(FString());FTextLocalizationManager::Get().RefreshResources();};
    auto* Sequence=NewObject<ULevelSequence>();Sequence->Initialize();
    auto* Track=Sequence->GetMovieScene()->AddTrack<UMovieSceneVTGSubtitleTrack>();
    auto* Section=CastChecked<UMovieSceneVTGSubtitleSection>(Track->CreateNewSection());
    Track->AddSection(*Section);Section->SetRange(TRange<FFrameNumber>(40,100));Section->bShowSpeaker=true;
    const FString Namespace=TEXT("VTG.SubtitleInline.Test.")+FGuid::NewGuid().ToString();
    const FString Source=TEXT("Origin {Name}");
    Section->Text=FText::ChangeKey(Namespace,TEXT("Text"),FText::FromString(Source));
    Section->Speaker=FText::ChangeKey(Namespace,TEXT("Speaker"),FText::FromString(TEXT("Origin Speaker")));
    auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    auto TextView=Module.CreateSingleProperty(Section,TEXT("Text"),FSinglePropertyParams());
    auto SpeakerView=Module.CreateSingleProperty(Section,TEXT("Speaker"),FSinglePropertyParams());
    auto ShowView=Module.CreateSingleProperty(Section,TEXT("bShowSpeaker"),FSinglePropertyParams());
    if(!TestTrue(TEXT("Subtitle property handles available"),TextView && SpeakerView && ShowView))return false;
    auto Text=TextView->GetPropertyHandle(),Speaker=SpeakerView->GetPropertyHandle();
    TestTrue(TEXT("Subtitle text is safe for inline editing"),VTGSubtitleLocalizationDetails::IsSafeText(Text));
    TestTrue(TEXT("Speaker display text is safe for inline editing"),VTGSubtitleLocalizationDetails::IsSafeText(Speaker));
    TestFalse(TEXT("Show Speaker flag has no language editor"),VTGSubtitleLocalizationDetails::IsSafeText(ShowView->GetPropertyHandle()));
    auto Rows=Module.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());Rows->SetObjects({Section});
    int32 InlineMenus=0;
    TFunction<void(TSharedRef<SWidget>)> VisitWidget=[&](TSharedRef<SWidget> Widget){
        if(Widget->GetType()==TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString().Contains(TEXT("多语言译文（展开编辑")))++InlineMenus;
        auto* Children=Widget->GetChildren();for(int32 Index=0;Index<Children->Num();++Index)VisitWidget(Children->GetChildAt(Index));
    };
    TFunction<void(TSharedRef<IDetailTreeNode>)> VisitNode=[&](TSharedRef<IDetailTreeNode> Tree){
        const auto Widgets=Tree->CreateNodeWidgets();if(Widgets.WholeRowWidget)VisitWidget(Widgets.WholeRowWidget.ToSharedRef());if(Widgets.ValueWidget)VisitWidget(Widgets.ValueWidget.ToSharedRef());
        TArray<TSharedRef<IDetailTreeNode>> Children;Tree->GetChildren(Children);for(auto Child:Children)VisitNode(Child);
    };
    for(auto Root:Rows->GetRootTreeNodes())VisitNode(Root);
    TestEqual(TEXT("Actual Section details contains two collapsible language editors"),InlineMenus,2);
    FString Origin,Draft,Error,Decoded;
    for(auto Property:{Text,Speaker})for(const FString Culture:{TEXT("en-US"),TEXT("zh-Hans")})
    {
        TestTrue(TEXT("Default language draft is readable"),VTGManualLocalization::GetDialogueDraft(Property,Culture,Origin,Draft,Error));
        TestTrue(TEXT("Protected references decode"),VTGManualLocalization::DecodeDraft(Origin,Draft,Decoded,Error));
        TestEqual(TEXT("Unedited translation copies its own Origin"),Decoded,Origin);
    }
    TestFalse(TEXT("Opening Section details does not save translations"),FPaths::FileExists(Directory/TEXT("ManualTranslations.json")));
    const auto TextKey=FTextInspector::GetKey(Section->Text);
    const auto SpeakerKey=FTextInspector::GetKey(Section->Speaker);
    TestTrue(TEXT("Manual subtitle translation saves"),VTGManualLocalization::SaveDialogueDraft(Text,TEXT("en-US"),Source,VTGManualLocalization::EncodeDraft(Source,TEXT("QA subtitle {Name}")),Error));
    TestTrue(TEXT("Manual speaker translation saves"),VTGManualLocalization::SaveDialogueDraft(Speaker,TEXT("en-US"),TEXT("Origin Speaker"),TEXT("QA Speaker"),Error));
    TestFalse(TEXT("Missing subtitle placeholder rejected"),VTGManualLocalization::SaveDialogueDraft(Text,TEXT("en-US"),Source,TEXT("QA subtitle"),Error));
    TestEqual(TEXT("Subtitle Origin preserved"),*FTextInspector::GetSourceString(Section->Text),Source);
    TestEqual(TEXT("Speaker Origin preserved"),*FTextInspector::GetSourceString(Section->Speaker),FString(TEXT("Origin Speaker")));
    TestTrue(TEXT("Text key preserved"),FTextInspector::GetKey(Section->Text)==TextKey);
    TestTrue(TEXT("Speaker key preserved"),FTextInspector::GetKey(Section->Speaker)==SpeakerKey);
    TestEqual(TEXT("Start time unchanged"),Section->GetInclusiveStartFrame().Value,40);
    TestEqual(TEXT("End time unchanged"),Section->GetExclusiveEndFrame().Value,100);
    TestTrue(TEXT("Show Speaker setting unchanged"),Section->bShowSpeaker);
    TestTrue(TEXT("Subtitle still resolves at the same timeline position"),UVTGSubtitleSubsystem::Resolve(Sequence,FFrameTime(50))==Section);
    VTGManualLocalization::SetTestStorageDirectory(Directory);
    VTGManualLocalization::GetDialogueDraft(Text,TEXT("en-US"),Origin,Draft,Error);
    VTGManualLocalization::DecodeDraft(Origin,Draft,Decoded,Error);
    TestEqual(TEXT("Subtitle translation survives reopening"),Decoded,FString(TEXT("QA subtitle {Name}")));
    const FString Revised=TEXT("Revised Origin {Name}");
    Section->Text=FText::ChangeKey(Namespace,TEXT("Text"),FText::FromString(Revised));
    VTGManualLocalization::GetDialogueDraft(Text,TEXT("zh-Hans"),Origin,Draft,Error);
    VTGManualLocalization::DecodeDraft(Origin,Draft,Decoded,Error);
    TestEqual(TEXT("Unedited Chinese follows revised Origin"),Decoded,Revised);
    TestFalse(TEXT("Old Origin cannot accept a stale save"),VTGManualLocalization::SaveDialogueDraft(Text,TEXT("en-US"),Source,Draft,Error));
    Section->Text=FText::ChangeKey(Namespace,TEXT("Text"),FText::FromString(Source));
    VTGManualLocalization::GetDialogueDraft(Text,TEXT("en-US"),Origin,Draft,Error);
    TestTrue(TEXT("Subtitle translations compile"),VTGManualLocalization::Compile(Error));
    FTextLocalizationResource English;
    TestTrue(TEXT("Compiled subtitle resource loads"),English.LoadFromFile(Directory/TEXT("Resources/en-US/Vertigo.locres"),0));
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    ON_SCOPE_EXIT {World->DestroyWorld(false);};
    auto* WidgetClass=LoadClass<UVTGSubtitleWidget>(nullptr,TEXT("/Game/Widget/Subtitles/WBP_SequenceSubtitles.WBP_SequenceSubtitles_C"));
    if(!TestNotNull(TEXT("Project subtitle widget"),WidgetClass))return false;
    auto* Widget=NewObject<UVTGSubtitleWidget>(World,WidgetClass);Widget->Initialize();Widget->SetLine(Section);
    auto* SubtitleText=CastChecked<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("SubtitleText")));
    auto* SpeakerText=CastChecked<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("SpeakerText")));
    FTextLocalizationManager::Get().UpdateFromLocalizationResource(English);
    TestEqual(TEXT("Already displayed subtitle receives manual translation"),SubtitleText->GetText().ToString(),FString(TEXT("QA subtitle {Name}")));
    TestEqual(TEXT("Already displayed speaker receives manual translation"),SpeakerText->GetText().ToString(),FString(TEXT("QA Speaker")));
    TestEqual(TEXT("Speaker stays visible"),SpeakerText->GetVisibility(),ESlateVisibility::HitTestInvisible);
    FTextLocalizationResource Native;
    Native.AddEntry(FTextKey(Namespace),FTextKey(TEXT("Text")),Source,Source,0);
    Native.AddEntry(FTextKey(Namespace),FTextKey(TEXT("Speaker")),TEXT("Origin Speaker"),TEXT("Origin Speaker"),0);
    FTextLocalizationManager::Get().UpdateFromLocalizationResource(Native);
    TestEqual(TEXT("Already displayed subtitle returns to Origin"),SubtitleText->GetText().ToString(),Source);
    return true;
}
#endif
