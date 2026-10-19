#include "VTGDialogueLocalizationDetails.h"
#include "VTGManualLocalization.h"
#include "NarrativeDialogueDetailsExtension.h"
#include "DialogueSM.h"
#include "Dialogue.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Internationalization/TextLocalizationManager.h"
#include "IPropertyTypeCustomization.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyEditorModule.h"

namespace VTGDialogueLocalizationDetails
{
namespace
{
FDelegateHandle Handle;
void AddField(IDetailLayoutBuilder& Layout,TSharedPtr<IPropertyHandle> Property)
{
    if(!IsSafeText(Property))return;
    const auto Content=MakeTextRow(Property);
    if(auto* Row=Layout.EditDefaultProperty(Property))Row->CustomWidget(false).WholeRowContent()[Content];
    else
    {
        Layout.HideProperty(Property);
        Layout.EditCategory(FName(*Property->GetProperty()->GetMetaData(TEXT("Category"))))
            .AddCustomRow(Property->GetPropertyDisplayName()).WholeRowContent()[Content];
    }
}
class FSpeakerDetails : public IPropertyTypeCustomization
{
public:
    static TSharedRef<IPropertyTypeCustomization> Make(){return MakeShared<FSpeakerDetails>();}
    virtual void CustomizeHeader(TSharedRef<IPropertyHandle> Property,FDetailWidgetRow& Row,IPropertyTypeCustomizationUtils&) override
    {Row.NameContent()[Property->CreatePropertyNameWidget()];}
    virtual void CustomizeChildren(TSharedRef<IPropertyHandle> Property,IDetailChildrenBuilder& Children,IPropertyTypeCustomizationUtils&) override
    {
        uint32 Count=0;Property->GetNumChildren(Count);
        for(uint32 Index=0;Index<Count;++Index)
        {
            const auto Child=Property->GetChildHandle(Index);if(!Child || !Child->IsValidHandle())continue;
            auto& Row=Children.AddProperty(Child.ToSharedRef());
            if(IsSafeText(Child))Row.CustomWidget(false).WholeRowContent()[MakeTextRow(Child)];
        }
    }
};
void AddLine(IDetailLayoutBuilder& Layout,TSharedPtr<IPropertyHandle> Line)
{
    if(!Line.IsValid() || !Line->IsValidHandle())return;
    AddField(Layout,Line->GetChildHandle(TEXT("Text")));
    TArray<TWeakObjectPtr<UObject>> Objects;Layout.GetObjectsBeingCustomized(Objects);
    const bool AllPlayer=Objects.Num()>0 && Objects.ContainsByPredicate([](const auto& Object){return !Cast<UDialogueNode_Player>(Object.Get());})==false;
    const bool AllNPC=Objects.Num()>0 && Objects.ContainsByPredicate([](const auto& Object){return !Cast<UDialogueNode_NPC>(Object.Get());})==false;
    // Preserve authored values; filter only Details, including Alternative Lines.
    // Mixed selections retain every field so usable data is never hidden.
    const TArray<FName> Hidden=AllPlayer ? TArray<FName>{TEXT("ProfilePictureExpression"),TEXT("Icon"),TEXT("CharacterReactionvfx"),TEXT("vfx")} :
        AllNPC ? TArray<FName>{TEXT("ResponsePicture")} : TArray<FName>{};
    for(const FName& Name:Hidden)
        if(auto Field=Line->GetChildHandle(Name);Field.IsValid() && Field->IsValidHandle())Layout.HideProperty(Field);
}
void Customize(IDetailLayoutBuilder& Layout)
{
    AddLine(Layout,Layout.GetProperty(TEXT("Line"),UDialogueNode::StaticClass()));
    auto Alternatives=Layout.GetProperty(TEXT("AlternativeLines"),UDialogueNode::StaticClass())->AsArray();
    if(Alternatives)
    {
        uint32 Count=0;Alternatives->GetNumElements(Count);
        for(uint32 Index=0;Index<Count;++Index)AddLine(Layout,Alternatives->GetElement(Index));
    }
    TArray<TWeakObjectPtr<UObject>> Objects;Layout.GetObjectsBeingCustomized(Objects);
    if(Objects.Num()>0 && Cast<UDialogueNode_Player>(Objects[0].Get()))
    {
        // Options use the dialogue line in Vertigo; hide the separate input to avoid authoring text there.
        Layout.HideProperty(Layout.GetProperty(TEXT("OptionText"),UDialogueNode_Player::StaticClass()));
        if(Objects.Num()==1)AddField(Layout,Layout.GetProperty(TEXT("HintText"),UDialogueNode_Player::StaticClass()));
    }
}
}
TSharedRef<SWidget> MakeTextRow(TSharedPtr<IPropertyHandle> Property)
{
    FString Label=Property->GetPropertyDisplayName().ToString();
    if(Property->GetProperty()->GetOwnerStruct()==FDialogueLine::StaticStruct())
    {
        const auto Parent=Property->GetParentHandle();
        const int32 Index=Parent.IsValid()?Parent->GetIndexInArray():INDEX_NONE;
        Label=Index==INDEX_NONE?TEXT("台词"):FString::Printf(TEXT("备选台词 %d"),Index+1);
    }
    return SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(STextBlock).Text(FText::FromString(TEXT("Origin — ")+Label))]
            +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([](){return FTextLocalizationManager::Get().IsLocalizationLocked()?EVisibility::Collapsed:EVisibility::Visible;})[Property->CreatePropertyValueWidget()]]
            +SVerticalBox::Slot().AutoHeight()[SNew(SMultiLineEditableTextBox).IsReadOnly(true)
                .Visibility_Lambda([](){return FTextLocalizationManager::Get().IsLocalizationLocked()?EVisibility::Visible:EVisibility::Collapsed;})
                .Text_Lambda([Property](){FText Value;if(Property->GetValue(Value)!=FPropertyAccess::Success)return FText::GetEmpty();const auto* Source=FTextInspector::GetSourceString(Value);return FText::AsCultureInvariant(Source?*Source:Value.ToString());})]
            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).ColorAndOpacity(FLinearColor(1,.65f,.2f)).AutoWrapText(true)
                .Visibility_Lambda([](){return FTextLocalizationManager::Get().IsLocalizationLocked()?EVisibility::Visible:EVisibility::Collapsed;})
                .Text(FText::FromString(TEXT("Origin 当前只读；语言预览期间请回到 origin 后编辑原文。译文仍可编辑。")))]
            +SVerticalBox::Slot().AutoHeight().Padding(0,4)[VTGManualLocalization::MakeInlineTranslations(Property)];
}
bool IsSafeText(TSharedPtr<IPropertyHandle> Property)
{
    if(!Property || !Property->IsValidHandle() || !CastField<FTextProperty>(Property->GetProperty()))return false;
    auto* Field=Property->GetProperty();
    if(Field->HasMetaData(TEXT("VTGLocalizationLocked")))return false;
    TArray<UObject*> Owners;Property->GetOuterObjects(Owners);
    if(Owners.Num()!=1)return false;
    if(Cast<UDialogue>(Owners[0]))return Field->GetOwnerStruct()==FSpeakerInfo::StaticStruct() && Field->GetFName()==TEXT("SpeakerName");
    if(!Cast<UDialogueNode>(Owners[0]))return false;
    if(Field->GetOwnerStruct()==FDialogueLine::StaticStruct() && Field->GetFName()==TEXT("Text"))return true;
    return Field->GetOwnerStruct()==UDialogueNode_Player::StaticClass() && (Field->GetFName()==TEXT("OptionText") || Field->GetFName()==TEXT("HintText"));
}
void Startup()
{
    Handle=GetNarrativeDialogueDetailsExtension().AddStatic(&Customize);
    auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    Module.RegisterCustomPropertyTypeLayout(TEXT("SpeakerInfo"),FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSpeakerDetails::Make));
    Module.RegisterCustomPropertyTypeLayout(TEXT("PlayerSpeakerInfo"),FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSpeakerDetails::Make));
}
void Shutdown()
{
    GetNarrativeDialogueDetailsExtension().Remove(Handle);
    if(FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
    {
        auto& Module=FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
        Module.UnregisterCustomPropertyTypeLayout(TEXT("SpeakerInfo"));Module.UnregisterCustomPropertyTypeLayout(TEXT("PlayerSpeakerInfo"));
    }
}
}
