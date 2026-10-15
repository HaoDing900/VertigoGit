#include "UI/VTGActionHUD.h"
#include "Components/TextBlock.h"
#include "CommonInputSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "Components/Image.h"
#include "CommonInputBaseTypes.h"

FKey UVTGActionHUD::MappedKeyForAction(const UInputAction* Action) const
{
 auto* Local=GetOwningLocalPlayer();
 auto* Input=Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
 auto* Device=Local ? Local->GetSubsystem<UCommonInputSubsystem>() : nullptr;
 const bool bGamepad=Device && Device->GetCurrentInputType()==ECommonInputType::Gamepad;
 if (Input && Action)
  for (const auto& Key : Input->QueryKeysMappedToAction(Action))
   if (Key.IsGamepadKey()==bGamepad)
    return Key;
 return EKeys::Invalid;
}
FText UVTGActionHUD::KeyForAction(const UInputAction* Action) const
{
 const FKey Key=MappedKeyForAction(Action);
 if(Key.IsValid())
   {
    if (Key==EKeys::LeftMouseButton) return FText::FromString(TEXT("LMB"));
    if (Key==EKeys::RightMouseButton) return FText::FromString(TEXT("RMB"));
    if (Key==EKeys::LeftControl || Key==EKeys::RightControl) return FText::FromString(TEXT("Ctrl"));
    if (Key==EKeys::LeftShift || Key==EKeys::RightShift) return FText::FromString(TEXT("Shift"));
    if (Key==EKeys::Gamepad_RightTrigger || Key==EKeys::Gamepad_RightTriggerAxis) return FText::FromString(TEXT("RT"));
    if (Key==EKeys::Gamepad_LeftTrigger || Key==EKeys::Gamepad_LeftTriggerAxis) return FText::FromString(TEXT("LT"));
    if (Key==EKeys::Gamepad_RightShoulder) return FText::FromString(TEXT("RB"));
    if (Key==EKeys::Gamepad_LeftShoulder) return FText::FromString(TEXT("LB"));
    return Key.GetDisplayName();
   }
 return FText::FromString(TEXT("—"));
}
void UVTGActionHUD::RefreshHints()
{
 for (const auto& Pair : {TPair<FName,UInputAction*>(TEXT("DodgeKey"),DodgeAction),TPair<FName,UInputAction*>(TEXT("PunchKey"),PunchAction)})
  if (auto* Text=Cast<UTextBlock>(GetWidgetFromName(Pair.Key)))
  {
   auto* Icon=Cast<UImage>(GetWidgetFromName(*FString::Printf(TEXT("%sIcon"),*Pair.Key.ToString())));
   auto* Local=GetOwningLocalPlayer();auto* Device=Local?Local->GetSubsystem<UCommonInputSubsystem>():nullptr;
   FSlateBrush Brush;
   const bool HasIcon=Icon && UCommonInputPlatformSettings::Get()->TryGetInputBrush(Brush,MappedKeyForAction(Pair.Value),Device?Device->GetCurrentInputType():ECommonInputType::MouseAndKeyboard,Device?Device->GetCurrentGamepadName():NAME_None);
   if(Icon){if(HasIcon)Icon->SetBrush(Brush);Icon->SetVisibility(HasIcon?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);}
   Text->SetVisibility(HasIcon?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
   const auto Value=KeyForAction(Pair.Value);
   if (!Text->GetText().EqualTo(Value)) Text->SetText(Value);
  }
}
void UVTGActionHUD::RefreshBloodOverlay(bool bInCombat)
{
 if(auto* Blood=GetWidgetFromName(TEXT("BloodOverlay")))
  Blood->SetVisibility(bInCombat && Health>0 && MaxHealth>0 && Health/MaxHealth<=.15f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
