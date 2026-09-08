#include "Characters/VTGProjectionComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

UVTGProjectionComponent::UVTGProjectionComponent()
{
	// Only ticks while the projection is on screen - see ShowProjection/HideProjection.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// The engine plane is a 100x100cm quad in XY whose normal is +Z. Everything below assumes that.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultPlane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (DefaultPlane.Succeeded())
	{
		PlaneMesh = DefaultPlane.Object;
	}
}

void UVTGProjectionComponent::BeginPlay()
{
	Super::BeginPlay();

	BuildPlane();

	// Start fully off; ShowProjection is what turns everything on.
	Alpha = 0.f;
	bWantsVisible = false;
	if (Plane)
	{
		Plane->SetVisibility(false, /*bPropagateToChildren=*/true);
	}
	if (UMeshComponent* Cone = Cast<UMeshComponent>(LightCone.GetComponent(GetOwner())))
	{
		Cone->SetVisibility(false, /*bPropagateToChildren=*/true);
	}
}

void UVTGProjectionComponent::BuildPlane()
{
	if (Plane)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	USceneComponent* Target = ResolveAttachTarget();
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("VTGProjectionComponent on %s: no attach target, projection disabled."),
			*Owner->GetName());
		return;
	}

	if (SocketName != NAME_None && !Target->DoesSocketExist(SocketName))
	{
		// Not fatal - it just attaches at the component origin - but it is always a typo, so shout.
		UE_LOG(LogTemp, Warning, TEXT("VTGProjectionComponent on %s: socket '%s' not found on %s."),
			*Owner->GetName(), *SocketName.ToString(), *Target->GetName());
	}

	Plane = NewObject<UStaticMeshComponent>(Owner, TEXT("VTGProjectionPlane"));
	Plane->SetStaticMesh(PlaneMesh);
	Plane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Plane->SetCastShadow(false);
	Plane->bReceivesDecals = false;
	Plane->SetHiddenInGame(false);
	Plane->RegisterComponent();
	Plane->AttachToComponent(Target, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	Plane->SetRelativeLocation(LocalOffset);

	if (ProjectionMaterial)
	{
		PlaneMID = Plane->CreateDynamicMaterialInstance(0, ProjectionMaterial);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("VTGProjectionComponent on %s: no Projection Material set."), *Owner->GetName());
	}

	if (DefaultImage)
	{
		SetProjectionImage(DefaultImage);
	}
}

USceneComponent* UVTGProjectionComponent::ResolveAttachTarget() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (USceneComponent* Picked = Cast<USceneComponent>(AttachTarget.GetComponent(Owner)))
	{
		return Picked;
	}

	// Nothing picked: the drone's skeletal mesh is what owns reel_ballcamSocket.
	if (const ACharacter* AsCharacter = Cast<ACharacter>(Owner))
	{
		if (USceneComponent* Mesh = AsCharacter->GetMesh())
		{
			return Mesh;
		}
	}

	return Owner->GetRootComponent();
}

UMaterialInstanceDynamic* UVTGProjectionComponent::ResolveConeMID()
{
	if (bConeResolved)
	{
		return ConeMID;
	}
	bConeResolved = true;

	if (UMeshComponent* Cone = Cast<UMeshComponent>(LightCone.GetComponent(GetOwner())))
	{
		ConeMID = Cone->CreateDynamicMaterialInstance(0);
	}
	return ConeMID;
}

// ---------------------------------------------------------------------------- api

void UVTGProjectionComponent::ShowProjection(UTexture2D* Image)
{
	if (!Plane)
	{
		BuildPlane();
		if (!Plane)
		{
			return;
		}
	}

	if (Image)
	{
		SetProjectionImage(Image);
	}

	bWantsVisible = true;
	Plane->SetVisibility(true, /*bPropagateToChildren=*/true);
	if (UMeshComponent* Cone = Cast<UMeshComponent>(LightCone.GetComponent(GetOwner())))
	{
		Cone->SetVisibility(true, /*bPropagateToChildren=*/true);
	}

	if (FadeTime <= KINDA_SMALL_NUMBER)
	{
		Alpha = 1.f;
	}

	// Face the player immediately so the first visible frame is already correct, not mid-swivel.
	UpdateFacing(/*DeltaTime=*/0.f);
	ApplyAlpha();
	SetComponentTickEnabled(true);
}

void UVTGProjectionComponent::HideProjection()
{
	bWantsVisible = false;

	if (FadeTime <= KINDA_SMALL_NUMBER)
	{
		Alpha = 0.f;
		ApplyAlpha();
		if (Plane)
		{
			Plane->SetVisibility(false, /*bPropagateToChildren=*/true);
		}
		if (UMeshComponent* Cone = Cast<UMeshComponent>(LightCone.GetComponent(GetOwner())))
		{
			Cone->SetVisibility(false, /*bPropagateToChildren=*/true);
		}
		SetComponentTickEnabled(false);
		return;
	}

	// Keep ticking so the fade-out can run; the tick turns itself off when it lands on 0.
	SetComponentTickEnabled(true);
}

