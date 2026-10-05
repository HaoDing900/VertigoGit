#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VTGBossHealthBar.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * Souls / Yakuza style boss bar for the HUD: name above a long thin bar at the bottom of the screen.
 *
 *  - The red bar drops the instant a hit lands.
 *  - A pale "damage trail" bar behind it holds for a moment, then drains down to the new value,
 *    so a combo reads as one chunk of lost health.
 *  - Damage dealt during that combo is summed into a number at the right end, which clears once
 *    the hits stop.
 *
 * All behaviour is here; the look (WBP_BossHealthBar) is a Widget Blueprint deriving from this,
 * bound by widget name. Driven by UVTGBossHealthBarComponent on the boss.
 */
UCLASS(Abstract)
class VERTIGO_API UVTGBossHealthBar : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Boss Health Bar")
	void SetBossName(const FText& Name);

	/** Feed the current health. Decreases start the trail + damage counter; increases snap. */
	UFUNCTION(BlueprintCallable, Category = "Boss Health Bar")
	void SetHealth(float Current, float Max);

	/** Fade in (or out). The widget stays in the viewport while hidden so a re-show is instant. */
	UFUNCTION(BlueprintCallable, Category = "Boss Health Bar")
	void SetShown(bool bShow);

	UFUNCTION(BlueprintPure, Category = "Boss Health Bar")
	bool IsFullyHidden() const { return !bShown && GetRenderOpacity() <= 0.f; }

	/** Seconds the trail waits after the latest hit before draining. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Health Bar")
	float TrailHoldTime = 0.7f;

	/** Trail drain speed, in fractions of the full bar per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Health Bar")
	float TrailDrainSpeed = 0.6f;

	/** Seconds without a hit before the summed damage number clears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Health Bar")
	float DamageNumberHoldTime = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Health Bar")
	float FadeTime = 0.35f;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BossNameText;

	/** Front bar: the real health. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	/** Back bar: lags behind HealthBar to show the chunk just lost. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> DamageTrailBar;

	/** Summed damage of the current combo. Optional. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DamageText;

private:
	float HealthPercent = 1.f;
	float TrailPercent = 1.f;
	float LastHealth = -1.f;
	float ComboDamage = 0.f;
	float SinceLastHit = 0.f;
	bool bShown = false;
};
