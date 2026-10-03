#pragma once
#include "IPropertyTypeCustomization.h"
class FVTGSlideDetails : public IPropertyTypeCustomization
{
public:
 static TSharedRef<IPropertyTypeCustomization> MakeInstance();
 virtual void CustomizeHeader(TSharedRef<IPropertyHandle>, FDetailWidgetRow&, IPropertyTypeCustomizationUtils&) override;
 virtual void CustomizeChildren(TSharedRef<IPropertyHandle>, IDetailChildrenBuilder&, IPropertyTypeCustomizationUtils&) override;
};
