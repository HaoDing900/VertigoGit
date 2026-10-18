#pragma once
#include "AnimGraphNode_Base.h"
#include "AnimNode_NarrativeJaw.h"
#include "AnimGraphNode_NarrativeJaw.generated.h"

UCLASS()
class NARRATIVEDIALOGUEEDITOR_API UAnimGraphNode_NarrativeJaw : public UAnimGraphNode_Base
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = Settings) FAnimNode_NarrativeJaw Node;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override { return NSLOCTEXT("VTG", "NarrativeJaw", "Narrative Jaw Movement (Jaw Only)"); }
    virtual FText GetMenuCategory() const override { return NSLOCTEXT("VTG", "NarrativeCategory", "Narrative"); }
};
