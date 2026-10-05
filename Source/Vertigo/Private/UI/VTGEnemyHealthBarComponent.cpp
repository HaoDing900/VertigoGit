#include "UI/VTGEnemyHealthBarComponent.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"

UVTGEnemyHealthBarComponent::UVTGEnemyHealthBarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Same look as the human enemies' HealthBar component.
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawSize(FVector2D(150.f, 12.f));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	CanCharacterStepUpOn = ECB_No;
	SetHiddenInGame(true);
}

void UVTGEnemyHealthBarComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!Source.Find(GetOwner(), HealthProperty, MaxHealthProperty))
	{
		UE_LOG(LogTemp, Warning, TEXT("[VTGEnemyHealthBar] %s: no component exposes %s / %s - no health bar."),
			*GetNameSafe(GetOwner()), *HealthProperty.ToString(), *MaxHealthProperty.ToString());
		SetComponentTickEnabled(false);
		return;
	}

	if (bPlaceAboveOwner)
	{
		PlaceAboveOwner();
	}
	SetHiddenInGame(true);
}

void UVTGEnemyHealthBarComponent::PlaceAboveOwner()
{
	// Colliding components only: effects and widgets would make the bounds much taller than the body.
	FVector Origin, Extent;
	GetOwner()->GetActorBounds(/*bOnlyCollidingComponents=*/true, Origin, Extent);
	const float Top = Origin.Z + Extent.Z + HeightAboveOwner;
	SetWorldLocation(FVector(GetOwner()->GetActorLocation().X, GetOwner()->GetActorLocation().Y, Top));
}

void UVTGEnemyHealthBarComponent::PushHealth(float Health, float MaxHealth)
{
	UUserWidget* BarWidget = GetUserWidgetObject();
	UFunction* Event = BarWidget ? BarWidget->FindFunction(UpdateEventName) : nullptr;
	if (!Event)
	{
		return;
	}

	// Fill the event's two numeric parameters (float or double, whatever the widget declares).
	TArray<uint8, TInlineAllocator<64>> Params;
	Params.SetNumZeroed(Event->ParmsSize);
	int32 Index = 0;
	for (TFieldIterator<FNumericProperty> It(Event); It && It->HasAnyPropertyFlags(CPF_Parm) && Index < 2; ++It, ++Index)
	{
		It->SetFloatingPointPropertyValue(It->ContainerPtrToValuePtr<void>(Params.GetData()), Index == 0 ? Health : MaxHealth);
	}
	BarWidget->ProcessEvent(Event, Params.GetData());
}

void UVTGEnemyHealthBarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Source.IsValid())
	{
		return;
	}

	const float Health = FMath::Max(Source.GetHealth(), 0.f);
	const float MaxHealth = Source.GetMaxHealth();
	if (Health != LastHealth)
	{
		LastHealth = Health;
		PushHealth(Health, MaxHealth);
	}

	const bool bDead = Health <= 0.f;
	DeadFor = bDead ? FMath::Max(DeadFor, 0.f) + DeltaTime : -1.f;
	const bool bShow = bDead ? DeadFor < HideDelayAfterDeath : Health < MaxHealth;
	if (bShow == bHiddenInGame)
	{
		SetHiddenInGame(!bShow);
	}
}
