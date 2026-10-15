#include "UI/VTGBossHealthBar.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"

void UVTGBossHealthBar::SetCausticOverlay(UMaterialInterface* Material,float Opacity)
{
 CausticMaterial=Material?UMaterialInstanceDynamic::Create(Material,this):nullptr;
 CausticBrush.SetResourceObject(CausticMaterial);CausticBrush.DrawAs=ESlateBrushDrawType::Image;
 CausticBrush.TintColor=FLinearColor(1,1,1,FMath::Clamp(Opacity,0.f,1.f));
 if(CausticMaterial)CausticMaterial->SetScalarParameterValue(TEXT("Opacity"),.65f);
}
int32 UVTGBossHealthBar::NativePaint(const FPaintArgs& Args,const FGeometry& Geo,const FSlateRect& Cull,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const
{
 Layer=Super::NativePaint(Args,Geo,Cull,Out,Layer,Style,Enabled);
 if(!CausticMaterial || !HealthBar || HealthPercent<=0)return Layer;
 const auto& BarGeo=HealthBar->GetCachedGeometry();const FVector2D Size=BarGeo.GetLocalSize();if(Size.X<=0 || Size.Y<=0)return Layer;
 #if WITH_EDITOR
 if(IsRunningCommandlet()){static bool Logged=false;if(!Logged){Logged=true;UE_LOG(LogTemp,Display,TEXT("CAUSTIC OVERLAY PAINT size=%s percent=%f opacity=%f"),*Size.ToString(),HealthPercent,GetRenderOpacity());}}
 #endif
 const FVector2D Position=Geo.AbsoluteToLocal(BarGeo.LocalToAbsolute(FVector2D::ZeroVector));
 Out.PushClip(FSlateClippingZone(Geo.ToPaintGeometry(FVector2D(Size.X*HealthPercent,Size.Y),FSlateLayoutTransform(Position))));
 FSlateDrawElement::MakeBox(Out,++Layer,Geo.ToPaintGeometry(Size,FSlateLayoutTransform(Position)),&CausticBrush,ESlateDrawEffect::None,Style.GetColorAndOpacityTint()*CausticBrush.GetTint(Style));
 Out.PopClip();return Layer;
}

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
