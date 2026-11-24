#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VTGSaveIndicator.generated.h"
class UCircularThrobber;
class UTextBlock;

/**
 * The small "saving" spinner in the bottom-right corner, so the hitch the autosave causes is something
 * the player is told about rather than something that just happens to them.
 *
 * It cannot spin DURING the write: SaveToSlot is synchronous and blocks the game thread. So the
 * coordinator shows it one tick BEFORE writing and keeps it up for a moment after, which puts an
 * animating spinner either side of the stall instead of a frozen one in the middle of it.
 */
UCLASS()
class VERTIGO_API UVTGSaveIndicator : public UUserWidget
{
 GENERATED_BODY()
public:
 /** Show it. Stays up for at least MinimumVisibleTime even if the write finishes instantly. */
 void Begin();
 /** The write is done; hide once the minimum time has passed. */
 void Finish();
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
 friend struct FVTGTerminalSaveRegression;
 UPROPERTY(Transient) TObjectPtr<UCircularThrobber> Spinner;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> Label;
 float MinimumVisibleTime = 0.8f;
 float Elapsed = 0.f;
 bool bFinished = false;
};
