#include "VTGManualLocalization.h"
#include "UI/VTGLocalizationSubsystem.h"
#include "UI/VTGMainMenu.h"
#include "UI/VTGSubtitleTrack.h"
#include "UI/VTGSubtitleSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Internationalization/TextLocalizationManager.h"
#include "PropertyEditorModule.h"
#include "ISinglePropertyView.h"
#include "PropertyHandle.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SNullWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ComboBoxString.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGLocalizationValidationTest,"Vertigo.Localization.ProtectedStructure",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGLocalizationValidationTest::RunTest(const FString&)
{
    FString Error;
    const FString Source=TEXT("<em>{Name}</> has {Count} items.");
    TestTrue(TEXT("Manual prose and placeholder position may change"),VTGManualLocalization::Validate(Source,TEXT("{Count} items belong to <em>{Name}</>."),Error));
    TestFalse(TEXT("Missing placeholder is blocked"),VTGManualLocalization::Validate(Source,TEXT("<em>{Name}</> has items."),Error));
    TestFalse(TEXT("Duplicate placeholder is blocked"),VTGManualLocalization::Validate(Source,TEXT("<em>{Name}</> {Name} {Count}"),Error));
    TestFalse(TEXT("Changed tag is blocked"),VTGManualLocalization::Validate(Source,TEXT("<strong>{Name}</> {Count}"),Error));
    TestFalse(TEXT("Broken formatting is blocked"),VTGManualLocalization::Validate(TEXT("Hello {Name}"),TEXT("Hello {Name} {"),Error));
    TestTrue(TEXT("Empty translation intentionally restores fallback"),VTGManualLocalization::Validate(Source,TEXT(""),Error));
    FString Decoded;
    const FString Draft=VTGManualLocalization::EncodeDraft(Source,TEXT("{Count} items belong to <em>{Name}</>."));
    TestFalse(TEXT("Raw placeholder syntax is not exposed in draft"),Draft.Contains(TEXT("{Name}")));
    TestTrue(TEXT("Protected references restore original program-owned structure"),VTGManualLocalization::DecodeDraft(Source,Draft,Decoded,Error));
    TestEqual(TEXT("Only prose and positions change"),Decoded,FString(TEXT("{Count} items belong to <em>{Name}</>.")));
    TestFalse(TEXT("Raw tag injection is rejected"),VTGManualLocalization::DecodeDraft(Source,TEXT("<em>injected</>"),Decoded,Error));
    TestFalse(TEXT("Unknown structure reference is rejected"),VTGManualLocalization::DecodeDraft(Source,TEXT("⟦99⟧"),Decoded,Error));
    TestTrue(TEXT("Origin is available in editor builds"),UVTGLocalizationSubsystem::GetLanguages().Contains(TEXT("origin")));
    auto* Settings=NewObject<UVTGLocalizationSubsystem>(NewObject<UGameInstance>());
    TestFalse(TEXT("Unknown language cannot change settings"),Settings->SetLanguage(TEXT("invalid-language")));
    TestTrue(TEXT("Invalid request leaves state untouched"),Settings->GetLanguage().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGLocalizationTextTest,"Vertigo.Localization.DisplayIdentity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGLocalizationTextTest::RunTest(const FString&)
{
    const FString Namespace=TEXT("VTG.Localization.Automation");
    FText Original=FText::ChangeKey(Namespace,TEXT("Line"),FText::FromString(TEXT("Origin {Name}")));
    FFormatNamedArguments Args;Args.Add(TEXT("Name"),FText::AsCultureInvariant(TEXT("Character_ID")));
    FText Formatted=FText::Format(Original,Args);
    FTextLocalizationResource Resource;
    Resource.AddEntry(FTextKey(Namespace),FTextKey(TEXT("Line")),TEXT("Origin {Name}"),TEXT("{Name} QA translated"),0);
    FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);
    TestEqual(TEXT("Already formatted text rebuilds without replaying dialogue"),Formatted.ToString(),FString(TEXT("Character_ID QA translated")));
    TestEqual(TEXT("Source stays Origin"),*FTextInspector::GetSourceString(Original),FString(TEXT("Origin {Name}")));
    TestEqual(TEXT("Internal text key remains unchanged"),FTextInspector::GetKey(Original).GetValue(),FString(TEXT("Line")));
    Resource=FTextLocalizationResource();Resource.AddEntry(FTextKey(Namespace),FTextKey(TEXT("Line")),TEXT("Origin {Name}"),TEXT("Origin {Name}"),0);
    FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);
    TestEqual(TEXT("Origin display restored"),Formatted.ToString(),FString(TEXT("Origin Character_ID")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGLocalizationEditorTest,"Vertigo.Localization.EditorEntry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGLocalizationEditorTest::RunTest(const FString&)
{
    auto* Section=NewObject<UMovieSceneVTGSubtitleSection>();
    Section->Text=FText::ChangeKey(TEXT("VTG.Localization.EditorTest"),TEXT("ReadonlySource"),FText::FromString(TEXT("Origin test")));
    auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    auto View=Module.CreateSingleProperty(Section,TEXT("Text"),FSinglePropertyParams());
    if(!TestTrue(TEXT("Subtitle text property can be inspected"),View.IsValid()))return false;
    FOnGenerateGlobalRowExtensionArgs Args;Args.PropertyHandle=View->GetPropertyHandle();
    TArray<FPropertyRowExtensionButton> Buttons;Module.GetGlobalRowExtensionDelegate().Broadcast(Args,Buttons);
    bool Found=false;
    const bool BeforeDirty=Section->GetOutermost()->IsDirty();
    for(const auto& Button:Buttons)if(Button.Label.Get().ToString()==TEXT("多语言 / 标识风险")){Found=true;break;}
    TestTrue(TEXT("Field has a localization entry action"),Found);
    // Build the real panel without requesting a native window in UnrealEditor-Cmd.
    auto Tab=VTGManualLocalization::MakeTestPanel(View->GetPropertyHandle());
    TestTrue(TEXT("Translation panel builds"),Tab->GetContent()!=SNullWidget::NullWidget);
    TestEqual(TEXT("Opening panel preserves source"),*FTextInspector::GetSourceString(Section->Text),FString(TEXT("Origin test")));
    TestEqual(TEXT("Opening panel does not dirty source package"),Section->GetOutermost()->IsDirty(),BeforeDirty);
    // Actual menu must inherit the class that provides the language selector.
    auto* MenuClass=LoadClass<UUserWidget>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/UI/MainMenu/WBP_MainMenu.WBP_MainMenu_C"));
    TestTrue(TEXT("Existing main menu uses VTGMainMenu"),MenuClass && MenuClass->IsChildOf(UVTGMainMenu::StaticClass()));
    if(MenuClass)
    {
        FString Error;
        TestTrue(TEXT("Read-only gather includes nested widget objects"),VTGManualLocalization::GatherAsset(MenuClass,Error));
        TestTrue(TEXT("Existing Continue button is available for manual translation"),VTGManualLocalization::HasCollectedTextForTests(TEXT("VTGMainMenu"),TEXT("Continue")));
    }
    return true;
}
#endif
