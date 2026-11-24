#include "VTGAssetDisplayLocalizationDetails.h"
#include "VTGManualLocalization.h"
#include "UI/VTGHintLayer.h"
#include "Loading/VTGLevelLoadingInfo.h"
#include "Save/VTGSaveDisplayNames.h"
#include "HAL/IConsoleManager.h"
#include "UI/VTGDisplayLocalizationLibrary.h"
#include "Engine/DataTable.h"
#include "PropertyEditorModule.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/StructOnScope.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Internationalization/TextLocalizationManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGAssetDisplayTest,"Vertigo.Localization.AssetDisplayInline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGAssetDisplayTest::RunTest(const FString&)
{
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("Automation/AssetDisplayLocalization")/FGuid::NewGuid().ToString();
    VTGManualLocalization::SetTestStorageDirectory(Directory);
    ON_SCOPE_EXIT {VTGManualLocalization::SetTestStorageDirectory(FString());FTextLocalizationManager::Get().RefreshResources();};
    auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    TArray<TSharedRef<IPropertyRowGenerator>> KeepRows;
    auto GetFields=[&](UObject* Object,int32& Menus){
        auto Rows=Module.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());Rows->SetObjects({Object});KeepRows.Add(Rows);
        TArray<TSharedPtr<IPropertyHandle>> Fields;Menus=0;
        TFunction<void(TSharedRef<SWidget>)> Widget=[&](TSharedRef<SWidget> W){
            if(W->GetType()==TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(W)->GetText().ToString().Contains(TEXT("多语言译文（展开编辑")))++Menus;
            auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)Widget(C->GetChildAt(I));
        };
        TFunction<void(TSharedRef<IDetailTreeNode>)> Node=[&](TSharedRef<IDetailTreeNode> N){
            auto P=N->CreatePropertyHandle();if(VTGAssetDisplayLocalizationDetails::IsSafeText(P))Fields.Add(P);
            auto W=N->CreateNodeWidgets();if(W.WholeRowWidget)Widget(W.WholeRowWidget.ToSharedRef());if(W.ValueWidget)Widget(W.ValueWidget.ToSharedRef());
            TArray<TSharedRef<IDetailTreeNode>> Children;N->GetChildren(Children);for(auto Child:Children)Node(Child);
        };
        for(auto Root:Rows->GetRootTreeNodes())Node(Root);return Fields;
    };
    auto* NoteAsset=LoadObject<UObject>(nullptr,TEXT("/Game/Inventory/InventoryXBased/DefaultNote.DefaultNote"));
    auto* InteractionClass=LoadClass<UObject>(nullptr,TEXT("/InventorySystemX/InventorySystemX/Components/AC_ActorInteractionComponent.AC_ActorInteractionComponent_C"));
    if(!TestTrue(TEXT("Requested Note and Interaction assets load"),NoteAsset && InteractionClass))return false;
    auto* Note=DuplicateObject(NoteAsset,GetTransientPackage());
    auto* Interaction=NewObject<UObject>(GetTransientPackage(),InteractionClass);
    auto* Hint=NewObject<UVTGHintLayer>();
    auto& Entry=Hint->HintDescriptions.AddDefaulted_GetRef();Entry.ID=TEXT("QA_Stable_Hint_ID");
    const FString Namespace=TEXT("VTG.AssetDisplay.Test.")+FGuid::NewGuid().ToString();
    Entry.Text=FText::ChangeKey(Namespace,TEXT("Hint"),FText::FromString(TEXT("Origin {Name}")));
    int32 Menus=0;auto NoteFields=GetFields(Note,Menus);
    TestTrue(TEXT("PDA title and page text have folded language editors"),Menus>=3 && NoteFields.Num()>=3);
    auto InteractionFields=GetFields(Interaction,Menus);
    TestTrue(TEXT("Interaction Text has a folded language editor"),Menus>=1 && InteractionFields.Num()==1);
    auto HintFields=GetFields(Hint,Menus);
    TestTrue(TEXT("Hint entry and new mail message have folded editors"),Menus>=2 && HintFields.Num()==2);
    TSharedPtr<IPropertyHandle> HintText;
    for(auto P:HintFields)if(P->GetProperty()->GetOwnerStruct()==FVTGHintDisplayText::StaticStruct())HintText=P;
    if(!TestTrue(TEXT("Hint text handle"),HintText.IsValid()))return false;
    FString Origin,Draft,Decoded,Error;
    for(auto P:NoteFields)for(const FString Language:{TEXT("en-US"),TEXT("zh-Hans")})
    {
        TestTrue(TEXT("PDA draft reads"),VTGManualLocalization::GetDialogueDraft(P,Language,Origin,Draft,Error));
        TestTrue(TEXT("PDA protected markers decode"),VTGManualLocalization::DecodeDraft(Origin,Draft,Decoded,Error));
        TestEqual(TEXT("Unedited language follows Origin"),Decoded,Origin);
    }
    TestFalse(TEXT("Opening display fields writes no translations"),FPaths::FileExists(Directory/TEXT("ManualTranslations.json")));
    TestTrue(TEXT("Hint translation saves"),VTGManualLocalization::SaveDialogueDraft(HintText,TEXT("en-US"),TEXT("Origin {Name}"),VTGManualLocalization::EncodeDraft(TEXT("Origin {Name}"),TEXT("QA {Name}")),Error));
    TestFalse(TEXT("Missing protected marker rejected"),VTGManualLocalization::SaveDialogueDraft(HintText,TEXT("en-US"),TEXT("Origin {Name}"),TEXT("QA"),Error));
    TestTrue(TEXT("Display resources compile"),VTGManualLocalization::Compile(Error));
    FTextLocalizationResource Resource;TestTrue(TEXT("Display resource loads"),Resource.LoadFromFile(Directory/TEXT("Resources/en-US/Vertigo.locres"),0));
    FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);
    TestEqual(TEXT("Hint resolver returns translated FText"),Hint->ResolveHintDescription(Entry.ID).ToString(),FString(TEXT("QA {Name}")));
    TestEqual(TEXT("Hint ID is unchanged"),Entry.ID,FName(TEXT("QA_Stable_Hint_ID")));
    TestEqual(TEXT("Unknown hint retains legacy fallback"),Hint->ResolveHintDescription(TEXT("Unknown_ID")).ToString(),FString(TEXT("Unknown_ID")));
    auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Narrative/Mail/DT_Mail_ID_FillHere.DT_Mail_ID_FillHere"));
    if(!TestNotNull(TEXT("Mail DataTable"),Table))return false;
    FNameProperty* Sender=nullptr;FTextProperty* Display=nullptr;
    for(TFieldIterator<FProperty> It(Table->GetRowStruct());It;++It)
    {
        if(It->GetName().StartsWith(TEXT("SenderName_")))Sender=CastField<FNameProperty>(*It);
        if(It->GetName().StartsWith(TEXT("SenderDisplayName_")))Display=CastField<FTextProperty>(*It);
    }
    if(!TestTrue(TEXT("Mail retains Name ID and adds FText display"),Sender && Display))return false;
    TestTrue(TEXT("Mail row texts are collected"),VTGManualLocalization::GatherAsset(Table,Error));
    for(const auto& Row:Table->GetRowMap())
    {
        const FText Text=Display->GetPropertyValue_InContainer(Row.Value);const auto Key=FTextInspector::GetKey(Text),NS=FTextInspector::GetNamespace(Text);
        TestTrue(TEXT("Mail display has stable text identity"),Key.IsSet() && NS.IsSet());
        TestEqual(TEXT("Runtime Mail lookup reads display text"),UVTGDisplayLocalizationLibrary::GetMailSenderText(Table,Sender->GetPropertyValue_InContainer(Row.Value)).ToString(),Text.ToString());
    }
    const bool WasDirty=Table->GetOutermost()->IsDirty();
    const auto& FirstRow=*Table->GetRowMap().CreateConstIterator();
    auto Scope=MakeShared<FStructOnScope>(Table->GetRowStruct(),FirstRow.Value);Scope->SetPackage(Table->GetOutermost());
    auto MailRows=Module.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());MailRows->SetStructure(Scope);
    TSharedPtr<IPropertyHandle> MailText;
    int32 MailMenus=0;
    TFunction<void(TSharedRef<SWidget>)> MailWidget=[&](TSharedRef<SWidget> W){
        if(W->GetType()==TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(W)->GetText().ToString().Contains(TEXT("多语言译文（展开编辑")))++MailMenus;
        auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)MailWidget(C->GetChildAt(I));
    };
    TFunction<void(TSharedRef<IDetailTreeNode>)> MailNode=[&](TSharedRef<IDetailTreeNode> N){
        auto P=N->CreatePropertyHandle();if(P && P->GetProperty()==Display)MailText=P;
        auto W=N->CreateNodeWidgets();if(W.WholeRowWidget)MailWidget(W.WholeRowWidget.ToSharedRef());if(W.ValueWidget)MailWidget(W.ValueWidget.ToSharedRef());
        TArray<TSharedRef<IDetailTreeNode>> Children;N->GetChildren(Children);for(auto Child:Children)MailNode(Child);
    };
    for(auto Root:MailRows->GetRootTreeNodes())MailNode(Root);
    TestEqual(TEXT("DataTable Row Editor has one folded display editor"),MailMenus,1);
    if(!TestTrue(TEXT("Mail row handle is safe without UObject owner"),VTGAssetDisplayLocalizationDetails::IsSafeText(MailText)))return false;
    TestTrue(TEXT("Mail row draft reads"),VTGManualLocalization::GetDialogueDraft(MailText,TEXT("en-US"),Origin,Draft,Error));
    TestTrue(TEXT("Mail row translation saves to isolated catalog"),VTGManualLocalization::SaveDialogueDraft(MailText,TEXT("en-US"),Origin,TEXT("QA Sender"),Error));
    TestTrue(TEXT("Mail translation compiles with source revalidation"),VTGManualLocalization::Compile(Error));
    FTextLocalizationResource MailResource;MailResource.LoadFromFile(Directory/TEXT("Resources/en-US/Vertigo.locres"),0);
    FTextLocalizationManager::Get().UpdateFromLocalizationResource(MailResource);
    TestEqual(TEXT("Live Mail sender uses manual translation"),UVTGDisplayLocalizationLibrary::GetMailSenderText(Table,Sender->GetPropertyValue_InContainer(FirstRow.Value)).ToString(),FString(TEXT("QA Sender")));
    TestEqual(TEXT("Translation never dirties the Mail DataTable"),Table->GetOutermost()->IsDirty(),WasDirty);

    // The save menu's place and phase wording lives in two more DataTables and has to get the same
    // folded editor. Authored text with no editor is text that can never be translated, which is the
    // failure these tables were introduced to avoid in the first place.
    auto* DisplayField=CastField<FTextProperty>(FVTGSaveDisplayRow::StaticStruct()->FindPropertyByName(
        GET_MEMBER_NAME_CHECKED(FVTGSaveDisplayRow,Display)));
    if(!TestNotNull(TEXT("Save display field"),DisplayField))return false;
    for(const TCHAR* Path:{TEXT("/Game/Widget/SaveLoad/DT_SaveLocationNames"),TEXT("/Game/Widget/SaveLoad/DT_SaveStageNames")})
    {
        auto* SaveTable=LoadObject<UDataTable>(nullptr,Path);
        if(!TestNotNull(TEXT("Save display DataTable"),SaveTable))continue;
        if(!TestTrue(TEXT("Save display DataTable has rows"),SaveTable->GetRowMap().Num()>0))continue;
        for(const auto& Row:SaveTable->GetRowMap())
        {
            const FText Text=DisplayField->GetPropertyValue_InContainer(Row.Value);
            // AsCultureInvariant is skipped by the gather, so it would silently never be translatable.
            TestFalse(TEXT("Save display text is not culture-invariant"),Text.IsCultureInvariant());
            TestTrue(TEXT("Save display text has a stable identity"),FTextInspector::GetKey(Text).IsSet());
        }
        const auto& SaveFirst=*SaveTable->GetRowMap().CreateConstIterator();
        auto SaveScope=MakeShared<FStructOnScope>(SaveTable->GetRowStruct(),SaveFirst.Value);SaveScope->SetPackage(SaveTable->GetOutermost());
        auto SaveRows=Module.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());SaveRows->SetStructure(SaveScope);
        TSharedPtr<IPropertyHandle> SaveText;int32 SaveMenus=0;
        TFunction<void(TSharedRef<SWidget>)> SaveWidget=[&](TSharedRef<SWidget> W){
            if(W->GetType()==TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(W)->GetText().ToString().Contains(TEXT("多语言译文（展开编辑")))++SaveMenus;
            auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)SaveWidget(C->GetChildAt(I));
        };
        TFunction<void(TSharedRef<IDetailTreeNode>)> SaveNode=[&](TSharedRef<IDetailTreeNode> N){
            auto P=N->CreatePropertyHandle();if(P && P->GetProperty()==DisplayField)SaveText=P;
            auto W=N->CreateNodeWidgets();if(W.WholeRowWidget)SaveWidget(W.WholeRowWidget.ToSharedRef());if(W.ValueWidget)SaveWidget(W.ValueWidget.ToSharedRef());
            TArray<TSharedRef<IDetailTreeNode>> Children;N->GetChildren(Children);for(auto Child:Children)SaveNode(Child);
        };
        for(auto Root:SaveRows->GetRootTreeNodes())SaveNode(Root);
        TestEqual(TEXT("Save display Row Editor has one folded translation editor"),SaveMenus,1);
        TestTrue(TEXT("Save display row handle is safe"),VTGAssetDisplayLocalizationDetails::IsSafeText(SaveText));
        FString SaveContext;
        TestTrue(TEXT("Save display row resolves to a table context"),
            VTGAssetDisplayLocalizationDetails::GetDataTableRowContext(SaveText,SaveContext));
        TestTrue(TEXT("Save display context names its own table"),SaveContext.Contains(SaveTable->GetName()));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGLoadingNewsInlineTest,"Vertigo.Localization.LoadingNewsInline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGLoadingNewsInlineTest::RunTest(const FString&)
{
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("Automation/LoadingNewsLocalization")/FGuid::NewGuid().ToString();
    VTGManualLocalization::SetTestStorageDirectory(Directory);
    ON_SCOPE_EXIT {VTGManualLocalization::SetTestStorageDirectory(FString());FTextLocalizationManager::Get().RefreshResources();};
    const FString Namespace=TEXT("VTG.LoadingNews.Test.")+FGuid::NewGuid().ToString();
    auto* Info=NewObject<UVTGLevelLoadingInfo>();
    Info->NewsBodies={FText::ChangeKey(Namespace,TEXT("One"),FText::FromString(TEXT("News one {Name}"))),
                      FText::ChangeKey(Namespace,TEXT("Two"),FText::FromString(TEXT("News two")))};
    const bool WasDirty=Info->GetOutermost()->IsDirty();
    auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    auto Rows=Module.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());Rows->SetObjects({Info});
    TArray<TSharedPtr<IPropertyHandle>> Fields;
    int32 Menus=0;
    TFunction<void(TSharedRef<SWidget>)> Widget=[&](TSharedRef<SWidget> W){
        if(W->GetType()==TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(W)->GetText().ToString().Contains(TEXT("多语言译文（展开编辑")))++Menus;
        auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)Widget(C->GetChildAt(I));
    };
    TFunction<void(TSharedRef<IDetailTreeNode>)> Node=[&](TSharedRef<IDetailTreeNode> N){
        auto P=N->CreatePropertyHandle();if(VTGAssetDisplayLocalizationDetails::IsSafeText(P))Fields.Add(P);
        auto W=N->CreateNodeWidgets();if(W.WholeRowWidget)Widget(W.WholeRowWidget.ToSharedRef());if(W.ValueWidget)Widget(W.ValueWidget.ToSharedRef());
        TArray<TSharedRef<IDetailTreeNode>> Children;N->GetChildren(Children);for(auto Child:Children)Node(Child);
    };
    for(auto Root:Rows->GetRootTreeNodes())Node(Root);
    TestEqual(TEXT("Every news array element has a folded language editor"),Menus,2);
    if(!TestEqual(TEXT("Only news text elements are writable translation fields"),Fields.Num(),2))return false;
    FString Error,Origin,Draft,Decoded;
    for(auto P:Fields)
    {
        const int32 Index=P->GetIndexInArray();
        if(!TestTrue(TEXT("News handle keeps its array index"),Index>=0 && Index<2))return false;
        const FString Source=Index==0?TEXT("News one {Name}"):TEXT("News two");
        TestTrue(TEXT("News draft can be opened"),VTGManualLocalization::GetDialogueDraft(P,TEXT("en-US"),Origin,Draft,Error));
        TestEqual(TEXT("News draft Origin matches its own item"),Origin,Source);
        const FString English=Index==0?TEXT("English one {Name}"):TEXT("English two");
        TestTrue(TEXT("Each news translation saves independently"),VTGManualLocalization::SaveDialogueDraft(P,TEXT("en-US"),Source,VTGManualLocalization::EncodeDraft(Source,English),Error));
        TestTrue(TEXT("Saved news translation can be reopened"),VTGManualLocalization::GetDialogueDraft(P,TEXT("en-US"),Origin,Draft,Error));
        TestTrue(TEXT("News draft decodes"),VTGManualLocalization::DecodeDraft(Origin,Draft,Decoded,Error));
        TestEqual(TEXT("English remains paired with its own news"),Decoded,English);
    }
    TestTrue(TEXT("News translations compile"),VTGManualLocalization::Compile(Error));
    FTextLocalizationResource Resource;
    TestTrue(TEXT("News English resource loads"),Resource.LoadFromFile(Directory/TEXT("Resources/en-US/Vertigo.locres"),0));
    FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);
    TestEqual(TEXT("First news resolves its own English"),Info->NewsBodies[0].ToString(),FString(TEXT("English one {Name}")));
    TestEqual(TEXT("Second news resolves its own English"),Info->NewsBodies[1].ToString(),FString(TEXT("English two")));
    TestEqual(TEXT("Translating news does not dirty the source asset"),Info->GetOutermost()->IsDirty(),WasDirty);
    TestEqual(TEXT("First news keeps its stable key"),FTextInspector::GetKey(Info->NewsBodies[0]).Get(FString()),FString(TEXT("One")));
    TestEqual(TEXT("Second news keeps its stable key"),FTextInspector::GetKey(Info->NewsBodies[1]).Get(FString()),FString(TEXT("Two")));
    return true;
}

// A direct synchronous entry point also permits checking Details without enumerating unrelated tests.
static FAutoConsoleCommand GVTGCheckLoadingNewsLocalization(
    TEXT("VTG.CheckLoadingNewsLocalization"),TEXT("Check per-item loading news translation editors."),
    FConsoleCommandDelegate::CreateLambda([](){
        class FDirectNewsTest:public FVTGLoadingNewsInlineTest
        {public:FDirectNewsTest():FVTGLoadingNewsInlineTest(TEXT("VTGDirectLoadingNews")){}using FVTGLoadingNewsInlineTest::RunTest;};
        FDirectNewsTest Test;
        const bool Ran=Test.RunTest({});
        FAutomationTestExecutionInfo Info;Test.GetExecutionInfo(Info);
        const bool Passed=Ran && !Test.HasAnyErrors();
        UE_LOG(LogTemp,Display,TEXT("LOADING NEWS INLINE CHECK: %s"),Passed?TEXT("PASS"):TEXT("FAIL"));
        for(const auto& Entry:Info.GetEntries())UE_LOG(LogTemp,Display,TEXT("LOADING NEWS CHECK RESULT: %s"),*Entry.ToString());
    }));
#endif
