#include "VTGMapLocalizationDetails.h"
#include "UI/VTGMapLocationNode.h"
#include "VTGDialogueLocalizationDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "IDetailCustomization.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"

namespace VTGMapLocalizationDetails
{
bool IsSafeText(TSharedPtr<IPropertyHandle> Property)
{
    if (!Property || !Property->IsValidHandle()) return false;
    auto* Field = Property->GetProperty();
    if (!CastField<FTextProperty>(Field) || Field->GetOwnerStruct() != UVTGMapLocationNode::StaticClass()) return false;
    const FName Name = Field->GetFName();
    if (Name != TEXT("LocationDisplayName") && Name != TEXT("LocationDescription")) return false;
    TArray<UObject*> Owners; Property->GetOuterObjects(Owners);
    return Owners.Num() == 1 && Cast<UVTGMapLocationNode>(Owners[0]);
}
class FMapDetails : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> Make() { return MakeShared<FMapDetails>(); }
    virtual void CustomizeDetails(IDetailLayoutBuilder& Layout) override
    {
        for (const FName Name : {FName(TEXT("LocationDisplayName")), FName(TEXT("LocationDescription"))})
        {
            auto Property = Layout.GetProperty(Name, UVTGMapLocationNode::StaticClass());
            if (IsSafeText(Property))
                if (auto* Row = Layout.EditDefaultProperty(Property))
                    Row->CustomWidget(false).WholeRowContent()[VTGDialogueLocalizationDetails::MakeTextRow(Property)];
        }
    }
};
void Startup()
{
    FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"))
        .RegisterCustomClassLayout(TEXT("VTGMapLocationNode"), FOnGetDetailCustomizationInstance::CreateStatic(&FMapDetails::Make));
}
void Shutdown()
{
    if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
        FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).UnregisterCustomClassLayout(TEXT("VTGMapLocationNode"));
}
}
