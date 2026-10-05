#include "UI/VTGBossHealthBarComponent.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "UI/VTGBossHealthBar.h"

UVTGBossHealthBarComponent::UVTGBossHealthBarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f; // the bar animates from the same values every frame
}

void UVTGBossHealthBarComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!FindHealthSource())
	{
		UE_LOG(LogTemp, Warning, TEXT("[VTGBossHealthBar] %s: no component exposes %s / %s / %s - no health bar."),
			*GetNameSafe(GetOwner()), *HealthProperty.ToString(), *MaxHealthProperty.ToString(), *InCombatProperty.ToString());
		SetComponentTickEnabled(false);
		return;
	}
	if (!BarClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VTGBossHealthBar] %s: Bar Class is not set."), *GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
	}
}

void UVTGBossHealthBarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Bar)
	{
		Bar->RemoveFromParent();
		Bar = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool UVTGBossHealthBarComponent::FindHealthSource()
{
	for (UActorComponent* Component : GetOwner()->GetComponents())
	{
		UClass* Class = Component->GetClass();
		FNumericProperty* H = FindFProperty<FNumericProperty>(Class, HealthProperty);
		FNumericProperty* M = FindFProperty<FNumericProperty>(Class, MaxHealthProperty);
		FBoolProperty* C = FindFProperty<FBoolProperty>(Class, InCombatProperty);
		if (H && M && C)
		{
			Source = Component;
			HealthProp = H;
			MaxHealthProp = M;
			InCombatProp = C;
			return true;
		}
	}
	return false;
}

void UVTGBossHealthBarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsValid(Source))
	{
		return;
	}

	const float Health = HealthProp->GetFloatingPointPropertyValue(HealthProp->ContainerPtrToValuePtr<void>(Source));
	const float MaxHealth = MaxHealthProp->GetFloatingPointPropertyValue(MaxHealthProp->ContainerPtrToValuePtr<void>(Source));
	const bool bInCombat = InCombatProp->GetPropertyValue_InContainer(Source);
	const bool bDead = Health <= 0.f;

	if (!Bar)
	{
		if (!bInCombat || bDead)
		{
			return; // nothing to show yet
		}
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		Bar = PC ? CreateWidget<UVTGBossHealthBar>(PC, BarClass) : nullptr;
		if (!Bar)
		{
			return;
		}
		Bar->SetBossName(DisplayName);
		Bar->SetHealth(Health, MaxHealth);
		Bar->AddToViewport(ViewportZOrder);
	}

	Bar->SetHealth(Health, MaxHealth);

	// Death flips InCombat off too, so look at health first: a kill keeps the empty bar up a moment.
	DeadFor = bDead ? FMath::Max(DeadFor, 0.f) + DeltaTime : -1.f;
	Bar->SetShown(bDead ? DeadFor < HideDelayAfterDeath : bInCombat);
}
