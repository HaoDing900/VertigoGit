#include "VTGLevelManagerDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"

TSharedRef<IDetailCustomization> FVTGLevelManagerDetails::MakeInstance()
{
	return MakeShareable(new FVTGLevelManagerDetails);
}

void FVTGLevelManagerDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.SortCategories([](const TMap<FName, IDetailCategoryBuilder*>& CategoryMap)
	{
		//Find where "Default" sits, then drop "Stage" immediately after it so the test fields are
		//among the first things you see when you select the level manager.
		int32 DefaultOrder = 0;
		if (IDetailCategoryBuilder* const* Default = CategoryMap.Find(FName("Default")))
		{
			DefaultOrder = (*Default)->GetSortOrder();
		}

		if (IDetailCategoryBuilder* const* Stage = CategoryMap.Find(FName("Stage")))
		{
			(*Stage)->SetSortOrder(DefaultOrder + 1);
		}
	});
}
