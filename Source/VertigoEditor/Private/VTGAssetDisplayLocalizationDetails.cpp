#include "VTGAssetDisplayLocalizationDetails.h"
#include "VTGDialogueLocalizationDetails.h"
#include "UI/VTGHintLayer.h"
#include "PropertyHandle.h"
#include "PropertyEditorModule.h"
#include "IPropertyTypeCustomization.h"
#include "DetailWidgetRow.h"
#include "Engine/DataTable.h"
#include "UObject/UObjectHash.h"

namespace VTGAssetDisplayLocalizationDetails
{
namespace
{
bool IsDisplayField(const FProperty* Field)
{
    if(!CastField<FTextProperty>(Field) || Field->HasMetaData(TEXT("VTGLocalizationLocked")))return false;
    const auto* Owner=Field->GetOwnerStruct();if(!Owner)return false;
    const FString Path=Owner->GetPathName();
    const FName Name=Field->GetOwnerProperty()->GetFName();
    if(Path==TEXT("/InventorySystemX/Modules/Notes/DataAssets/PDA_NoteText.PDA_NoteText_C"))return Name==TEXT("Name") || Name==TEXT("Pages");
    if(Path==TEXT("/InventorySystemX/Modules/Notes/DataAssets/PDA_NoteOptions.PDA_NoteOptions_C"))return Name==TEXT("Text");
    if(Path==TEXT("/InventorySystemX/InventorySystemX/Structures/UI/S_TabInfo.S_TabInfo"))return Name.ToString().StartsWith(TEXT("Text_"));
    if(Path==TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/WB_TabButton.WB_TabButton_C"))return Name==TEXT("Text");
    if(Path==TEXT("/InventorySystemX/InventorySystemX/Components/AC_ActorInteractionComponent.AC_ActorInteractionComponent_C"))return Name==TEXT("Interaction Text");
    if(Path==TEXT("/Game/Narrative/Mail/S_MailRow.S_MailRow"))return Name.ToString().StartsWith(TEXT("SenderDisplayName_"));
    if(Owner==FVTGHintDisplayText::StaticStruct())return Name==TEXT("Text");
    return Owner==UVTGHintLayer::StaticClass() && Name==TEXT("NewMailHintText");
}
class FDisplayIdentifier:public IPropertyTypeIdentifier
{
public:
    virtual bool IsPropertyTypeCustomized(const IPropertyHandle& Property)const override{return IsDisplayField(Property.GetProperty());}
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
bool GetMailContext(TSharedPtr<IPropertyHandle> Property,FString& Context)
{
    if(!Property || !IsDisplayField(Property->GetProperty()) || Property->GetProperty()->GetOwnerStruct()->GetPathName()!=TEXT("/Game/Narrative/Mail/S_MailRow.S_MailRow"))return false;
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
    TArray<UObject*> Objects;Property->GetOuterObjects(Objects);
    if(Objects.Num()==1)
    {
        const auto* Owner=Property->GetProperty()->GetOwnerStruct();
        if(Owner==FVTGHintDisplayText::StaticStruct())return Cast<UVTGHintLayer>(Objects[0])!=nullptr;
        if(const auto* Class=Cast<UClass>(Owner))return Objects[0]->IsA(Class);
        if(Owner->GetPathName()==TEXT("/InventorySystemX/InventorySystemX/Structures/UI/S_TabInfo.S_TabInfo"))return true;
    }
    FString Context;return GetMailContext(Property,Context);
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
