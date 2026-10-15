#include "Combat/VTGCombatPresentation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "UI/VTGActionHUD.h"
#include "Save/VTGSaveCoordinator.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/WeaponBase_ISX.h"
#include "Objects/Item.h"

namespace VTGPresentation
{
void Burst(UObject *Context, UNiagaraSystem *System, FVector Location, FRotator Rotation, float Seconds)
{
    if (!System || !Context || !Context->GetWorld() || Context->GetWorld()->GetNetMode() == NM_DedicatedServer)
        return;
    if (auto *FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Context, System, Location, Rotation, FVector(1), true,
                                                                  true, ENCPoolMethod::None, true))
    {
        FTimerHandle Handle;
        Context->GetWorld()->GetTimerManager().SetTimer(
            Handle, FTimerDelegate::CreateWeakLambda(FX, [FX]() { FX->Deactivate(); }), FMath::Max(.05f, Seconds),
            false);
        // A second bound limits even systems whose emitters do not complete after deactivation.
        FTimerHandle Cleanup;
        Context->GetWorld()->GetTimerManager().SetTimer(
            Cleanup, FTimerDelegate::CreateWeakLambda(FX, [FX]() { FX->DestroyComponent(); }),
            FMath::Max(.05f, Seconds) + 8.f, false);
    }
}
float Number(UObject *O, FName Name, float Fallback)
{
    auto *P = FindFProperty<FNumericProperty>(O->GetClass(), Name);
    if (!P)
        return Fallback;
    auto *V = P->ContainerPtrToValuePtr<void>(O);
    return P->IsInteger() ? float(P->GetSignedIntPropertyValue(V)) : float(P->GetFloatingPointPropertyValue(V));
}
bool Flag(UObject *O, FName Name)
{
    auto *P = FindFProperty<FBoolProperty>(O->GetClass(), Name);
    return P && P->GetPropertyValue_InContainer(O);
}
} // namespace VTGPresentation
UVTGEnemyFeedbackComponent::UVTGEnemyFeedbackComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}
void UVTGEnemyFeedbackComponent::BeginPlay()
{
    Super::BeginPlay();
    if (Source.Find(GetOwner(), TEXT("Health"), TEXT("MaxHealth")))
        LastHealth = Source.GetHealth();
    GetOwner()->OnDestroyed.AddDynamic(this, &ThisClass::OwnerDestroyed);
}
void UVTGEnemyFeedbackComponent::CheckHealth()
{
    if (!Source.IsValid())
        return;
    const float H = Source.GetHealth();
    if (H > 0 && LastHealth <= 0)
        bDeathPlayed = false;
    if (H <= 0 && !bDeathPlayed && LastHealth > 0)
    {
        bDeathPlayed = true;
        if (bEndsSewerCombatOnDeath)
            if(auto* Player=UGameplayStatics::GetPlayerPawn(this,0))
                if(auto* Presentation=Player->FindComponentByClass<UVTGPlayerPresentationComponent>()) Presentation->FinishSewerCombat();
        VTGPresentation::Burst(this, DeathEffect, GetOwner()->GetActorLocation(), FRotator::ZeroRotator, 2.f);
    }
    else if (H > 0 && H < LastHealth)
        VTGPresentation::Burst(this, HitEffect, GetOwner()->GetActorLocation() + FVector(0, 0, 35),
                               FRotator::ZeroRotator, .3f);
    LastHealth = H;
}
void UVTGEnemyFeedbackComponent::TickComponent(float Dt, ELevelTick T, FActorComponentTickFunction *Fn)
{
    Super::TickComponent(Dt, T, Fn);
    CheckHealth();
}
void UVTGEnemyFeedbackComponent::OwnerDestroyed(AActor *)
{
    CheckHealth();
}
void UVTGAttackEffectNotify::Notify(USkeletalMeshComponent *Mesh, UAnimSequenceBase *Animation,
                                    const FAnimNotifyEventReference &Ref)
{
    Super::Notify(Mesh, Animation, Ref);
    if (!Mesh || !Mesh->GetOwner() || !Mesh->GetWorld()->IsGameWorld())
        return;
    const float Scale = Mesh->GetOwner()->GetActorScale3D().GetAbsMax();
    auto *Selected = Scale >= 1.8f && LargeEffect    ? LargeEffect.Get()
                     : Scale >= 1.2f && MediumEffect ? MediumEffect.Get()
                                                     : Effect.Get();
    VTGPresentation::Burst(Mesh, Selected,
                           SocketName.IsNone() ? Mesh->GetOwner()->GetActorLocation()
                                               : Mesh->GetSocketLocation(SocketName),
                           Mesh->GetOwner()->GetActorRotation(), ActiveSeconds);
}
int32 UVTGPlayerHearts::NativePaint(const FPaintArgs &Args, const FGeometry &Geo, const FSlateRect &Cull,
                                    FSlateWindowElementList &Out, int32 Layer, const FWidgetStyle &Style,
                                    bool Enabled) const
{
    Layer = Super::NativePaint(Args, Geo, Cull, Out, Layer, Style, Enabled);
    const int32 Count = 8;
    const float Units = MaxHealth / Count;
    const FVector2D Size(44, 44);
    for (int32 I = 0; I < Count; ++I)
    {
        const FVector2D Position(40 + I * 48, 40);
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.SetResourceObject(Empty);
        FSlateDrawElement::MakeBox(Out, Layer + 1, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), &Brush,
                                   ESlateDrawEffect::None);
        const float Fill = Units > 0 ? FMath::Clamp((Health - I * Units) / Units, 0.f, 1.f) : 0;
        if (Fill > 0)
        {
            Brush.SetResourceObject(HitFlash > 0 && Hurt ? Hurt.Get() : Full.Get());
            Out.PushClip(FSlateClippingZone(
                Geo.ToPaintGeometry(FVector2D(Size.X * Fill, Size.Y), FSlateLayoutTransform(Position))));
            FSlateDrawElement::MakeBox(Out, Layer + 2, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(Position)),
                                       &Brush, ESlateDrawEffect::None);
            Out.PopClip();
        }
    }
    if (HitFlash > 0)
    {
        const auto *Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
        FSlateDrawElement::MakeBox(Out, Layer + 3, Geo.ToPaintGeometry(), Brush, ESlateDrawEffect::None,
                                   FLinearColor(.65f, .01f, .025f, FMath::Min(.13f, HitFlash * .35f)));
    }
    return Layer + 3;
}
UVTGPlayerPresentationComponent::UVTGPlayerPresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}
void UVTGPlayerPresentationComponent::TickComponent(float Dt, ELevelTick T, FActorComponentTickFunction *Fn)
{
    Super::TickComponent(Dt, T, Fn);
    auto *Pawn = Cast<APawn>(GetOwner());
    auto *PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!PC || !PC->IsLocalController())
        return;
    ConsumeBufferedAttack();
    RefreshBrassKnuckles();
    if (!Hearts)
    {
        Hearts = CreateWidget<UVTGPlayerHearts>(PC, ActionHUDClass ? ActionHUDClass.Get() : UVTGPlayerHearts::StaticClass());
        if (!Hearts)
            return;
        Hearts->Full = HeartFull;
        Hearts->Empty = HeartEmpty;
        Hearts->Hurt = HeartHurt;
        Hearts->SetVisibility(ESlateVisibility::Collapsed);
        Hearts->AddToViewport(ActionHUDZOrder);
    }
    for (UActorComponent* C : Pawn->GetComponents())
    {
        if (auto* Bar = Cast<UWidgetComponent>(C); Bar && Bar->GetFName() == TEXT("HealthBar") && Bar->IsVisible())
            Bar->SetVisibility(false);
    }
    const float H = VTGPresentation::Number(Pawn, TEXT("Health"), 100);
    Hearts->Health = H;
    Hearts->MaxHealth = VTGPresentation::Number(Pawn, TEXT("MaxHealth_Normal"), 100);
    Hearts->HitFlash = H < LastHealth ? .35f : FMath::Max(0.f, Hearts->HitFlash - Dt);
    LastHealth = H;
    const bool Hidden = !VTGPresentation::Flag(Pawn, TEXT("InCombat")) ||
                        VTGPresentation::Flag(Pawn, TEXT("HidePlayerNCam"));
    Hearts->SetVisibility(Hidden ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    if (auto* Action=Cast<UVTGActionHUD>(Hearts)) { Action->RefreshBloodOverlay(!Hidden && !VTGPresentation::Flag(Pawn,TEXT("IsDead?"))); if(!Hidden)Action->RefreshHints(); }
}
bool UVTGPlayerPresentationComponent::ResolveCombatState(bool EnemyNearby,bool SewerLevel) const
{
 auto* Pawn=GetOwner();if(!Pawn)return false;
 auto* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr;
 auto* Save=GI?GI->GetSubsystem<UVTGSaveCoordinator>():nullptr;
 if(VTGPresentation::Number(Pawn,TEXT("Health"),100)<=0 || VTGPresentation::Flag(Pawn,TEXT("IsDead?")) || VTGPresentation::Flag(Pawn,TEXT("IsInCineCutscene_LockInput(NotWorkForMont)")) || (SewerLevel && Save && Save->GetPersistentInt(TEXT("L3SewerBossDefeated"),0)))return false;
 return EnemyNearby || SewerLevel;
}
void UVTGPlayerPresentationComponent::FinishSewerCombat()
{
 if(auto* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr)
  if(auto* Save=GI->GetSubsystem<UVTGSaveCoordinator>())Save->SetPersistentInt(TEXT("L3SewerBossDefeated"),1);
 for(const auto& Pair:{TPair<FName,bool>(TEXT("InCombat"),false),TPair<FName,bool>(TEXT("StoryMode?"),true),TPair<FName,bool>(TEXT("GunOrFistEquipped"),false)})
  if(auto* P=FindFProperty<FBoolProperty>(GetOwner()->GetClass(),Pair.Key))P->SetPropertyValue_InContainer(GetOwner(),Pair.Value);
 RefreshBrassKnuckles();
 if(auto* Action=Cast<UVTGActionHUD>(Hearts)){Action->RefreshBloodOverlay(false);Action->SetVisibility(ESlateVisibility::Collapsed);}
}
void UVTGPlayerPresentationComponent::RefreshBrassKnuckles()
{
 auto* Pawn=GetOwner();if(!Pawn)return;
 bool Armed=VTGPresentation::Flag(Pawn,TEXT("HasWeaponInHand")) || VTGPresentation::Number(Pawn,TEXT("CurrentMeleeWeapon"),0)!=0;
 // WeaponBase_ISX owns the actual ranged equipment; the legacy hand flag is not updated by every inventory path.
 if(auto* Weapon=Pawn->FindComponentByClass<UWeaponBase_ISX>())Armed|=IsValid(Weapon->GetEquippedItem());
 const bool Show=VTGPresentation::Flag(Pawn,TEXT("InCombat")) && !Armed && VTGPresentation::Number(Pawn,TEXT("Health"),100)>0 && !VTGPresentation::Flag(Pawn,TEXT("HidePlayerNCam"));
 for(UActorComponent* C:Pawn->GetComponents())
  if(auto* Mesh=Cast<UStaticMeshComponent>(C);Mesh && (Mesh->GetName().Contains(TEXT("BrassKnuckle")) || (Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetName().Contains(TEXT("BrassKnuckle")))))
  {Mesh->SetHiddenInGame(!Show);Mesh->SetVisibility(Show);}
}
void UVTGPlayerPresentationComponent::ConsumeBufferedAttack()
{
    auto *Pawn = GetOwner();
    if (!Pawn)
        return;
    for (UActorComponent* C : Pawn->GetComponents())
    {
        if (auto* Bar = Cast<UWidgetComponent>(C); Bar && Bar->GetFName() == TEXT("HealthBar") && Bar->IsVisible())
            Bar->SetVisibility(false);
    }
    const float H = VTGPresentation::Number(Pawn, TEXT("Health"), 100);
    // Consume the existing one-press buffer only when the authored combo window permits it.
    if (H > 0 && !VTGPresentation::Flag(Pawn, TEXT("IsDead?")) &&
        !VTGPresentation::Flag(Pawn, TEXT("IsInCineCutscene_LockInput(NotWorkForMont)")) &&
        VTGPresentation::Flag(Pawn, TEXT("InCombat")) && VTGPresentation::Flag(Pawn, TEXT("IsAttacking")) &&
        VTGPresentation::Flag(Pawn, TEXT("IsInComboWindow")) && VTGPresentation::Flag(Pawn, TEXT("SaveAttack")))
    {
        if (auto *Saved = FindFProperty<FBoolProperty>(Pawn->GetClass(), TEXT("SaveAttack")))
            Saved->SetPropertyValue_InContainer(Pawn, false);
        if (auto *Punch = Pawn->FindFunction(TEXT("Punch")); Punch && Punch->ParmsSize == 0)
            Pawn->ProcessEvent(Punch, nullptr);
    }
}
void UVTGPlayerPresentationComponent::EndPlay(EEndPlayReason::Type Reason)
{
    if (Hearts)
    {
        Hearts->RemoveFromParent();
        Hearts = nullptr;
    }
    Super::EndPlay(Reason);
}
