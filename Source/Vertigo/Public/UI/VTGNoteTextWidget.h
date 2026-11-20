#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VTGNoteTextWidget.generated.h"

/** Native layout policy for WB_TextContainer; Blueprint still supplies text, size and style. */
UCLASS()
class VERTIGO_API UVTGNoteTextWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void FitDocument();
protected:
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    uint32 LastLayoutHash = 0;
    bool bHasLayout = false;
    UPROPERTY(Transient) TObjectPtr<class UScrollBox> OverflowScroll;
    int32 OriginalSize = 0;
    class UTextBlock* Body() const;
    class USizeBox* Frame() const;
    FVector2D AvailableSize() const;
    FVector2D Measure(int32 Size) const;
    int32 BaseSize();
    void Apply(int32 Reduction);
};
