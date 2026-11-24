#include "VTGAssetDisplayLocalizationDetails.h"
#include "VTGDialogueLocalizationDetails.h"
#include "UI/VTGHintLayer.h"
#include "Loading/VTGLevelLoadingInfo.h"
#include "Save/VTGSaveDisplayNames.h"
#include "UI/VTGInfoInGamePage.h"
#include "PropertyHandle.h"
#include "PropertyEditorModule.h"
#include "IPropertyTypeCustomization.h"
#include "DetailWidgetRow.h"
#include "Engine/DataTable.h"
#include "UObject/UObjectHash.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

namespace VTGAssetDisplayLocalizationDetails
{
namespace
{
bool IsWidgetTextField(const FProperty* Field)
{
    return Field && Field->GetOwnerStruct()==UTextBlock::StaticClass() && Field->GetFName()==TEXT("Text");
}
bool IsRequestedWidgetText(const IPropertyHandle& Property)
{
    TArray<UObject*> Objects;Property.GetOuterObjects(Objects);
    if(Objects.Num()!=1)return false;
    const auto* Text=Cast<UTextBlock>(Objects[0]);
    if(!Text)return false;
    const auto* BP=Text->GetTypedOuter<UWidgetBlueprint>();
    // The UMG Designer may edit a preview instance instead of the source tree.
    if(!BP)
        if(const auto* Tree=Text->GetTypedOuter<UWidgetTree>())
            if(const auto* Preview=Cast<UUserWidget>(Tree->GetOuter()))BP=Cast<UWidgetBlueprint>(Preview->GetClass()->ClassGeneratedBy);
    if(!BP)return false;
    const FString Path=BP->GetPathName();
    if(Path==TEXT("/Game/Widget/Loading/WBP_VTGLoadingScreen.WBP_VTGLoadingScreen"))return Text->GetFName()==TEXT("LoadingLabel");
    if(Path!=TEXT("/Game/ThirdPerson/Blueprints/UI/WBP_PauseMenu.WBP_PauseMenu"))return false;
    for(const UWidget* Parent=Text->GetParent();Parent;Parent=Parent->GetParent())
        if(Cast<UButton>(Parent))return Parent->GetFName()==TEXT("B_Continue") || Parent->GetFName()==TEXT("B_Language") || Parent->GetFName()==TEXT("B_Exit");
    return false;
}
bool IsDisplayField(const FProperty* Field)
{
    if(!CastField<FTextProperty>(Field) || Field->HasMetaData(TEXT("VTGLocalizationLocked")))return false;
    const auto* Owner=Field->GetOwnerStruct();if(!Owner)return false;
    if(IsWidgetTextField(Field))return true;
    const FString Path=Owner->GetPathName();
    const FName Name=Field->GetOwnerProperty()->GetFName();
    if(Path==TEXT("/InventorySystemX/Modules/Notes/DataAssets/PDA_NoteText.PDA_NoteText_C"))return Name==TEXT("Name") || Name==TEXT("Pages");
    if(Path==TEXT("/InventorySystemX/Modules/Notes/DataAssets/PDA_NoteOptions.PDA_NoteOptions_C"))return Name==TEXT("Text");
    if(Path==TEXT("/InventorySystemX/InventorySystemX/Structures/UI/S_TabInfo.S_TabInfo"))return Name.ToString().StartsWith(TEXT("Text_"));
    if(Path==TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/WB_TabButton.WB_TabButton_C"))return Name==TEXT("Text");
    if(Path==TEXT("/InventorySystemX/InventorySystemX/Components/AC_ActorInteractionComponent.AC_ActorInteractionComponent_C"))return Name==TEXT("Interaction Text");
    if(Path==TEXT("/Game/Narrative/Mail/S_MailRow.S_MailRow"))return Name.ToString().StartsWith(TEXT("SenderDisplayName_"));
    if(Path==TEXT("/Game/AI/AIBehaviorSystem/Blueprint/AI/BP_AI_Base.BP_AI_Base_C"))return Name==TEXT("EnemyDisplayName");
    if(Owner==UVTGLevelLoadingInfo::StaticClass())return Name==GET_MEMBER_NAME_CHECKED(UVTGLevelLoadingInfo,NewsBodies);
    if(Owner==FVTGHintDisplayText::StaticStruct())return Name==TEXT("Text");
    if(Owner==FVTGSaveDisplayRow::StaticStruct())return Name==GET_MEMBER_NAME_CHECKED(FVTGSaveDisplayRow,Display);
    // The info pages carry all of their wording in two tables, so every text column on them is
    // player-facing by construction.
    if(Owner==FVTGInfoPageRow::StaticStruct())return Name==GET_MEMBER_NAME_CHECKED(FVTGInfoPageRow,Breadcrumb)
        || Name==GET_MEMBER_NAME_CHECKED(FVTGInfoPageRow,Title) || Name==GET_MEMBER_NAME_CHECKED(FVTGInfoPageRow,Body)
        || Name==GET_MEMBER_NAME_CHECKED(FVTGInfoPageRow,DismissLabel);
    if(Owner==FVTGInfoPageLineRow::StaticStruct())return Name==GET_MEMBER_NAME_CHECKED(FVTGInfoPageLineRow,Label);
    return Owner==UVTGHintLayer::StaticClass() && Name==TEXT("NewMailHintText");
}
class FDisplayIdentifier:public IPropertyTypeIdentifier
{
public:
    virtual bool IsPropertyTypeCustomized(const IPropertyHandle& Property)const override
    {return IsDisplayField(Property.GetProperty()) && (!IsWidgetTextField(Property.GetProperty()) || IsRequestedWidgetText(Property));}
};
TSharedPtr<IPropertyTypeIdentifier> Identifier;
class FDisplayText:public IPropertyTypeCustomization
{
public:
    static TSharedRef<IPropertyTypeCustomization> Make(){return MakeShared<FDisplayText>();}
    virtual void CustomizeHeader(TSharedRef<IPropertyHandle> Property,FDetailWidgetRow& Row,IPropertyTypeCustomizationUtils&)override
    {Row.WholeRowContent()[VTGDialogueLocalizationDetails::MakeTextRow(Property)];}
    virtual void CustomizeChildren(TSharedRef<IPropertyHandle>,IDetailChildrenBuilder&,IPropertyTypeCustomizationUtils&)override{}
};
}
bool GetDataTableRowContext(TSharedPtr<IPropertyHandle> Property,FString& Context)
{
    // Any whitelisted field owned by a plain struct may live in a DataTable row; the scan below is
    // what decides, by finding the row whose memory this handle points at. Class-owned fields never
    // are, so they leave early rather than walking every object in the package.
    if(!Property || !IsDisplayField(Property->GetProperty()))return false;
    if(const auto* Owner=Property->GetProperty()->GetOwnerStruct();!Owner || Cast<UClass>(Owner))return false;
    TArray<void*> Values;Property->AccessRawData(Values);if(Values.Num()!=1 || !Values[0])return false;
    TArray<UPackage*> Packages;Property->GetOuterPackages(Packages);
    for(auto* Package:Packages)
    {
        TArray<UObject*> Objects;GetObjectsWithPackage(Package,Objects,false);
        for(auto* Object:Objects)if(auto* Table=Cast<UDataTable>(Object))
            if(Table->GetRowStruct()==Property->GetProperty()->GetOwnerStruct())
                for(const auto& Row:Table->GetRowMap())if(Property->GetProperty()->ContainerPtrToValuePtr<void>(Row.Value)==Values[0])
                {Context=Table->GetPathName()+TEXT(" : Row[")+Row.Key.ToString()+TEXT("].")+Property->GetProperty()->GetName();return true;}
    }
    return false;
}
bool IsSafeText(TSharedPtr<IPropertyHandle> Property)
{
    if(!Property || !Property->IsValidHandle() || !IsDisplayField(Property->GetProperty()))return false;
    if(IsWidgetTextField(Property->GetProperty()))return IsRequestedWidgetText(*Property);
    TArray<UObject*> Objects;Property->GetOuterObjects(Objects);
    if(Objects.Num()==1)
    {
        const auto* Owner=Property->GetProperty()->GetOwnerStruct();
        if(Owner==FVTGHintDisplayText::StaticStruct())return Cast<UVTGHintLayer>(Objects[0])!=nullptr;
        if(const auto* Class=Cast<UClass>(Owner))return Objects[0]->IsA(Class);
        if(Owner->GetPathName()==TEXT("/InventorySystemX/InventorySystemX/Structures/UI/S_TabInfo.S_TabInfo"))return true;
    }
    FString Context;return GetDataTableRowContext(Property,Context);
}
void Startup()
{
    Identifier=MakeShared<FDisplayIdentifier>();
    FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).RegisterCustomPropertyTypeLayout(TEXT("TextProperty"),FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDisplayText::Make),Identifier);
}
void Shutdown()
{
    if(FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).UnregisterCustomPropertyTypeLayout(TEXT("TextProperty"),Identifier);
    Identifier.Reset();
}
}
