#pragma once
#include "Commandlets/Commandlet.h"
#include "VTGExportSlideshowNodesCommandlet.generated.h"
UCLASS()
class UVTGExportSlideshowNodesCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVTGExportSlideshowNodesCommandlet();
 virtual int32 Main(const FString& Params) override;
};
