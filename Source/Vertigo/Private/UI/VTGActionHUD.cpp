#include "UI/VTGActionHUD.h"
#include "Components/TextBlock.h"
#include "CommonInputSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "Components/Image.h"
#include "CommonInputBaseTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

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

void UVTGActionHUD::EnsureRecoveryWidgets()
{
 if(RecoveryBox || !WidgetTree)return;
 auto* Root=Cast<UCanvasPanel>(WidgetTree->RootWidget);if(!Root)return;
 RecoveryBox=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("AttackRecovery"));
 auto* RecoverySlot=Root->AddChildToCanvas(RecoveryBox);RecoverySlot->SetAnchors(FAnchors(1,1));RecoverySlot->SetPosition(FVector2D(-300,-59));RecoverySlot->SetSize(FVector2D(260,22));
 auto* Label=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("AttackRecoveryLabel"));
 Label->SetText(NSLOCTEXT("VTG","AttackRecovery","Recovery"));auto Font=Label->GetFont();Font.Size=12;Font.OutlineSettings.OutlineSize=1;Label->SetFont(Font);Label->SetColorAndOpacity(FSlateColor(FLinearColor(.78f,.77f,.71f,1)));
 auto* TextSlot=RecoveryBox->AddChildToCanvas(Label);TextSlot->SetPosition(FVector2D(0,0));TextSlot->SetSize(FVector2D(80,22));
 RecoveryBar=WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),TEXT("AttackRecoveryProgress"));
 FProgressBarStyle Style;Style.BackgroundImage.SetResourceObject(nullptr);Style.BackgroundImage.DrawAs=ESlateBrushDrawType::Image;Style.BackgroundImage.TintColor=FSlateColor(FLinearColor(.055f,.06f,.065f,.9f));
 Style.FillImage.DrawAs=ESlateBrushDrawType::Image;Style.FillImage.TintColor=FSlateColor(FLinearColor::White);RecoveryBar->SetWidgetStyle(Style);RecoveryBar->SetFillColorAndOpacity(FLinearColor(.8f,.77f,.63f,1));RecoveryBar->SetBorderPadding(FVector2D::ZeroVector);
 auto* BarSlot=RecoveryBox->AddChildToCanvas(RecoveryBar);BarSlot->SetPosition(FVector2D(86,7));BarSlot->SetSize(FVector2D(174,5));
 RecoveryBox->SetVisibility(ESlateVisibility::Collapsed);
}
void UVTGActionHUD::ShowRecovery(float Fraction,float Opacity)
{
 EnsureRecoveryWidgets();if(!RecoveryBox)return;
 RecoveryBar->SetPercent(FMath::Clamp(Fraction,0.f,1.f));RecoveryBox->SetRenderOpacity(Opacity);
 RecoveryBox->SetVisibility(Opacity>0?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
}
void UVTGActionHUD::RecoveryBlendingOut(UAnimMontage* Montage,bool Interrupted)
{
 if(Montage==RecoveryMontage.Get() && Interrupted){if(auto* Anim=RecoveryAnim.Get())if(auto* Active=Anim->GetActiveInstanceForMontage(Montage);Active && Active->IsActive())return;bInterrupted=true;bPendingReady=false;ReadyFlash=0;ShowRecovery(0,0);}
}
void UVTGActionHUD::RecoveryEnded(UAnimMontage* Montage,bool Interrupted)
{
 if(Montage==RecoveryMontage.Get()){if(auto* Anim=RecoveryAnim.Get())if(auto* Active=Anim->GetActiveInstanceForMontage(Montage);Active && Active->IsActive())return;bPendingReady=bRecovering && !Interrupted && !bInterrupted;bRecovering=false;}
}
void UVTGActionHUD::RefreshRecovery(APawn* Pawn,float Dt)
{
 auto Flag=[&](const TCHAR* Name){auto* P=Pawn?FindFProperty<FBoolProperty>(Pawn->GetClass(),Name):nullptr;return P && P->GetPropertyValue_InContainer(Pawn);};
 auto* Character=Cast<ACharacter>(Pawn);auto* Anim=Character?Character->GetMesh()->GetAnimInstance():nullptr;
 if(RecoveryAnim.Get()!=Anim)
 {
  if(auto* Old=RecoveryAnim.Get()){Old->OnMontageEnded.RemoveDynamic(this,&ThisClass::RecoveryEnded);Old->OnMontageBlendingOut.RemoveDynamic(this,&ThisClass::RecoveryBlendingOut);}
  RecoveryAnim=Anim;RecoveryInstanceID=INDEX_NONE;bRecovering=false;bPendingReady=false;ReadyFlash=0;
  if(Anim){Anim->OnMontageEnded.AddUniqueDynamic(this,&ThisClass::RecoveryEnded);Anim->OnMontageBlendingOut.AddUniqueDynamic(this,&ThisClass::RecoveryBlendingOut);}
 }
 const bool Hidden=Health<=0 || !Character || !Anim || !Flag(TEXT("InCombat")) || Flag(TEXT("IsDead?")) || Flag(TEXT("HidePlayerNCam")) || Flag(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)")) || Flag(TEXT("IsDodging?")) || Flag(TEXT("HasWeaponInHand"));
 if(Hidden){bRecovering=false;bPendingReady=false;ReadyFlash=0;ShowRecovery(0,0);return;}
 auto* Montage=RecoveryMontage.LoadSynchronous();if(!Montage){bRecovering=false;bPendingReady=false;ReadyFlash=0;ShowRecovery(0,0);return;}auto* Instance=Anim->GetActiveInstanceForMontage(Montage);
 if(Instance && Instance->GetInstanceID()!=RecoveryInstanceID)
 {
  RecoveryInstanceID=Instance->GetInstanceID();bInterrupted=false;bPendingReady=false;ReadyFlash=0;RecoveryStart=0;
  // The recovery begins after the authored last damage window, not after a hard-coded delay.
  for(const auto& Notify:Montage->Notifies)if(Notify.GetNotifyEventName().ToString().Contains(TEXT("Notify_Damage")))RecoveryStart=FMath::Max(RecoveryStart,Notify.GetEndTriggerTime());
 }
 if(!Instance && RecoveryInstanceID!=INDEX_NONE)Instance=Anim->GetMontageInstanceForID(RecoveryInstanceID);
 const bool Locked=Flag(TEXT("IsAttacking")) && Character->GetCharacterMovement()->MovementMode==MOVE_None;
 auto* Active=Anim->GetCurrentActiveMontage();
 if(Instance && Locked && !bInterrupted && (!Active || Active==Montage))
 {
  const float Position=Instance->GetPosition();
  bRecovering=Position>=RecoveryStart;
  if(bRecovering){const float Fraction=(Position-RecoveryStart)/FMath::Max(.001f,Montage->GetPlayLength()-RecoveryStart);ShowRecovery(FMath::Clamp(Fraction,0.f,.98f),1);return;}
 }
 else bRecovering=false;
 // A full bar is shown only after the existing gameplay lock has actually cleared.
 if(bPendingReady && !Locked && !Flag(TEXT("IsAttacking")) && Character->GetCharacterMovement()->MovementMode!=MOVE_None){ReadyFlash=.22f;bPendingReady=false;}
 if(ReadyFlash>0){ReadyFlash=FMath::Max(0.f,ReadyFlash-Dt);ShowRecovery(1,FMath::Min(1.f,ReadyFlash/.12f));return;}
 ShowRecovery(0,0);
}
void UVTGActionHUD::NativeDestruct()
{
 if(auto* Anim=RecoveryAnim.Get()){Anim->OnMontageEnded.RemoveDynamic(this,&ThisClass::RecoveryEnded);Anim->OnMontageBlendingOut.RemoveDynamic(this,&ThisClass::RecoveryBlendingOut);}
 RecoveryAnim.Reset();Super::NativeDestruct();
}
