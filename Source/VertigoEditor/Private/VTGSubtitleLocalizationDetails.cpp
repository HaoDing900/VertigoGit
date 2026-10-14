#include "VTGSubtitleLocalizationDetails.h"
#include "VTGDialogueLocalizationDetails.h"
#include "UI/VTGSubtitleTrack.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailWidgetRow.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"

namespace VTGSubtitleLocalizationDetails
{
namespace
{
const FName ClassName(TEXT("MovieSceneVTGSubtitleSection"));
class FSubtitleDetails : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> Make(){return MakeShared<FSubtitleDetails>();}
    virtual void CustomizeDetails(IDetailLayoutBuilder& Layout) override
    {
        for(const FName Field:{FName(TEXT("Text")),FName(TEXT("Speaker"))})
        {
            auto Property=Layout.GetProperty(Field,UMovieSceneVTGSubtitleSection::StaticClass());
            if(!IsSafeText(Property))continue;
            const auto Content=VTGDialogueLocalizationDetails::MakeTextRow(Property);
            if(auto* Row=Layout.EditDefaultProperty(Property))Row->CustomWidget(false).WholeRowContent()[Content];
            else
            {
                Layout.HideProperty(Property);
                Layout.EditCategory(TEXT("Subtitle")).AddCustomRow(Property->GetPropertyDisplayName()).WholeRowContent()[Content];
            }
        }
    }
};
}
bool IsSafeText(TSharedPtr<IPropertyHandle> Property)
{
    if(!Property || !Property->IsValidHandle() || !CastField<FTextProperty>(Property->GetProperty()))return false;
    auto* Field=Property->GetProperty();
    if(Field->HasMetaData(TEXT("VTGLocalizationLocked")) || Field->GetOwnerStruct()!=UMovieSceneVTGSubtitleSection::StaticClass())return false;
    if(Field->GetFName()!=TEXT("Text") && Field->GetFName()!=TEXT("Speaker"))return false;
    TArray<UObject*> Owners;Property->GetOuterObjects(Owners);
    return Owners.Num()==1 && Cast<UMovieSceneVTGSubtitleSection>(Owners[0]);
}
void Startup()
{
    FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"))
        .RegisterCustomClassLayout(ClassName,FOnGetDetailCustomizationInstance::CreateStatic(&FSubtitleDetails::Make));
}
void Shutdown()
{
    if(FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
        FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).UnregisterCustomClassLayout(ClassName);
}
}
