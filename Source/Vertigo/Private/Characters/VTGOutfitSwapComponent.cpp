#include "Characters/VTGOutfitSwapComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

UVTGOutfitSwapComponent::UVTGOutfitSwapComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Fallback only - per-slot materials are captured from the outline component at BeginPlay.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OutlineMatFinder(
		TEXT("/Game/Characters/VTGCharacterShader/Outline/MM_Outline_01a_size15.MM_Outline_01a_size15"));
	if (OutlineMatFinder.Succeeded())
	{
		DefaultOutlineMaterial = OutlineMatFinder.Object;
	}
}

void UVTGOutfitSwapComponent::BeginPlay()
{
	Super::BeginPlay();

	// If not configured by hand, snapshot the outline component's editor-set per-slot materials
	// (size15 face, size35/70 elsewhere...) so we can restore them slot-by-slot after each swap.
	if (OutlineMaterials.Num() == 0)
	{
		if (USkeletalMeshComponent* Outline = ResolveOutline())
		{
			const int32 NumSlots = Outline->GetNumMaterials();
			OutlineMaterials.Reset(NumSlots);
			for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
			{
				OutlineMaterials.Add(Outline->GetMaterial(SlotIndex));
			}
		}
	}
}

void UVTGOutfitSwapComponent::SwapOutfit(USkeletalMesh* NewBody)
{
	USkeletalMeshComponent* Body = ResolveBody();
	USkeletalMeshComponent* Outline = ResolveOutline();

	auto Report = [](const FString& Msg, const FColor& Color)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OutfitSwap] %s"), *Msg);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 6.f, Color, FString::Printf(TEXT("[OutfitSwap] %s"), *Msg));
		}
	};

	Report(FString::Printf(TEXT("NewBody=%s | Body=%s | Outline=%s"),
		*GetNameSafe(NewBody), *GetNameSafe(Body), *GetNameSafe(Outline)), FColor::Cyan);

	if (!NewBody)
	{
		Report(TEXT("ABORT: 'New Body' pin is empty - pick a skeletal mesh on the SwapOutfit node."), FColor::Red);
		return;
	}

	if (Body)
	{
		Body->SetSkeletalMeshAsset(NewBody);
		// No per-component overrides: let the new asset's own material slots show through.
		Body->EmptyOverrideMaterials();
	}
	else
	{
		Report(TEXT("ABORT: Body mesh not resolved - set 'Body Mesh' on the component."), FColor::Red);
	}

	if (Outline)
	{
		Outline->SetSkeletalMeshAsset(NewBody);

		if (bRebindLeaderPose)
		{
			// Re-bind to the body so the outline keeps copying its pose after the mesh swap.
			Outline->SetLeaderPoseComponent(Body);
		}

		RefreshOutlineMaterials();
	}
	else
	{
		Report(TEXT("WARNING: Outline mesh not resolved - set 'Outline Mesh' on the component."), FColor::Yellow);
	}
}

void UVTGOutfitSwapComponent::RefreshOutlineMaterials()
{
	USkeletalMeshComponent* Outline = ResolveOutline();
	if (!Outline)
	{
		return;
	}

	const int32 NumSlots = Outline->GetNumMaterials();
	for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
	{
		// Keep each slot's own outline material (thickness varies per body region); only fall
		// back to the default for slots a new costume added beyond the captured list.
		UMaterialInterface* SlotMaterial = OutlineMaterials.IsValidIndex(SlotIndex)
			? OutlineMaterials[SlotIndex].Get()
			: nullptr;
		if (!SlotMaterial)
		{
			SlotMaterial = DefaultOutlineMaterial;
		}
		if (SlotMaterial)
		{
			Outline->SetMaterial(SlotIndex, SlotMaterial);
		}
	}
}

USkeletalMeshComponent* UVTGOutfitSwapComponent::ResolveBody() const
{
	// Prefer the explicit picker; fall back to the Character's main mesh if it wasn't bound.
	if (USkeletalMeshComponent* Picked = Cast<USkeletalMeshComponent>(BodyMesh.GetComponent(GetOwner())))
	{
		return Picked;
	}
	if (const ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		return OwnerChar->GetMesh();
	}
	return nullptr;
}

USkeletalMeshComponent* UVTGOutfitSwapComponent::ResolveOutline() const
{
	// Prefer the explicit picker; fall back to a skeletal mesh component named "Outline" that
	// isn't the body, so the swap still works if the picker wasn't bound.
	if (USkeletalMeshComponent* Picked = Cast<USkeletalMeshComponent>(OutlineMesh.GetComponent(GetOwner())))
	{
		return Picked;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	const USkeletalMeshComponent* Body = nullptr;
	if (const ACharacter* OwnerChar = Cast<ACharacter>(Owner))
	{
		Body = OwnerChar->GetMesh();
	}

	TArray<USkeletalMeshComponent*> SkeletalComps;
	Owner->GetComponents<USkeletalMeshComponent>(SkeletalComps);
	for (USkeletalMeshComponent* Comp : SkeletalComps)
	{
		if (Comp && Comp != Body && Comp->GetName().Contains(TEXT("Outline")))
		{
			return Comp;
		}
	}
	return nullptr;
}
