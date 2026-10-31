#include "VTGQuestLocalizationDetails.h"
#include "VTGDialogueLocalizationDetails.h"
#include "NarrativeQuestDetailsExtension.h"
#include "Quest.h"
#include "QuestSM.h"
#include "QuestTask.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "IDetailCustomization.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "HAL/PlatformApplicationMisc.h"

namespace VTGQuestLocalizationDetails
{
namespace
{
FDelegateHandle Handle;
void AddStateIDHelp(IDetailLayoutBuilder& Layout, UQuestState* State, UQuestBranch* Branch = nullptr)
{
    const bool bFromBranch = Branch != nullptr;
    const TWeakObjectPtr<UQuestState> WeakState(State);
    const TWeakObjectPtr<UQuestBranch> WeakBranch(Branch);
    const auto TargetState = [WeakState, WeakBranch, bFromBranch]() -> UQuestState* {
        return bFromBranch ? GetQuestBranchGraphDestination(WeakBranch.Get()) : WeakState.Get();
    };
    auto& Category = Layout.EditCategory(TEXT("Dialogue Quest Progress"), FText::FromString(TEXT("对话推进任务：目标 State ID")), ECategoryPriority::Important);
    Category.AddCustomRow(FText::FromString(TEXT("State ID 目标状态 复制")))
        .NameContent()[SNew(STextBlock).Text(FText::FromString(bFromBranch ? TEXT("Destination State ID") : TEXT("State ID（复制此值）")))]
        .ValueContent().MinDesiredWidth(320)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
            [SNew(STextBlock).Text_Lambda([TargetState]() { auto* Target = TargetState(); return Target ? FText::FromName(Target->GetID()) : FText::FromString(TEXT("未连接目标 State")); })]
            + SHorizontalBox::Slot().AutoWidth().Padding(8, 0)
            [SNew(SButton).Text(FText::FromString(TEXT("复制 State ID")))
                .IsEnabled_Lambda([TargetState]() { auto* Target = TargetState(); return Target && !Target->GetID().IsNone(); })
                .OnClicked_Lambda([TargetState]() {
                    if (auto* Target = TargetState()) FPlatformApplicationMisc::ClipboardCopy(*Target->GetID().ToString());
                    return FReply::Handled();
                })]];
    if (bFromBranch)
        Category.AddCustomRow(FText::FromString(TEXT("连线一致性"))).WholeRowContent()
            [SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FLinearColor(1,.65f,.2f))
                .Text_Lambda([WeakBranch, TargetState]() {
                    return WeakBranch.IsValid() && WeakBranch->DestinationState != TargetState()
                        ? FText::FromString(TEXT("注意：图上连线与内部 DestinationState 引用不一致。上方复制的是图上目标 ID；请断开并重新连接该 Branch 右侧连线，再编译保存，以同步运行时目标。")) : FText::GetEmpty();
                })];
    Category.AddCustomRow(FText::FromString(TEXT("推进任务说明"))).WholeRowContent()
        [SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(bFromBranch
            ? TEXT("当前选中的是 Branch（任务条件分支），它只有 Branch ID。上方是其右侧连线目标 State 的 ID：进入该 State 会越过当前 Branch 的任务。若要开始当前 Branch 的任务，应选择左侧来源 State 并复制其 ID。不要填写 QuestBranch_*。")
            : TEXT("在 Quest 图中选中想要进入的 State，复制上方 ID，粘贴到 Dialogue 节点 Events → VTG Ensure Quest State → State ID；Quest 选择本任务。填写实际 ID，不是节点标题、描述或 QuestBranch_*。Begin Quest 只用于开始任务，不用于推进已有任务。")))];
}
void AddField(IDetailLayoutBuilder& Layout,TSharedPtr<IPropertyHandle> Property,bool bDisplayCategory=false)
{
    if(!IsSafeText(Property))return;
    auto* Row=Layout.EditDefaultProperty(Property);
    if(Row && !bDisplayCategory)
        Row->CustomWidget(false).WholeRowContent()[VTGDialogueLocalizationDetails::MakeTextRow(Property)];
    else
    {
        Layout.HideProperty(Property);
        Layout.EditCategory(TEXT("Quest Display Text")).AddCustomRow(Property->GetPropertyDisplayName())
            .WholeRowContent()[VTGDialogueLocalizationDetails::MakeTextRow(Property)];
    }
}
void Customize(IDetailLayoutBuilder& Layout)
{
    TArray<TWeakObjectPtr<UObject>> Objects;Layout.GetObjectsBeingCustomized(Objects);
    if(Objects.Num()!=1)return;
    if(auto* State=Cast<UQuestState>(Objects[0].Get()))AddStateIDHelp(Layout,State);
    else if(auto* Branch=Cast<UQuestBranch>(Objects[0].Get()))AddStateIDHelp(Layout,nullptr,Branch);
    Layout.EditCategory(TEXT("Localization Safety")).AddCustomRow(FText::FromString(TEXT("标识风险"))).WholeRowContent()
        [SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FLinearColor(1,.65f,.2f))
        .Text(FText::FromString(TEXT("⚠ ID、事件名、目标状态、Task 参数及数量影响任务逻辑；不要为了翻译修改它们。只在显示文本的多语言区填写译文。自动生成的描述请通过 Description Override 提供显示文本。")))];
    if(Cast<UQuest>(Objects[0].Get()))
    {
        AddField(Layout,Layout.GetProperty(TEXT("QuestName"),UQuest::StaticClass()));
        AddField(Layout,Layout.GetProperty(TEXT("QuestDescription"),UQuest::StaticClass()));
    }
    if(Cast<UQuestNode>(Objects[0].Get()))Layout.HideProperty(Layout.GetProperty(TEXT("Description"),UQuestNode::StaticClass()));
    if(Cast<UNarrativeTask>(Objects[0].Get()))AddField(Layout,Layout.GetProperty(TEXT("DescriptionOverride"),UNarrativeTask::StaticClass()));
    if(Cast<UQuestBranch>(Objects[0].Get()))
    {
        auto Tasks=Layout.GetProperty(TEXT("QuestTasks"),UQuestBranch::StaticClass())->AsArray();
        if(Tasks)
        {
            uint32 Count=0;Tasks->GetNumElements(Count);
            for(uint32 Index=0;Index<Count;++Index)
                AddField(Layout,Tasks->GetElement(Index)->GetChildHandle(TEXT("DescriptionOverride")),true);
        }
        Layout.SortCategories([](const TMap<FName,IDetailCategoryBuilder*>& Categories) {
            TArray<IDetailCategoryBuilder*> Ordered;
            IDetailCategoryBuilder* Display=nullptr;
            IDetailCategoryBuilder* Details=nullptr;
            for(const auto& Pair:Categories)
            {
                if(Pair.Key==TEXT("Quest Display Text"))Display=Pair.Value;
                else {Ordered.Add(Pair.Value);if(Pair.Key==TEXT("Details"))Details=Pair.Value;}
            }
            Ordered.StableSort([](const IDetailCategoryBuilder& A,const IDetailCategoryBuilder& B){return A.GetSortOrder()<B.GetSortOrder();});
            if(Display)Ordered.Insert(Display,Details?Ordered.IndexOfByKey(Details)+1:0);
            for(int32 Index=0;Index<Ordered.Num();++Index)Ordered[Index]->SetSortOrder(Index);
        });
    }
}
class FQuestDetails : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> Make(){return MakeShared<FQuestDetails>();}
    virtual void CustomizeDetails(IDetailLayoutBuilder& Layout)override{Customize(Layout);}
};
}
bool IsSafeText(TSharedPtr<IPropertyHandle> Property)
{
    if(!Property || !Property->IsValidHandle())return false;
    auto* Field=Property->GetProperty();
    if(!CastField<FTextProperty>(Field) || Field->HasMetaData(TEXT("VTGLocalizationLocked")))return false;
    TArray<UObject*> Owners;Property->GetOuterObjects(Owners);if(Owners.Num()!=1)return false;
    const FName Name=Field->GetFName();
    if(Field->GetOwnerStruct()==UQuest::StaticClass())
        return Cast<UQuest>(Owners[0]) && (Name==TEXT("QuestName") || Name==TEXT("QuestDescription"));
    if(Field->GetOwnerStruct()==UQuestNode::StaticClass())return Cast<UQuestNode>(Owners[0]) && Name==TEXT("Description");
    if(Field->GetOwnerStruct()==UNarrativeTask::StaticClass())
        return (Cast<UNarrativeTask>(Owners[0]) || Cast<UQuestBranch>(Owners[0])) && Name==TEXT("DescriptionOverride");
    return false;
}
void Startup()
{
    Handle=GetNarrativeQuestDetailsExtension().AddStatic(&Customize);
    auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    for(const FName Name:{FName(TEXT("Quest")),FName(TEXT("NarrativeTask"))})
        Module.RegisterCustomClassLayout(Name,FOnGetDetailCustomizationInstance::CreateStatic(&FQuestDetails::Make));
}
void Shutdown()
{
    GetNarrativeQuestDetailsExtension().Remove(Handle);
    if(FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
        for(const FName Name:{FName(TEXT("Quest")),FName(TEXT("NarrativeTask"))})
            FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).UnregisterCustomClassLayout(Name);
}
}
