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

namespace VTGQuestLocalizationDetails
{
namespace
{
FDelegateHandle Handle;
void AddField(IDetailLayoutBuilder& Layout,TSharedPtr<IPropertyHandle> Property)
{
    if(!IsSafeText(Property))return;
    if(auto* Row=Layout.EditDefaultProperty(Property))
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
    Layout.EditCategory(TEXT("Localization Safety")).AddCustomRow(FText::FromString(TEXT("标识风险"))).WholeRowContent()
        [SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FLinearColor(1,.65f,.2f))
        .Text(FText::FromString(TEXT("⚠ ID、事件名、目标状态、Task 参数及数量影响任务逻辑；不要为了翻译修改它们。只在显示文本的多语言区填写译文。自动生成的描述请通过 Description Override 提供显示文本。")))];
    if(Cast<UQuest>(Objects[0].Get()))
    {
        AddField(Layout,Layout.GetProperty(TEXT("QuestName"),UQuest::StaticClass()));
        AddField(Layout,Layout.GetProperty(TEXT("QuestDescription"),UQuest::StaticClass()));
    }
    if(Cast<UQuestNode>(Objects[0].Get()))AddField(Layout,Layout.GetProperty(TEXT("Description"),UQuestNode::StaticClass()));
    if(Cast<UNarrativeTask>(Objects[0].Get()))AddField(Layout,Layout.GetProperty(TEXT("DescriptionOverride"),UNarrativeTask::StaticClass()));
    if(Cast<UQuestBranch>(Objects[0].Get()))
    {
        auto Tasks=Layout.GetProperty(TEXT("QuestTasks"),UQuestBranch::StaticClass())->AsArray();
        if(Tasks)
        {
            uint32 Count=0;Tasks->GetNumElements(Count);
            for(uint32 Index=0;Index<Count;++Index)
                AddField(Layout,Tasks->GetElement(Index)->GetChildHandle(TEXT("DescriptionOverride")));
        }
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
