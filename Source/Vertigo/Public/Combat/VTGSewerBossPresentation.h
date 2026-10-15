#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/ActorComponent.h"
#include "VTGSewerBossPresentation.generated.h"

/** Only installed on the hanging L3 boss. Keep the placed body fixed while the upper spine aims. */
UCLASS(ClassGroup=(Vertigo),meta=(BlueprintSpawnableComponent))
class VERTIGO_API UVTGAnchoredBossComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVTGAnchoredBossComponent();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Hanging Boss") bool bAnchorEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Hanging Boss") float MaxYaw=80.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Hanging Boss") float MaxPitch=35.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Hanging Boss") float AimSpeed=5.f;
 UPROPERTY(BlueprintReadOnly,Transient,Category="Hanging Boss") FRotator UpperBodyWorldRotation;
 virtual void BeginPlay() override;
 virtual void TickComponent(float Dt,ELevelTick T,FActorComponentTickFunction* Fn) override;
private:
 FTransform Anchor;
};

UCLASS(Transient,Blueprintable)
class VERTIGO_API UVTGAnchoredBossAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 UPROPERTY(BlueprintReadOnly,Transient,Category="Hanging Boss") FRotator UpperBodyWorldRotation;
 virtual void NativeUpdateAnimation(float Dt) override;
};

/** End the authored egg death animation by handing its evaluated, scaled pose to Chaos. */
UCLASS()
class VERTIGO_API UVTGEggRagdollNotify : public UAnimNotify
{
 GENERATED_BODY()
public:
 virtual void Notify(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Animation,const FAnimNotifyEventReference& Ref) override;
};

/** Flight lifetime and facing are shared by collision and the attached local-space acid effect. */
UCLASS(ClassGroup=(Vertigo),meta=(BlueprintSpawnableComponent))
class VERTIGO_API UVTGAcidFlightComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVTGAcidFlightComponent();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Acid Flight",meta=(ClampMin="1")) float MaxTravelDistance=1800.f;
 virtual void BeginPlay() override;
 virtual void TickComponent(float Dt,ELevelTick T,FActorComponentTickFunction* Fn) override;
private:
 FVector LastPosition;
 float Travelled=0.f;
};
