#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "VTGProjectionComponent.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UTexture2D;

/**
 * Holographic "projected image" for the IPHA drone (a map, a police badge, any PNG).
 *
 * The component spawns ONE plane mesh at runtime, snaps it to a socket on the drone
 * (reel_ballcamSocket by default), drives a dynamic material instance with whatever texture you
 * hand it, and re-orients the plane at the player camera every frame so the image reads correctly
 * no matter how the drone flies, spins or tumbles.
 *
 * The light cone stays where it belongs - in the drone Blueprint. Point LightCone at it and the
 * component shows/hides and fades it in lockstep with the image; leave it empty and the cone is
 * simply left alone.
 *
 * Setup:
 *   1. Add the component to BP_Drone_IPHA_NPC.
 *   2. Projection Material = a translucent/unlit hologram material (MM_Hologram_Plane_Panning or
 *      MM_VTGHologramBase). It needs a texture param and a scalar opacity param, and their names
 *      must match Texture Parameter Name / Opacity Parameter Name below.
 *   3. Light Cone = the cone mesh component you already added in the Blueprint (optional).
 *   4. Tune Local Offset until the plane sits at the far end of your cone.
 *   5. Call ShowProjection(MyBadgeTexture) / HideProjection() from dialogue, ISX or Sequencer.
 */
UCLASS(ClassGroup = (Vertigo), meta = (BlueprintSpawnableComponent))
class VERTIGO_API UVTGProjectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVTGProjectionComponent();

	// ---------------------------------------------------------------- attachment

	/**
	 * Mesh that owns the socket. Leave EMPTY to auto-use the owning Character's "Mesh"
	 * (the drone skeletal mesh) - which is what you want in BP_Drone_IPHA_NPC.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Attachment",
		meta = (UseComponentPicker, AllowedClasses = "SceneComponent"))
	FComponentReference AttachTarget;

	/** Socket the projector emits from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Attachment")
	FName SocketName = TEXT("reel_ballcamSocket");

	/**
	 * Where the image floats, in SOCKET space (cm). This is the only knob for "how far out in front
	 * of the drone the picture hangs" - slide it along the axis your cone points down.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Attachment")
	FVector LocalOffset = FVector(120.f, 0.f, 0.f);

	// ---------------------------------------------------------------- look

	/** The quad. Defaults to /Engine/BasicShapes/Plane (a 100x100cm plane whose normal is +Z). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Look")
	TObjectPtr<UStaticMesh> PlaneMesh;

	/** Hologram material applied to the plane. A dynamic instance of it is created at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Look")
	TObjectPtr<UMaterialInterface> ProjectionMaterial;

	/** Texture param on ProjectionMaterial that receives the PNG. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Look")
	FName TextureParameterName = TEXT("Texture");

	/** Scalar param on ProjectionMaterial driven 0 -> 1 by the fade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Look")
	FName OpacityParameterName = TEXT("Opacity");

	/** Image to show when nothing is passed to ShowProjection (e.g. the default badge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Look")
	TObjectPtr<UTexture2D> DefaultImage;

	/** Plane size multiplier. The default plane is 100x100cm, so 1.5 = a 150cm image. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Look", meta = (ClampMin = "0.01"))
	float ProjectionScale = 1.5f;

	// ---------------------------------------------------------------- facing

	/**
	 * TRUE  - align the plane to the camera view plane (classic billboard). Never skewed, dead
	 *         stable, and stays readable at the screen edge. Recommended.
	 * FALSE - aim the plane normal straight at the camera location. Slightly more "physical", but
	 *         the image shears when the drone is off to the side of the screen.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Facing")
	bool bScreenAligned = true;

	/**
	 * Keep the image "up" pointing at world up, so text and badges never appear rotated even when
	 * the player camera rolls. Turn OFF to let the image roll with the camera.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Facing")
	bool bKeepUprightInWorld = true;

	/**
	 * Degrees-per-second the plane turns to catch up with the camera. 0 = snap instantly (no lag).
	 * Something like 720 gives a subtle mechanical swivel as the player walks around the drone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Facing", meta = (ClampMin = "0.0"))
	float TurnSpeed = 0.f;

	/**
	 * Escape hatch for UV orientation: if your PNG comes out sideways or upside down on the plane,
	 * put 90 / 180 / 270 here instead of re-authoring the texture or the mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Facing")
	float ImageRollDegrees = 0.f;

	// ---------------------------------------------------------------- fade

	/** Seconds for the image to fade in / out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Fade", meta = (ClampMin = "0.0"))
	float FadeTime = 0.35f;

	/** Scale the image starts at while fading in, so it "pops" open. 1 = no pop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Fade", meta = (ClampMin = "0.01"))
	float PopInScale = 0.7f;

	/** The cone mesh you added in the Blueprint. Optional - faded and hidden alongside the image. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Fade",
		meta = (UseComponentPicker, AllowedClasses = "MeshComponent"))
	FComponentReference LightCone;

	/** Scalar param on the cone material driven by the same fade. Blank = only toggle visibility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projection|Fade")
	FName ConeOpacityParameterName = TEXT("Opacity");

	// ---------------------------------------------------------------- api

	/** Fade the projection in. Pass an image, or leave it empty to reuse DefaultImage / the last one. */
	UFUNCTION(BlueprintCallable, Category = "Projection", meta = (AdvancedDisplay = "1"))
	void ShowProjection(UTexture2D* Image = nullptr);

	/** Fade the projection out. The plane hides itself once it reaches zero. */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void HideProjection();

	/** Swap the PNG without fading - safe to call mid-show to flip between map and badge. */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void SetProjectionImage(UTexture2D* Image);

	/** Resize the image at runtime (multiplier, same meaning as ProjectionScale). */
	UFUNCTION(BlueprintCallable, Category = "Projection")
	void SetProjectionScale(float NewScale);

	/** True while the projection is on or fading in. */
	UFUNCTION(BlueprintPure, Category = "Projection")
	bool IsProjecting() const { return bWantsVisible; }

	/** The runtime plane, in case you want to drive extra material params from Blueprint. */
	UFUNCTION(BlueprintPure, Category = "Projection")
	UStaticMeshComponent* GetProjectionPlane() const { return Plane; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Spawns + attaches the plane and builds its dynamic material. */
	void BuildPlane();

	/** Resolves AttachTarget, falling back to the owning Character's skeletal mesh. */
	USceneComponent* ResolveAttachTarget() const;

	/** Grabs (and caches) a MID for the cone so we can fade it. Returns null if there is no cone. */
	UMaterialInstanceDynamic* ResolveConeMID();

	/** Turns the plane to face the player camera this frame. */
	void UpdateFacing(float DeltaTime);

	/** Pushes Alpha into opacity + pop-scale on the plane and the cone. */
	void ApplyAlpha();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Plane;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlaneMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ConeMID;

	/** Whether the last request was Show (true) or Hide (false). */
	bool bWantsVisible = false;

	/** Current fade value, 0 = fully hidden, 1 = fully shown. */
	float Alpha = 0.f;

	/** Set once the cone MID lookup has run, so a coneless setup does not retry every frame. */
	bool bConeResolved = false;
};
