#include "UI/VTGBossHealthBar.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UVTGBossHealthBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetRenderOpacity(0.f);
	if (DamageText)
	{
		DamageText->SetText(FText::GetEmpty());
	}
}

void UVTGBossHealthBar::SetBossName(const FText& Name)
{
	if (BossNameText)
	{
		BossNameText->SetText(Name);
	}
}

void UVTGBossHealthBar::SetHealth(float Current, float Max)
{
	const float NewPercent = Max > 0.f ? FMath::Clamp(Current / Max, 0.f, 1.f) : 0.f;

	if (LastHealth >= 0.f && Current < LastHealth)
	{
		// A hit: the front bar drops now, the trail holds where it was.
		ComboDamage += LastHealth - Current;
		SinceLastHit = 0.f;
	}
	else if (NewPercent > TrailPercent || LastHealth < 0.f)
	{
		// First value, a heal or a reset: no trail to show.
		TrailPercent = NewPercent;
	}

	LastHealth = Current;
	HealthPercent = NewPercent;
	if (HealthBar)
	{
		HealthBar->SetPercent(HealthPercent);
	}
	if (DamageTrailBar)
	{
		DamageTrailBar->SetPercent(TrailPercent);
	}
	if (DamageText && ComboDamage > 0.f)
	{
		DamageText->SetText(FText::AsNumber(FMath::RoundToInt(ComboDamage)));
	}
}

void UVTGBossHealthBar::SetShown(bool bShow)
{
	bShown = bShow;
}

void UVTGBossHealthBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float TargetOpacity = bShown ? 1.f : 0.f;
	SetRenderOpacity(FMath::FInterpConstantTo(GetRenderOpacity(), TargetOpacity, InDeltaTime, 1.f / FMath::Max(FadeTime, KINDA_SMALL_NUMBER)));

	SinceLastHit += InDeltaTime;
	if (TrailPercent > HealthPercent && SinceLastHit >= TrailHoldTime)
	{
		TrailPercent = FMath::Max(HealthPercent, TrailPercent - TrailDrainSpeed * InDeltaTime);
		if (DamageTrailBar)
		{
			DamageTrailBar->SetPercent(TrailPercent);
		}
	}

	if (ComboDamage > 0.f && SinceLastHit >= DamageNumberHoldTime)
	{
		ComboDamage = 0.f;
		if (DamageText)
		{
			DamageText->SetText(FText::GetEmpty());
		}
	}
}
