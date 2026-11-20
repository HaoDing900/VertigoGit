#include "UI/VTGDialoguePagination.h"
#include "VTGDialoguePlaybackTest.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Components/RichTextBlock.h"
#include "UObject/UnrealType.h"
#include "Widgets/SWidget.h"
#include "Engine/DataTable.h"
#include "Styling/CoreStyle.h"
#include "UI/VTGSubtitleStyleSettings.h"
#include "UI/VTGSubtitleTrack.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGPaginationSplitTest,"Vertigo.Dialogue.Pagination.Split",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGPaginationSplitTest::RunTest(const FString&)
{
 auto Fits=[](const FString& S){return S.Len()<=36;};
 const FString Original=TEXT("First sentence. Second sentence has several words. Final sentence.");
 const auto Pages=VTGDialoguePagination::Split(Original,Fits);
 TestTrue(TEXT("Overflow becomes multiple pages"),Pages.Num()>1);
 TestEqual(TEXT("Prefer sentence boundary"),Pages[0],FString(TEXT("First sentence.")));
 FString Joined;for(const auto& Page:Pages){Joined+=Page;TestTrue(TEXT("Every page fits"),Fits(Page));}
 TestEqual(TEXT("No text lost or duplicated"),Joined,Original);
 auto Manual=VTGDialoguePagination::Split(TEXT("One.[page]Two."),[](const FString&){return true;});
 TestEqual(TEXT("Explicit break even when full text fits"),Manual.Num(),2);
 TestEqual(TEXT("Marker hidden"),Manual.Last(),FString(TEXT("Two.")));
 auto Abbreviation=VTGDialoguePagination::Split(TEXT("Dr. Smith comes here, with a longer explanation."),[](const FString& S){return S.Len()<=25;});
 TestFalse(TEXT("Do not treat Dr. as a sentence"),Abbreviation[0]==TEXT("Dr."));
 auto Rich=VTGDialoguePagination::Split(TEXT("<em>One.[page]Two.</>"),[](const FString&){return true;});
 TestEqual(TEXT("First rich page balanced"),Rich[0],FString(TEXT("<em>One.</>")));
 TestEqual(TEXT("Next rich page reopens style"),Rich[1],FString(TEXT("<em>Two.</>")));
 auto* Text=NewObject<URichTextBlock>();
 FindFProperty<FFloatProperty>(Text->GetClass(),TEXT("WrapTextAt"))->SetPropertyValue_InContainer(Text,240.f);
 Text->SetAutoWrapText(false);auto Slate=Text->TakeWidget();
 const auto Measured=VTGDialoguePagination::Split(Original+Original,[&](const FString& Page){Text->SetText(FText::AsCultureInvariant(Page));Slate->SlatePrepass(1.f);return Slate->GetDesiredSize().Y<75.f;});
 TestTrue(TEXT("Actual rich-text layout makes pages"),Measured.Num()>1);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGPaginationFlowTest,"Vertigo.Dialogue.Pagination.Flow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGPaginationFlowTest::RunTest(const FString&)
{
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("PaginationTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);
 auto* Actor=World->SpawnActor<AActor>();auto* Component=NewObject<UNarrativeComponent>(Actor);Component->RegisterComponent();
 auto* D=NewObject<UVTGDialoguePlaybackTest>(Component);D->Prepare(Component,true);Component->CurrentDialogue=D;
 D->SetReadingText(FText::FromString(TEXT("Complete unchanged history text")));
 const auto* Node=D->GetPresentationNode();D->SetAutoAdvance(false);D->SetPresentationPages({TEXT("Page one"),TEXT("Page two")});
 D->NaturalFinish();TestEqual(TEXT("Auto OFF holds first page"),D->GetPresentationPageIndex(),0);
 D->ManualFinish();TestEqual(TEXT("Continue flips page"),D->GetPresentationPageIndex(),1);
 TestEqual(TEXT("No completed nodes on inner page"),D->FinishedLines,0);TestEqual(TEXT("No end events on inner page"),D->PaginationEndEvents,0);
 TestTrue(TEXT("Same node"),Node==D->GetPresentationNode());TestEqual(TEXT("Full text preserved"),D->GetCompletePresentationText().ToString(),FString(TEXT("Complete unchanged history text")));
 D->ManualFinish();TestEqual(TEXT("Final page finishes once"),D->FinishedLines,1);TestEqual(TEXT("Final page end events once"),D->PaginationEndEvents,1);
 D->Prepare(Component,true);D->SetReadingText(FText::FromString(TEXT("New complete text")));D->SetAutoAdvance(true);D->SetPresentationPages({TEXT("One"),TEXT("Two")});
 D->NaturalFinish();TestEqual(TEXT("Auto moves to second page"),D->GetPresentationPageIndex(),1);TestEqual(TEXT("Auto does not advance node early"),D->FinishedLines,1);
 D->NaturalFinish();TestEqual(TEXT("Auto final page advances node"),D->FinishedLines,2);
 D->SetDurationForTest(ELineDuration::LD_AfterDuration);TestFalse(TEXT("Fixed timing excluded"),D->CanPaginateCurrentLine());
 D->Prepare(Component,true);D->bFreeMovement=true;D->SetReadingText(FText::FromString(TEXT("Side complete line")));
 TestFalse(TEXT("Main paging still excludes free movement"),D->CanPaginateCurrentLine());
 TestTrue(TEXT("Side paging explicitly accepts free movement"),D->CanPaginateCurrentLine(true));
 D->SetPresentationPages({TEXT("Side first"),TEXT("Side second")},true);
 const int32 FinishedBefore=D->FinishedLines,EventsBefore=D->PaginationEndEvents;
 D->NaturalFinish();TestEqual(TEXT("Action-mode side auto advances page"),D->GetPresentationPageIndex(),1);
 TestEqual(TEXT("Side page does not finish node"),D->FinishedLines,FinishedBefore);
 TestEqual(TEXT("Side page does not fire end event"),D->PaginationEndEvents,EventsBefore);
 D->NaturalFinish();TestEqual(TEXT("Last side page finishes once"),D->FinishedLines,FinishedBefore+1);
 D->SetPresentationPages({},true);TestFalse(TEXT("Side permission clears with pages"),D->CanPaginateCurrentLine());
 World->GetTimerManager().ClearAllTimersForObject(D);World->DestroyWorld(false);
 return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGSideTextFitTest,"Vertigo.Dialogue.SideTextFit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGSideTextFitTest::RunTest(const FString&)
{
    auto* Label=NewObject<URichTextBlock>();
    auto* Table=NewObject<UDataTable>();Table->RowStruct=FRichTextStyleRow::StaticStruct();
    FRichTextStyleRow Default;Default.TextStyle.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),28));
    Table->AddRow(TEXT("Default"),Default);Table->AddRow(TEXT("em"),Default);
    Label->SetTextStyleSet(Table);Label->TakeWidget();
    const FString Long=TEXT("<em>This translated side dialogue must remain fully visible inside its original frame.</> We need enough words to overflow the original font size, including this second sentence about the street investigation.");
    Label->SetText(FText::AsCultureInvariant(Long));
    FVTGDialogueTextFit Fit;const FVector2D Area(360,110);
    Fit.Update(Label,Area,true);
    TestTrue(TEXT("Overflow reduces side text font"),Label->GetCurrentDefaultTextStyle().Font.Size<28);
    TestEqual(TEXT("Rich text remains complete"),Label->GetText().ToString(),Long);
    TestEqual(TEXT("Shared style table stays unchanged"),Table->FindRow<FRichTextStyleRow>(TEXT("em"),TEXT("Test"))->TextStyle.Font.Size,28.f);
    AddInfo(FString::Printf(TEXT("SIDE_UNIT font=%.1f pages=%d"),Label->GetCurrentDefaultTextStyle().Font.Size,Fit.GetPages().Num()));
    TestTrue(TEXT("Overflow at readable minimum becomes multiple pages"),Fit.GetPages().Num()>1);
    TestTrue(TEXT("Font never shrinks below readable minimum"),Label->GetCurrentDefaultTextStyle().Font.Size>=21.f);
    auto* Measure=DuplicateObject<URichTextBlock>(Label,GetTransientPackage());
    FindFProperty<FFloatProperty>(Measure->GetClass(),TEXT("WrapTextAt"))->SetPropertyValue_InContainer(Measure,Area.X-4.f);
    Measure->SetAutoWrapText(false);auto Slate=Measure->TakeWidget();
    FString Joined;
    for(const auto& Page:Fit.GetPages())
    {
        Measure->SetText(FText::AsCultureInvariant(Page));Measure->RefreshTextLayout();Slate->SlatePrepass(1.f);Joined+=Page;
        AddInfo(FString::Printf(TEXT("SIDE_UNIT_PAGE desired=%s chars=%d text=[%s]"),*Slate->GetDesiredSize().ToString(),Page.Len(),*Page));
        TestTrue(TEXT("Actual page rich text height fits frame"),Slate->GetDesiredSize().Y<=Area.Y-2.f);
        TestTrue(TEXT("Actual page rich text width fits frame"),Slate->GetDesiredSize().X<=Area.X-2.f);
    }
    TestEqual(TEXT("Paging preserves all visible text"),Joined.Replace(TEXT("<em>"),TEXT("")).Replace(TEXT("</>"),TEXT("")),Long.Replace(TEXT("<em>"),TEXT("")).Replace(TEXT("</>"),TEXT("")));
    Label->SetText(FText::AsCultureInvariant(TEXT("Short text")));Fit.Update(Label,Area,true);
    TestEqual(TEXT("Short line restores authored font"),Label->GetCurrentDefaultTextStyle().Font.Size,28.f);
    Label->SetText(FText::AsCultureInvariant(Long));Fit.Update(Label,Area,true);Fit.Update(Label,Area,false);
    TestEqual(TEXT("Origin restores authored font"),Label->GetCurrentDefaultTextStyle().Font.Size,28.f);
    TestTrue(TEXT("Origin restores original rich style table"),Label->GetTextStyleSet()==Table);
    TestEqual(TEXT("Global subtitle body default is 28"),GetDefault<UVTGSubtitleStyleSettings>()->SubtitleFont.Size,28.f);
    TestEqual(TEXT("New track body default is 28"),NewObject<UMovieSceneVTGSubtitleTrack>()->SubtitleFont.Size,28.f);
    return true;
}
#endif
