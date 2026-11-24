#include "VTGAssetDisplayLocalizationDetails.h"
#include "VTGManualLocalization.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "HAL/IConsoleManager.h"
#include "PropertyEditorModule.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/TextLocalizationResource.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGLoadingPauseInlineTest,"Vertigo.Localization.LoadingPauseInline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGLoadingPauseInlineTest::RunTest(const FString&)
{
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("Automation/LoadingPauseLocalization")/FGuid::NewGuid().ToString();
    VTGManualLocalization::SetTestStorageDirectory(Directory);
    ON_SCOPE_EXIT {VTGManualLocalization::SetTestStorageDirectory(FString());FTextLocalizationManager::Get().RefreshResources();};
    auto* Loading=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Widget/Loading/WBP_VTGLoadingScreen"));
    auto* Pause=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/UI/WBP_PauseMenu"));
    if(!TestTrue(TEXT("Both requested Widget Blueprints load"),Loading && Pause))return false;
    TArray<UTextBlock*> Labels{Cast<UTextBlock>(Loading->WidgetTree->FindWidget(TEXT("LoadingLabel")))};
    for(const FName Name:{FName(TEXT("B_Continue")),FName(TEXT("B_Language")),FName(TEXT("B_Exit"))})
    {
        const auto* Button=Cast<UButton>(Pause->WidgetTree->FindWidget(Name));
        Labels.Add(Button?Cast<UTextBlock>(Button->GetContent()):nullptr);
    }
    auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    TArray<TSharedRef<IPropertyRowGenerator>> KeepRows;
    auto FindText=[&](UTextBlock* Label,int32& Menus)
    {
        auto Rows=Module.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());Rows->SetObjects({Label});KeepRows.Add(Rows);
        TSharedPtr<IPropertyHandle> Text;Menus=0;
        TFunction<void(TSharedRef<SWidget>)> VisitWidget=[&](TSharedRef<SWidget> W)
        {
            if(W->GetType()==TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(W)->GetText().ToString().Contains(TEXT("多语言译文（展开编辑")))++Menus;
            auto* Children=W->GetChildren();for(int32 I=0;I<Children->Num();++I)VisitWidget(Children->GetChildAt(I));
        };
        TFunction<void(TSharedRef<IDetailTreeNode>)> VisitNode=[&](TSharedRef<IDetailTreeNode> Node)
        {
            auto P=Node->CreatePropertyHandle();if(P && P->IsValidHandle() && P->GetProperty()->GetFName()==TEXT("Text"))Text=P;
            const auto W=Node->CreateNodeWidgets();if(W.WholeRowWidget)VisitWidget(W.WholeRowWidget.ToSharedRef());if(W.ValueWidget)VisitWidget(W.ValueWidget.ToSharedRef());
            TArray<TSharedRef<IDetailTreeNode>> Children;Node->GetChildren(Children);for(auto Child:Children)VisitNode(Child);
        };
        for(auto Root:Rows->GetRootTreeNodes())VisitNode(Root);
        return Text;
    };
    FString Error,Origin,Draft;
    TArray<bool> Dirty;
    for(int32 I=0;I<Labels.Num();++I)
    {
        auto* Label=Labels[I];if(!TestNotNull(TEXT("Requested label exists"),Label))return false;
        Dirty.Add(Label->GetOutermost()->IsDirty());
        int32 Menus;auto Property=FindText(Label,Menus);
        TestEqual(TEXT("Requested Designer Text has one folded multilingual editor"),Menus,1);
        if(!TestTrue(TEXT("Requested label is safe to translate"),VTGAssetDisplayLocalizationDetails::IsSafeText(Property)))return false;
        if(!TestTrue(TEXT("Requested label has a stable editable Origin"),VTGManualLocalization::GetDialogueDraft(Property,TEXT("en-US"),Origin,Draft,Error)))
        {AddError(Error);return false;}
        const FString Translation=FString::Printf(TEXT("QA Label %d"),I);
        TestTrue(TEXT("Manual translation saves to isolated catalog"),VTGManualLocalization::SaveDialogueDraft(Property,TEXT("en-US"),Origin,Translation,Error));
    }
    // UMG edits transient preview trees too. Their translations must still point at
    // the persisted source asset so Compile can revalidate the original identities.
    for(auto* BP:{Loading,Pause})
    {
        auto* Preview=NewObject<UUserWidget>(GetTransientPackage(),BP->GeneratedClass);
        Preview->WidgetTree=DuplicateObject<UWidgetTree>(BP->WidgetTree,Preview);
        UTextBlock* PreviewText=nullptr;
        if(BP==Loading)PreviewText=Cast<UTextBlock>(Preview->WidgetTree->FindWidget(TEXT("LoadingLabel")));
        else if(auto* Button=Cast<UButton>(Preview->WidgetTree->FindWidget(TEXT("B_Language"))))PreviewText=Cast<UTextBlock>(Button->GetContent());
        if(!TestNotNull(TEXT("Designer preview label exists"),PreviewText))return false;
        int32 PreviewMenus;auto Property=FindText(PreviewText,PreviewMenus);
        TestEqual(TEXT("Transient Designer preview retains multilingual editor"),PreviewMenus,1);
        TestTrue(TEXT("Preview draft reads without replacing source context"),VTGManualLocalization::GetDialogueDraft(Property,TEXT("en-US"),Origin,Draft,Error));
    }
    int32 Menus;
    auto* Unrelated=NewObject<UTextBlock>();auto UnrelatedText=FindText(Unrelated,Menus);
    TestEqual(TEXT("Unrelated TextBlocks have no added editor"),Menus,0);
    TestFalse(TEXT("Unrelated widget is not approved for inline translation"),VTGAssetDisplayLocalizationDetails::IsSafeText(UnrelatedText));
    auto* Heading=Cast<UTextBlock>(Pause->WidgetTree->FindWidget(TEXT("TextBlock_81")));
    if(TestNotNull(TEXT("Pause heading exists"),Heading))
    {
        auto HeadingText=FindText(Heading,Menus);
        TestEqual(TEXT("Pause heading outside requested buttons is unchanged"),Menus,0);
        TestFalse(TEXT("Only requested pause button labels are allowed"),VTGAssetDisplayLocalizationDetails::IsSafeText(HeadingText));
    }
    if(!TestTrue(TEXT("Widget translations compile with persisted source validation"),VTGManualLocalization::Compile(Error)))
    {AddError(Error);return false;}
    FTextLocalizationResource Resource;
    TestTrue(TEXT("Compiled test resource loads"),Resource.LoadFromFile(Directory/TEXT("Resources/en-US/Vertigo.locres"),0));
    FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);
    for(int32 I=0;I<Labels.Num();++I)
    {
        TestEqual(TEXT("Existing FText resolves manual translation"),Labels[I]->GetText().ToString(),FString::Printf(TEXT("QA Label %d"),I));
        TestEqual(TEXT("Translation does not dirty authored layout"),Labels[I]->GetOutermost()->IsDirty(),Dirty[I]);
    }
    return true;
}
static FAutoConsoleCommand GVTGCheckLoadingPauseLocalization(
    TEXT("VTG.CheckLoadingPauseLocalization"),TEXT("Check LoadingLabel and pause button translation editors using isolated translations."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        class FDirectTest:public FVTGLoadingPauseInlineTest
        {public:FDirectTest():FVTGLoadingPauseInlineTest(TEXT("VTGDirectLoadingPause")){}using FVTGLoadingPauseInlineTest::RunTest;};
        FDirectTest Test;const bool Ran=Test.RunTest({});
        FAutomationTestExecutionInfo Info;Test.GetExecutionInfo(Info);
        UE_LOG(LogTemp,Display,TEXT("LOADING PAUSE INLINE CHECK: %s"),Ran && !Test.HasAnyErrors()?TEXT("PASS"):TEXT("FAIL"));
        for(const auto& Entry:Info.GetEntries())UE_LOG(LogTemp,Display,TEXT("LOADING PAUSE CHECK RESULT: %s"),*Entry.ToString());
    }));
#endif
