#include "VTGManualLocalization.h"
#include "VTGDialogueLocalizationDetails.h"
#include "NarrativeDialogueDetailsExtension.h"
#include "DialogueSM.h"
#include "Dialogue.h"
#include "DetailLayoutBuilder.h"
#include "PropertyEditorModule.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "PropertyHandle.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Internationalization/TextLocalizationManager.h"
#include "HAL/IConsoleManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/VTGLocalizationSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGDialogueInlineLocalizationTest,"Vertigo.Localization.DialogueInline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGDialogueInlineLocalizationTest::RunTest(const FString&)
{
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("Automation/DialogueLocalization")/FGuid::NewGuid().ToString();
    VTGManualLocalization::SetTestStorageDirectory(Directory);
    ON_SCOPE_EXIT {VTGManualLocalization::SetTestStorageDirectory(FString());};
    const FString Namespace=TEXT("VTG.DialogueInline.Test.")+FGuid::NewGuid().ToString();
    const FString Source=TEXT("Origin <em>{Name}</>");
    auto* Node=NewObject<UDialogueNode_Player>();
    Node->Line.Text=FText::ChangeKey(Namespace,TEXT("Main"),FText::FromString(Source));
    Node->AlternativeLines.AddDefaulted();
    Node->AlternativeLines[0].Text=FText::ChangeKey(Namespace,TEXT("Alternative"),FText::FromString(TEXT("Alternative Origin")));
    TSharedPtr<IPropertyHandle> Text,Option,Hint,Alternative;
    int32 Hooks=0;
    const auto Hook=GetNarrativeDialogueDetailsExtension().AddLambda([&](IDetailLayoutBuilder& Layout){
        ++Hooks;
        Text=Layout.GetProperty(TEXT("Line"),UDialogueNode::StaticClass())->GetChildHandle(TEXT("Text"));
        Option=Layout.GetProperty(TEXT("OptionText"),UDialogueNode_Player::StaticClass());
        Hint=Layout.GetProperty(TEXT("HintText"),UDialogueNode_Player::StaticClass());
        auto Array=Layout.GetProperty(TEXT("AlternativeLines"),UDialogueNode::StaticClass())->AsArray();
        if(Array)Alternative=Array->GetElement(0)->GetChildHandle(TEXT("Text"));
    });
    ON_SCOPE_EXIT {GetNarrativeDialogueDetailsExtension().Remove(Hook);};
    auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    auto Rows=Module.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());Rows->SetObjects({Node});
    if(!TestTrue(TEXT("Narrative node details invokes project extension"),Hooks>0 && Text.IsValid()))return false;
    TestTrue(TEXT("Main line is eligible"),VTGDialogueLocalizationDetails::IsSafeText(Text));
    TestTrue(TEXT("Alternative line is eligible"),VTGDialogueLocalizationDetails::IsSafeText(Alternative));
    TestTrue(TEXT("Option and hint use display-only fields"),VTGDialogueLocalizationDetails::IsSafeText(Option)&&VTGDialogueLocalizationDetails::IsSafeText(Hint));
    auto* Dialogue=NewObject<UDialogue>();
    Dialogue->Speakers.AddDefaulted();Dialogue->Speakers[0].SpeakerID=TEXT("Character_ID");
    Dialogue->Speakers[0].SpeakerName=FText::ChangeKey(Namespace,TEXT("Speaker"),FText::FromString(TEXT("Origin Character")));
    Dialogue->PlayerSpeakerInfo.SpeakerName=FText::ChangeKey(Namespace,TEXT("PlayerName"),FText::FromString(TEXT("Origin Player")));
    auto SpeakerRows=Module.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());SpeakerRows->SetObjects({Dialogue});
    TSharedPtr<IPropertyHandle> SpeakerName,SpeakerID;
    TFunction<void(TSharedRef<IDetailTreeNode>)> FindSpeaker=[&](TSharedRef<IDetailTreeNode> Tree){
        auto Property=Tree->CreatePropertyHandle();
        if(Property && Property->IsValidHandle() && Property->GetProperty()->GetFName()==TEXT("Speakers"))
        {
            if(auto Array=Property->AsArray()){auto Element=Array->GetElement(0);SpeakerName=Element->GetChildHandle(TEXT("SpeakerName"));SpeakerID=Element->GetChildHandle(TEXT("SpeakerID"));}
        }
        TArray<TSharedRef<IDetailTreeNode>> Children;Tree->GetChildren(Children);for(auto Child:Children)FindSpeaker(Child);
    };
    for(auto Root:SpeakerRows->GetRootTreeNodes())FindSpeaker(Root);
    TestTrue(TEXT("Character display name is eligible for inline translation"),VTGDialogueLocalizationDetails::IsSafeText(SpeakerName));
    TestFalse(TEXT("Character ID has no translation editor"),VTGDialogueLocalizationDetails::IsSafeText(SpeakerID));
    int32 InlineMenus=0;
    TFunction<void(TSharedRef<SWidget>)> VisitWidget=[&](TSharedRef<SWidget> Widget){
        if(Widget->GetType()==TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString().Contains(TEXT("多语言译文（展开编辑")))++InlineMenus;
        auto* Children=Widget->GetChildren();for(int32 Index=0;Index<Children->Num();++Index)VisitWidget(Children->GetChildAt(Index));
    };
    TFunction<void(TSharedRef<IDetailTreeNode>)> VisitNode=[&](TSharedRef<IDetailTreeNode> Tree){
        FNodeWidgets Widgets=Tree->CreateNodeWidgets();
        if(Widgets.WholeRowWidget)VisitWidget(Widgets.WholeRowWidget.ToSharedRef());
        if(Widgets.ValueWidget)VisitWidget(Widgets.ValueWidget.ToSharedRef());
        TArray<TSharedRef<IDetailTreeNode>> Children;Tree->GetChildren(Children);for(auto Child:Children)VisitNode(Child);
    };
    for(auto Root:Rows->GetRootTreeNodes())VisitNode(Root);
    TestEqual(TEXT("Dialogue details shows inline editors for main line, alternative line and hint only"),InlineMenus,3);
    InlineMenus=0;for(auto Root:SpeakerRows->GetRootTreeNodes())VisitNode(Root);
    TestTrue(TEXT("NPC and player display names have collapsible language editors"),InlineMenus>=2);
    FString Origin,Draft,Error,Decoded;
    for(const FString Culture:{TEXT("en-US"),TEXT("zh-Hans")})
    {
        TestTrue(TEXT("Default language draft is readable"),VTGManualLocalization::GetDialogueDraft(Text,Culture,Origin,Draft,Error));
        TestTrue(TEXT("Default draft preserves protected structure"),VTGManualLocalization::DecodeDraft(Origin,Draft,Decoded,Error));
        TestEqual(TEXT("Unedited draft copies Origin"),Decoded,Source);
    }
    TestFalse(TEXT("Opening inline editor writes no catalog"),FPaths::FileExists(Directory/TEXT("ManualTranslations.json")));
    TestFalse(TEXT("Empty optional hint cannot be given text by a translation"),VTGManualLocalization::GetDialogueDraft(Hint,TEXT("en-US"),Origin,Draft,Error));
    const FName ID=Node->GetID();
    const FString English=TEXT("QA <em>{Name}</>");
    TestTrue(TEXT("Manual English saves"),VTGManualLocalization::SaveDialogueDraft(Text,TEXT("en-US"),Source,VTGManualLocalization::EncodeDraft(Source,English),Error));
    TestEqual(TEXT("Translation does not change node ID"),Node->GetID(),ID);
    TestEqual(TEXT("Translation does not change Origin"),*FTextInspector::GetSourceString(Node->Line.Text),Source);
    TestFalse(TEXT("Broken protected references cannot save"),VTGManualLocalization::SaveDialogueDraft(Text,TEXT("en-US"),Source,TEXT("QA ⟦99⟧"),Error));
    VTGManualLocalization::SetTestStorageDirectory(Directory);
    VTGManualLocalization::GetDialogueDraft(Text,TEXT("en-US"),Origin,Draft,Error);
    VTGManualLocalization::DecodeDraft(Origin,Draft,Decoded,Error);
    TestEqual(TEXT("Saved English survives reopening and invalid edits"),Decoded,English);
    const FString Revised=TEXT("Revised Origin <em>{Name}</>");
    Node->Line.Text=FText::ChangeKey(Namespace,TEXT("Main"),FText::FromString(Revised));
    TestFalse(TEXT("Old source snapshot cannot be saved"),VTGManualLocalization::SaveDialogueDraft(Text,TEXT("en-US"),Source,VTGManualLocalization::EncodeDraft(Source,English),Error));
    VTGManualLocalization::GetDialogueDraft(Text,TEXT("zh-Hans"),Origin,Draft,Error);
    VTGManualLocalization::DecodeDraft(Origin,Draft,Decoded,Error);
    TestEqual(TEXT("Unedited Chinese continues to follow changed Origin"),Decoded,Revised);
    TestTrue(TEXT("Manual character display name can save"),VTGManualLocalization::SaveDialogueDraft(SpeakerName,TEXT("en-US"),TEXT("Origin Character"),TEXT("QA Character"),Error));
    TestEqual(TEXT("Display name translation keeps internal Character ID"),Dialogue->Speakers[0].SpeakerID,FName(TEXT("Character_ID")));
    VTGManualLocalization::GetDialogueDraft(Text,TEXT("en-US"),Origin,Draft,Error);
    TestFalse(TEXT("Manual English is marked for review after Origin changes"),Error.IsEmpty());
    Node->Line.Text=FText::ChangeKey(Namespace,TEXT("Main"),FText::FromString(Source));
    // Transient fixtures have no asset package to refresh during compile; refresh their live field explicitly.
    VTGManualLocalization::GetDialogueDraft(Text,TEXT("en-US"),Origin,Draft,Error);
    TestTrue(TEXT("Manual translation compiles to normal UE resources"),VTGManualLocalization::Compile(Error));
    FTextLocalizationResource Resource;
    TestTrue(TEXT("Compiled English resource exists"),Resource.LoadFromFile(Directory/TEXT("Resources/en-US/Vertigo.locres"),0));
    FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);
    TestEqual(TEXT("Existing Dialogue FText receives manual translation"),Node->Line.Text.ToString(),English);
    TestEqual(TEXT("Compiled translation still preserves Origin"),*FTextInspector::GetSourceString(Node->Line.Text),Source);
    auto& Manager=FTextLocalizationManager::Get();
    const bool PreviouslyPreviewing=Manager.IsGameLocalizationPreviewEnabled();
    const FString PreviousPreviewLanguage=Manager.GetConfiguredGameLocalizationPreviewLanguage();
    ON_SCOPE_EXIT {if(PreviouslyPreviewing)Manager.EnableGameLocalizationPreview(PreviousPreviewLanguage);else Manager.DisableGameLocalizationPreview();Manager.RefreshResources();};
    Manager.EnableGameLocalizationPreview(TEXT("en-US"));
    Manager.UpdateFromLocalizationResource(Resource);
    TestTrue(TEXT("Language preview locks the native Origin editor"),Manager.IsLocalizationLocked());
    bool VisibleOrigin=false;
    TFunction<void(TSharedRef<SWidget>)> FindOrigin=[&](TSharedRef<SWidget> Widget){
        if(Widget->GetType()==TEXT("SMultiLineEditableTextBox") && Widget->GetVisibility()==EVisibility::Visible)
            if(StaticCastSharedRef<SMultiLineEditableTextBox>(Widget)->GetText().ToString()==Source)VisibleOrigin=true;
        auto* Children=Widget->GetChildren();for(int32 Index=0;Index<Children->Num();++Index)FindOrigin(Children->GetChildAt(Index));
    };
    TFunction<void(TSharedRef<IDetailTreeNode>)> FindOriginNode=[&](TSharedRef<IDetailTreeNode> Tree){
        const auto Widgets=Tree->CreateNodeWidgets();if(Widgets.WholeRowWidget)FindOrigin(Widgets.WholeRowWidget.ToSharedRef());if(Widgets.ValueWidget)FindOrigin(Widgets.ValueWidget.ToSharedRef());
        TArray<TSharedRef<IDetailTreeNode>> Children;Tree->GetChildren(Children);for(auto Child:Children)FindOriginNode(Child);
    };
    for(auto Root:Rows->GetRootTreeNodes())FindOriginNode(Root);
    TestTrue(TEXT("Origin row displays the real source even while English is previewed"),VisibleOrigin);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGLanguageConsoleTest,"Vertigo.Localization.ConsoleLanguage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGLanguageConsoleTest::RunTest(const FString&)
{
    TestEqual(TEXT("en selects formal English"),UVTGLocalizationSubsystem::ResolveConsoleLanguage(TEXT("en")),FString(TEXT("en-US")));
    TestEqual(TEXT("zh selects Chinese"),UVTGLocalizationSubsystem::ResolveConsoleLanguage(TEXT("zh")),FString(TEXT("zh-Hans")));
    TestEqual(TEXT("Origin alias is case insensitive in editor"),UVTGLocalizationSubsystem::ResolveConsoleLanguage(TEXT("Origin")),FString(TEXT("origin")));
    TestTrue(TEXT("Invalid languages rejected"),UVTGLocalizationSubsystem::ResolveConsoleLanguage(TEXT("invalid" )).IsEmpty());
    auto* Command=IConsoleManager::Get().FindConsoleObject(TEXT("vtg.Language"));
    if(!TestTrue(TEXT("Language command is registered"),Command && Command->AsCommand()))return false;
    FString Saved;const bool HadSaved=GConfig->GetString(TEXT("Vertigo.Localization"),TEXT("TextLanguage"),Saved,GGameUserSettingsIni);
    auto* Instance=NewObject<UGameInstance>();Instance->Init();
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);World->SetGameInstance(Instance);
    ON_SCOPE_EXIT {
        Instance->Shutdown();World->SetGameInstance(nullptr);World->DestroyWorld(false);
        if(HadSaved)GConfig->SetString(TEXT("Vertigo.Localization"),TEXT("TextLanguage"),*Saved,GGameUserSettingsIni);
        else GConfig->RemoveKey(TEXT("Vertigo.Localization"),TEXT("TextLanguage"),GGameUserSettingsIni);
        GConfig->Flush(false,GGameUserSettingsIni);
    };
    auto* Settings=Instance->GetSubsystem<UVTGLocalizationSubsystem>();
    if(!TestNotNull(TEXT("Game has localization service"),Settings))return false;
    Command->AsCommand()->Execute({TEXT("en")},World,*GLog);
    TestEqual(TEXT("Actual console command switches game to English"),Settings->GetLanguage(),FString(TEXT("en-US")));
    TestTrue(TEXT("Game localization path is configured in the correct config"),!FTextLocalizationManager::Get().GetNativeCultureName(ELocalizedTextSourceCategory::Game).IsEmpty());
    TestTrue(TEXT("Command actually activates translated game resources"),FTextLocalizationManager::Get().IsGameLocalizationPreviewEnabled());
    Command->AsCommand()->Execute({TEXT("zh")},World,*GLog);
    TestEqual(TEXT("Actual console command switches game to Chinese"),Settings->GetLanguage(),FString(TEXT("zh-Hans")));
    Command->AsCommand()->Execute({TEXT("origin")},World,*GLog);
    TestEqual(TEXT("Actual console command restores Origin"),Settings->GetLanguage(),FString(TEXT("origin")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGDialogueSpeakerFieldsTest,"Vertigo.Editor.DialogueSpeakerFields",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGDialogueSpeakerFieldsTest::RunTest(const FString&)
{
    auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    for(bool Player:{true,false})
    {
        UDialogueNode* Node=Player ? static_cast<UDialogueNode*>(NewObject<UDialogueNode_Player>()) : NewObject<UDialogueNode_NPC>();
        Node->AlternativeLines.AddDefaulted();
        auto Rows=Module.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());Rows->SetObjects({Node});
        TSet<FName> Visible;
        TFunction<void(TSharedRef<IDetailTreeNode>)> Visit=[&](TSharedRef<IDetailTreeNode> Tree)
        {
            auto P=Tree->CreatePropertyHandle();
            if(P && P->IsValidHandle() && P->GetProperty() && P->GetProperty()->GetOwnerStruct()==FDialogueLine::StaticStruct())Visible.Add(P->GetProperty()->GetFName());
            TArray<TSharedRef<IDetailTreeNode>> Children;Tree->GetChildren(Children);for(auto Child:Children)Visit(Child);
        };
        for(auto Root:Rows->GetRootTreeNodes())Visit(Root);
        TestEqual(Player?TEXT("Player response image remains visible"):TEXT("NPC response image hidden"),Visible.Contains(TEXT("ResponsePicture")),Player);
        for(FName Field:{FName(TEXT("ProfilePictureExpression")),FName(TEXT("Icon")),FName(TEXT("CharacterReactionvfx")),FName(TEXT("vfx"))})
            TestEqual(*FString::Printf(TEXT("%s field %s"),Player?TEXT("Player"):TEXT("NPC"),*Field.ToString()),Visible.Contains(Field),!Player);
    }
    return true;
}
#endif