void UVTGProjectionComponent::SetProjectionImage(UTexture2D* Image)
{
	if (PlaneMID && Image && TextureParameterName != NAME_None)
	{
		PlaneMID->SetTextureParameterValue(TextureParameterName, Image);
	}
}

void UVTGProjectionComponent::SetProjectionScale(float NewScale)
{
	ProjectionScale = FMath::Max(NewScale, 0.01f);
	ApplyAlpha();
}

// ---------------------------------------------------------------------------- tick

void UVTGProjectionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Plane)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const float Target = bWantsVisible ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(Alpha, Target))
	{
		const float Step = (FadeTime > KINDA_SMALL_NUMBER) ? (DeltaTime / FadeTime) : 1.f;
		Alpha = FMath::FInterpConstantTo(Alpha, Target, 1.f, Step);
	}

	UpdateFacing(DeltaTime);
	ApplyAlpha();

	// Fully faded out: park the plane and stop paying for the tick until the next ShowProjection.
	if (!bWantsVisible && Alpha <= KINDA_SMALL_NUMBER)
	{
		Plane->SetVisibility(false, /*bPropagateToChildren=*/true);
		if (UMeshComponent* Cone = Cast<UMeshComponent>(LightCone.GetComponent(GetOwner())))
		{
			Cone->SetVisibility(false, /*bPropagateToChildren=*/true);
		}
		SetComponentTickEnabled(false);
	}
}

void UVTGProjectionComponent::UpdateFacing(float DeltaTime)
{
	if (!Plane)
	{
		return;
	}

	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam)
	{
		return;
	}

	const FRotator CamRot = Cam->GetCameraRotation();
	const FMatrix CamBasis = FRotationMatrix(CamRot);
	const FVector CamForward = CamBasis.GetUnitAxis(EAxis::X);
	const FVector CamUp = CamBasis.GetUnitAxis(EAxis::Z);

	// The plane's normal is its local +Z, so that is the axis we aim at the viewer.
	//   screen-aligned : normal = the reverse of where the camera looks -> parallel to the view plane
	//   point-at       : normal = straight back at the camera's location
	FVector Normal;
	if (bScreenAligned)
	{
		Normal = -CamForward;
	}
	else
	{
		Normal = (Cam->GetCameraLocation() - Plane->GetComponentLocation()).GetSafeNormal(KINDA_SMALL_NUMBER, -CamForward);
	}

	// Which direction should read as "up" in the image.
	FVector UpRef = bKeepUprightInWorld ? FVector::UpVector : CamUp;
	// Degenerate when the viewer is directly above/below a world-upright image - fall back to the
	// camera's own up, which can never be parallel to the normal.
	if (FMath::Abs(FVector::DotProduct(UpRef, Normal)) > 0.999f)
	{
		UpRef = CamUp;
	}

	// MakeFromZX pins local +Z to the normal and orthogonalises local +X against UpRef, so the
	// quad's X axis ends up pointing "up" on screen. ImageRollDegrees then spins it about the
	// normal to correct however the plane's UVs happen to be laid out.
	FQuat Facing = FRotationMatrix::MakeFromZX(Normal, UpRef).ToQuat();
	if (!FMath::IsNearlyZero(ImageRollDegrees))
	{
		Facing = FQuat(Normal, FMath::DegreesToRadians(ImageRollDegrees)) * Facing;
	}

	if (TurnSpeed > KINDA_SMALL_NUMBER && DeltaTime > 0.f)
	{
		const FQuat Current = Plane->GetComponentQuat();
		const float MaxStep = FMath::DegreesToRadians(TurnSpeed * DeltaTime);
		const float Angle = Current.AngularDistance(Facing);
		if (Angle > MaxStep)
		{
			Facing = FQuat::Slerp(Current, Facing, MaxStep / Angle).GetNormalized();
		}
	}

	Plane->SetWorldRotation(Facing);
}

void UVTGProjectionComponent::ApplyAlpha()
{
	if (!Plane)
	{
		return;
	}

	if (PlaneMID && OpacityParameterName != NAME_None)
	{
		PlaneMID->SetScalarParameterValue(OpacityParameterName, Alpha);
	}

	if (UMaterialInstanceDynamic* Cone = ResolveConeMID())
	{
		if (ConeOpacityParameterName != NAME_None)
		{
			Cone->SetScalarParameterValue(ConeOpacityParameterName, Alpha);
		}
	}

	// Pop open on the way in, shrink back on the way out. Eased so it settles instead of snapping.
	const float Pop = FMath::InterpEaseOut(PopInScale, 1.f, FMath::Clamp(Alpha, 0.f, 1.f), 2.f);
	Plane->SetWorldScale3D(FVector(ProjectionScale * Pop));
}
