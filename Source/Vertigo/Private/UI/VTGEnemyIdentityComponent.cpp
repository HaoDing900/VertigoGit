#include "UI/VTGEnemyIdentityComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Font.h"
#include "UObject/UnrealType.h"
UVTGEnemyIdentityComponent::UVTGEnemyIdentityComponent()
{
 PrimaryComponentTick.bCanEverTick=true;
 PrimaryComponentTick.TickInterval=.1f;
 FallbackName=NSLOCTEXT("VTG","EnemyNameFallback","Enemy");
}
void UVTGEnemyIdentityComponent::BeginPlay(){Super::BeginPlay();RefreshName();}
void UVTGEnemyIdentityComponent::RefreshName()
{
 if (!GetOwner()) return;
 auto Name=FallbackName;
 if (auto* Prop=FindFProperty<FTextProperty>(GetOwner()->GetClass(),TEXT("EnemyDisplayName")))
 {
  auto Value=Prop->GetPropertyValue_InContainer(GetOwner());
  if (!Value.IsEmpty()) Name=Value;
 }
 for (UActorComponent* C : GetOwner()->GetComponents())
  if (auto* Bar=Cast<UWidgetComponent>(C))
  {
   auto* Widget=Bar->GetUserWidgetObject();
   auto* Text=Widget ? Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("EnemyName"))) : nullptr;
   if (!Text) continue;
   // Enforce the shared overhead typeface even for a custom enemy widget.
   static TWeakObjectPtr<UFont> LightFont;
   if (!LightFont.IsValid()) LightFont=LoadObject<UFont>(nullptr,TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Light_Font"));
   if (LightFont.IsValid())
   {
    auto Font=Text->GetFont();
    if (Font.FontObject!=LightFont.Get())
    {Font.FontObject=LightFont.Get();Font.TypefaceFontName=NAME_None;Text->SetFont(Font);}
   }
   const auto Size=Bar->GetDrawSize();
   if (Size.X<180 || Size.Y<40) Bar->SetDrawSize(FVector2D(FMath::Max(Size.X,180),FMath::Max(Size.Y,40)));
   if (!Text->GetText().EqualTo(Name)) Text->SetText(Name);
  }
}
void UVTGEnemyIdentityComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Fn)
{Super::TickComponent(Dt,Type,Fn);RefreshName();}
