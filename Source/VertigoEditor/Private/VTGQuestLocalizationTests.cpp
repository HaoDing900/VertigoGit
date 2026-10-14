#include "VTGQuestLocalizationDetails.h"
#include "VTGManualLocalization.h"
#include "QuestBlueprint.h"
#include "Quest.h"
#include "QuestSM.h"
#include "QuestTask.h"
#include "PropertyEditorModule.h"
#include "ISinglePropertyView.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/TextLocalizationResource.h"
#include "UObject/UObjectHash.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGQuestInlineTest,"Vertigo.Localization.QuestInline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGQuestInlineTest::RunTest(const FString&)
{
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("Automation/QuestLocalization")/FGuid::NewGuid().ToString();
    VTGManualLocalization::SetTestStorageDirectory(Directory);
    ON_SCOPE_EXIT {VTGManualLocalization::SetTestStorageDirectory(FString());FTextLocalizationManager::Get().RefreshResources();};
    auto* Asset=LoadObject<UQuestBlueprint>(nullptr,TEXT("/Game/Narrative/Quest/MasterQuests/MasterQuest_Failure.MasterQuest_Failure"));
    if(!TestNotNull(TEXT("Requested MasterQuest_Failure asset"),Asset) || !TestNotNull(TEXT("Quest template"),Asset->QuestTemplate))return false;
    const bool WasDirty=Asset->GetOutermost()->IsDirty();
    auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    auto CountMenus=[&](UObject* Object){
        auto Rows=Module.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());Rows->SetObjects({Object});
        int32 Count=0;
        TFunction<void(TSharedRef<SWidget>)> Widget=[&](TSharedRef<SWidget> W){
            if(W->GetType()==TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(W)->GetText().ToString().Contains(TEXT("多语言译文（展开编辑")))++Count;
            auto* Children=W->GetChildren();for(int32 I=0;I<Children->Num();++I)Widget(Children->GetChildAt(I));
        };
        TFunction<void(TSharedRef<IDetailTreeNode>)> Node=[&](TSharedRef<IDetailTreeNode> N){
            auto W=N->CreateNodeWidgets();if(W.WholeRowWidget)Widget(W.WholeRowWidget.ToSharedRef());if(W.ValueWidget)Widget(W.ValueWidget.ToSharedRef());
            TArray<TSharedRef<IDetailTreeNode>> Children;N->GetChildren(Children);for(auto Child:Children)Node(Child);
        };
        for(auto Root:Rows->GetRootTreeNodes())Node(Root);return Count;
    };
    TestEqual(TEXT("Real Quest defaults contain title and description editors"),CountMenus(Asset->QuestTemplate),2);
    TArray<UObject*> Objects;GetObjectsWithPackage(Asset->GetOutermost(),Objects,true);
    UQuestBranch* OriginalBranch=nullptr;
    for(auto* Object:Objects)if(auto* Branch=Cast<UQuestBranch>(Object)){OriginalBranch=Branch;break;}
    for(auto* State:Asset->QuestTemplate->GetStates())TestTrue(TEXT("Real quest state contains description editor"),CountMenus(State)>=1);
    if(!OriginalBranch)
    {
        OriginalBranch=NewObject<UQuestBranch>();
        auto* TaskClass=LoadClass<UNarrativeTask>(nullptr,TEXT("/Narrative/DefaultTasks/BPT_CompleteDataTask.BPT_CompleteDataTask_C"));
        if(!TestNotNull(TEXT("Concrete Narrative task class"),TaskClass))return false;
        OriginalBranch->QuestTasks.Add(NewObject<UNarrativeTask>(OriginalBranch,TaskClass));
        OriginalBranch->SetID(TEXT("QA_Internal_ID"));
        OriginalBranch->OnEnteredFuncName=TEXT("QA_Internal_Event");
        OriginalBranch->DestinationState=NewObject<UQuestState>();
    }
    TestTrue(TEXT("Real branch details include description and nested task editors"),CountMenus(OriginalBranch)>=1+OriginalBranch->QuestTasks.Num());
    TestEqual(TEXT("Opening original asset never marks it dirty"),Asset->GetOutermost()->IsDirty(),WasDirty);
    TestFalse(TEXT("Opening Quest details does not write translations"),FPaths::FileExists(Directory/TEXT("ManualTranslations.json")));
    auto* Branch=DuplicateObject<UQuestBranch>(OriginalBranch,GetTransientPackage());
    const FName ID=Branch->GetID(),Callback=Branch->OnEnteredFuncName;
    auto* Destination=Branch->DestinationState;
    const FString Namespace=TEXT("VTG.Quest.Test.")+FGuid::NewGuid().ToString(),Source=TEXT("Origin <b>{Target}</>");
    Branch->Description=FText::ChangeKey(Namespace,TEXT("Description"),FText::FromString(Source));
    auto View=Module.CreateSingleProperty(Branch,TEXT("Description"),FSinglePropertyParams());
    auto IDView=Module.CreateSingleProperty(Branch,TEXT("ID"),FSinglePropertyParams());
    if(!TestTrue(TEXT("Quest property handles"),View && IDView))return false;
    auto Property=View->GetPropertyHandle();
    TestTrue(TEXT("Description whitelisted"),VTGQuestLocalizationDetails::IsSafeText(Property));
    TestFalse(TEXT("ID is excluded"),VTGQuestLocalizationDetails::IsSafeText(IDView->GetPropertyHandle()));
    FString Origin,Draft,Decoded,Error;
    for(const FString Culture:{TEXT("en-US"),TEXT("zh-Hans")})
    {
        TestTrue(TEXT("Quest language draft"),VTGManualLocalization::GetDialogueDraft(Property,Culture,Origin,Draft,Error));
        TestTrue(TEXT("Protected structure decode"),VTGManualLocalization::DecodeDraft(Origin,Draft,Decoded,Error));
        TestEqual(TEXT("Default translation inherits Origin"),Decoded,Source);
    }
    TestTrue(TEXT("Manual Quest translation saves"),VTGManualLocalization::SaveDialogueDraft(Property,TEXT("en-US"),Source,VTGManualLocalization::EncodeDraft(Source,TEXT("QA <b>{Target}</>")),Error));
    TestFalse(TEXT("Broken placeholder rejected"),VTGManualLocalization::SaveDialogueDraft(Property,TEXT("en-US"),Source,TEXT("QA"),Error));
    TestTrue(TEXT("Quest resources compile"),VTGManualLocalization::Compile(Error));
    FTextLocalizationResource Resource;TestTrue(TEXT("Quest resource loads"),Resource.LoadFromFile(Directory/TEXT("Resources/en-US/Vertigo.locres"),0));
    FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);
    TestEqual(TEXT("Live branch display receives translation"),Branch->Description.ToString(),FString(TEXT("QA <b>{Target}</>")));
    TestEqual(TEXT("Origin unchanged"),*FTextInspector::GetSourceString(Branch->Description),Source);
    TestEqual(TEXT("Branch ID unchanged"),Branch->GetID(),ID);TestEqual(TEXT("Callback unchanged"),Branch->OnEnteredFuncName,Callback);
    TestTrue(TEXT("Destination unchanged"),Branch->DestinationState==Destination);
    for(int32 I=0;I<Branch->QuestTasks.Num();++I)
    {
        auto* Task=Branch->QuestTasks[I];auto TaskView=Module.CreateSingleProperty(Task,TEXT("DescriptionOverride"),FSinglePropertyParams());
        TestTrue(TEXT("Task override is safe"),TaskView && VTGQuestLocalizationDetails::IsSafeText(TaskView->GetPropertyHandle()));
        TestEqual(TEXT("Required quantity unchanged"),Task->RequiredQuantity,OriginalBranch->QuestTasks[I]->RequiredQuantity);
    }
    return true;
}
#endif
