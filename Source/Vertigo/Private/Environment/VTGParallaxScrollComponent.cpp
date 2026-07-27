#include "Environment/VTGParallaxScrollComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	/** fmod that always returns a value in [0, Range), unlike FMath::Fmod which keeps the sign. */
	float PositiveFmod(float Value, float Range)
	{
		if (Range <= KINDA_SMALL_NUMBER)
		{
			return 0.f;
		}
		const float R = FMath::Fmod(Value, Range);
		return R < 0.f ? R + Range : R;
	}
}

UVTGParallaxScrollComponent::UVTGParallaxScrollComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVTGParallaxScrollComponent::BeginPlay()
{
	Super::BeginPlay();

	BaseSpeeds.Reset();
	for (FVTGScrollLayer& Layer : Layers)
	{
		InitLayer(Layer);
		BaseSpeeds.Add(Layer.Speed);
	}
}

void UVTGParallaxScrollComponent::InitLayer(FVTGScrollLayer& Layer)
{
	// Skip any empty/None or invalid slot - a hole in the array won't break the conveyor.
	Layer.Groups.RemoveAll([](const TObjectPtr<AActor>& A) { return !IsValid(A); });

	Layer.DirUnit = Layer.Direction.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0.f, -1.f, 0.f));
	Layer.Accum = 0.f;

	// Only group A filled (B was None)? Auto-duplicate A so a 2-tile loop still works.
	if (Layer.Groups.Num() == 1)
	{
		if (AActor* Clone = DuplicateGroup(Layer.Groups[0]))
		{
			Layer.Groups.Add(Clone);
		}
	}

	if (Layer.Groups.Num() < 2)
	{
		// Nothing to leapfrog against - leave it inert rather than teleporting a lone actor.
		Layer.ResolvedTile = 0.f;
		Layer.Loop = 0.f;
		return;
	}

	// Anchor = the FIRST group's placed position. That's the layer's start point; the rest is derived.
	const FVector Anchor = Layer.Groups[0]->GetActorLocation();
	Layer.AnchorAlong = FVector::DotProduct(Anchor, Layer.DirUnit);
	Layer.AnchorPerp = Anchor - Layer.DirUnit * Layer.AnchorAlong;   // strip the along-axis component

	// TileWidth = spacing between groups. Manual if set, else the anchor mesh's own width along Dir.
	if (Layer.TileWidth > KINDA_SMALL_NUMBER)
	{
		Layer.ResolvedTile = Layer.TileWidth;
	}
	else
	{
		FVector BoundsOrigin, BoundsExtent;
		Layer.Groups[0]->GetActorBounds(/*bOnlyCollidingComponents=*/false, BoundsOrigin, BoundsExtent);
		// Full width along the scroll axis = 2 x the extent projected onto Dir.
		Layer.ResolvedTile = 2.f * FMath::Abs(FVector::DotProduct(BoundsExtent, Layer.DirUnit.GetAbs()));
	}

	Layer.Loop = Layer.ResolvedTile * Layer.Groups.Num();
	// Window is one tile ahead of the anchor back to a full loop behind it, so the wrap seam sits
	// off the front edge (place the anchor near where content should exit).
	Layer.WindowStart = Layer.AnchorAlong + Layer.ResolvedTile - Layer.Loop;
}

void UVTGParallaxScrollComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bScrolling || DeltaTime <= 0.f)
	{
		return;
	}

	for (FVTGScrollLayer& Layer : Layers)
	{
		TickLayer(Layer, DeltaTime);
	}
}

void UVTGParallaxScrollComponent::TickLayer(FVTGScrollLayer& Layer, float DeltaTime)
{
	if (Layer.Loop <= KINDA_SMALL_NUMBER || Layer.Groups.Num() < 2)
	{
		return;
	}

	Layer.Accum += Layer.Speed * DeltaTime;

	for (int32 i = 0; i < Layer.Groups.Num(); ++i)
	{
		AActor* G = Layer.Groups[i];
		if (!G)
		{
			continue;
		}

		// Group i starts one TileWidth behind the previous, then everything slides by Accum...
		const float Along = Layer.AnchorAlong - i * Layer.ResolvedTile + Layer.Accum;
		// ...folded into the wrap window - the fold IS the teleport, and even spacing is guaranteed.
		const float Folded = Layer.WindowStart + PositiveFmod(Along - Layer.WindowStart, Layer.Loop);

		// Perpendicular (depth/height) comes from the anchor; only the along-axis moves.
		const FVector NewLoc = Layer.AnchorPerp + Layer.DirUnit * Folded;
		G->SetActorLocation(NewLoc);
	}
}

AActor* UVTGParallaxScrollComponent::DuplicateGroup(AActor* Source) const
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(Source))
	{
		return nullptr;
	}

	// Template spawn copies the source actor's properties (mesh, materials, scale, mobility, ...).
	// The clone is repositioned by TickLayer immediately, so the spawn transform is just a starting point.
	FActorSpawnParameters SpawnParams;
	SpawnParams.Template = Source;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags = RF_Transient;   // don't try to save the runtime copy

	return World->SpawnActor<AActor>(Source->GetClass(), Source->GetActorTransform(), SpawnParams);
}

void UVTGParallaxScrollComponent::SetLayerSpeed(int32 LayerIndex, float NewSpeed)
{
	if (Layers.IsValidIndex(LayerIndex))
	{
		Layers[LayerIndex].Speed = NewSpeed;
	}
}

void UVTGParallaxScrollComponent::SetGlobalSpeedScale(float Scale)
{
	for (int32 i = 0; i < Layers.Num(); ++i)
	{
		if (BaseSpeeds.IsValidIndex(i))
		{
			Layers[i].Speed = BaseSpeeds[i] * Scale;
		}
	}
}
