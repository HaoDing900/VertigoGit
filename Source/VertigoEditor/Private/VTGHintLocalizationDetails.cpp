#include "VTGHintLocalizationDetails.h"
#include "UI/VTGHintLayer.h"
#include "VTGDialogueLocalizationDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "IDetailCustomization.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"
namespace VTGHintLocalizationDetails
{
bool IsSafeText(TSharedPtr<IPropertyHandle> Property)
{
    if(!Property || !Property->IsValidHandle())return false;
    auto* Field=Property->GetProperty();
    if(!CastField<FTextProperty>(Field) || Field->GetOwnerStruct()!=UVTGHintLayer::StaticClass())return false;
    if(Field->GetFName()!=TEXT("SkipHoldText") && Field->GetFName()!=TEXT("SkipActionText"))return false;
    TArray<UObject*> Owners;Property->GetOuterObjects(Owners);return Owners.Num()==1 && Cast<UVTGHintLayer>(Owners[0]);
}
class FHintDetails:public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> Make(){return MakeShared<FHintDetails>();}
    virtual void CustomizeDetails(IDetailLayoutBuilder& Layout)override
    {
        for(FName Name:{FName(TEXT("SkipHoldText")),FName(TEXT("SkipActionText"))})
        {
            auto Property=Layout.GetProperty(Name,UVTGHintLayer::StaticClass());
            if(IsSafeText(Property))if(auto* Row=Layout.EditDefaultProperty(Property))Row->CustomWidget(false).WholeRowContent()[VTGDialogueLocalizationDetails::MakeTextRow(Property)];
        }
    }
};
void Startup(){FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).RegisterCustomClassLayout(TEXT("VTGHintLayer"),FOnGetDetailCustomizationInstance::CreateStatic(&FHintDetails::Make));}
void Shutdown(){if(FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).UnregisterCustomClassLayout(TEXT("VTGHintLayer"));}
}
