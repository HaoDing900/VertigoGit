#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/** Pins the "Stage" category to sit right under "Default" in the level manager's details panel. */
class FVTGLevelManagerDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
