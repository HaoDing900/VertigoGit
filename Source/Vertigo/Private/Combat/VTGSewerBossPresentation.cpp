#include "Combat/VTGSewerBossPresentation.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/UnrealType.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"

namespace SewerPresentation
{
UObject* Target(UObject* Object)
{
 if(!Object)return nullptr;
 if(auto* P=FindFProperty<FObjectPropertyBase>(Object->GetClass(),TEXT("TargetActor")))return P->GetObjectPropertyValue_InContainer(Object);
 return nullptr;
}
}
UVTGAnchoredBossComponent::UVTGAnchoredBossComponent(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PrePhysics;}
void UVTGAnchoredBossComponent::BeginPlay()
{
 Super::BeginPlay();Anchor=GetOwner()->GetActorTransform();AddTickPrerequisiteActor(GetOwner());
 for(auto* C:GetOwner()->GetComponents())if(C!=this && C->GetClass()->FindPropertyByName(TEXT("AbilityRotationRate")))AddTickPrerequisiteComponent(C);
 if(auto* Ch=Cast<ACharacter>(GetOwner()))Ch->GetMesh()->AddTickPrerequisiteComponent(this);
}
void UVTGAnchoredBossComponent::TickComponent(float Dt,ELevelTick T,FActorComponentTickFunction* Fn)
{
 Super::TickComponent(Dt,T,Fn);if(!bAnchorEnabled)return;
 auto* Owner=GetOwner();AActor* Target=nullptr;bool Alive=true;
 for(auto* C:Owner->GetComponents())
 {
  if(auto* P=FindFProperty<FBoolProperty>(C->GetClass(),TEXT("IsAlive")))Alive=P->GetPropertyValue_InContainer(C);
  if(auto* A=Cast<AActor>(SewerPresentation::Target(C)))Target=A;
  for(const TCHAR* Name:{TEXT("InitialRotationRate"),TEXT("CombatRotationRate"),TEXT("AbilityRotationRate")})if(auto* P=FindFProperty<FNumericProperty>(C->GetClass(),Name))P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(C),0);
 }
 if(auto* Ch=Cast<ACharacter>(Owner))
 {
  Ch->bUseControllerRotationYaw=false;Ch->bUseControllerRotationPitch=false;Ch->bUseControllerRotationRoll=false;
  auto* Move=Ch->GetCharacterMovement();Move->StopMovementImmediately();Move->DisableMovement();Move->bOrientRotationToMovement=false;Move->bUseControllerDesiredRotation=false;
 }
 Owner->SetActorLocationAndRotation(Anchor.GetLocation(),Anchor.GetRotation(),false,nullptr,ETeleportType::TeleportPhysics);
 FRotator Desired=FRotator::ZeroRotator;
 if(Target && Alive)
 {
  FVector From=Owner->GetActorLocation();if(auto* Ch=Cast<ACharacter>(Owner))From=Ch->GetMesh()->GetSocketLocation(TEXT("spine_001"));
  const FRotator Look=(Target->GetActorLocation()-From).Rotation();
  const float Yaw=Anchor.Rotator().Yaw+FMath::Clamp(FMath::FindDeltaAngleDegrees(Anchor.Rotator().Yaw,Look.Yaw),-MaxYaw,MaxYaw);
  const float Pitch=FMath::Clamp(Look.Pitch,-MaxPitch,MaxPitch);
  Desired=(FRotator(Pitch,Yaw,Anchor.Rotator().Roll).Quaternion()*Anchor.GetRotation().Inverse()).Rotator();
 }
 UpperBodyWorldRotation=FMath::RInterpTo(UpperBodyWorldRotation,Desired,Dt,AimSpeed);
}
void UVTGAnchoredBossAnimInstance::NativeUpdateAnimation(float Dt)
{
 Super::NativeUpdateAnimation(Dt);UpperBodyWorldRotation=FRotator::ZeroRotator;
 if(auto* Owner=GetOwningActor())if(auto* C=Owner->FindComponentByClass<UVTGAnchoredBossComponent>();C && C->bAnchorEnabled)UpperBodyWorldRotation=C->UpperBodyWorldRotation;
}
void UVTGEggRagdollNotify::Notify(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Animation,const FAnimNotifyEventReference& Ref)
{
 Super::Notify(Mesh,Animation,Ref);if(!Mesh || !Mesh->GetWorld() || !Mesh->GetWorld()->IsGameWorld())return;
 // The notify is dispatched after pose evaluation. Use those bone transforms, including animation scale.
 for(auto* Body:Mesh->Bodies)if(Body)
 {
  if(!Body->BodySetup.IsValid())continue;
  const int32 Index=Mesh->GetBoneIndex(Body->BodySetup.Get()->BoneName);if(Index==INDEX_NONE)continue;
  const auto Bone=Mesh->GetBoneTransform(Index);Body->UpdateBodyScale(Bone.GetScale3D(),true);Body->SetBodyTransform(Bone,ETeleportType::TeleportPhysics);
 }
 for(auto* C:Mesh->GetOwner()->GetComponents())if(auto* Event=C->FindFunction(TEXT("OnRagdoll"));Event && Event->ParmsSize==0){C->ProcessEvent(Event,nullptr);return;}
 Mesh->SetCollisionProfileName(TEXT("Ragdoll"));Mesh->SetSimulatePhysics(true);
}
UVTGAcidFlightComponent::UVTGAcidFlightComponent(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostPhysics;}
void UVTGAcidFlightComponent::BeginPlay()
{
 Super::BeginPlay();LastPosition=LaunchPosition=GetOwner()->GetActorLocation();
 PlayerTarget=UGameplayStatics::GetPlayerPawn(this,0);if(PlayerTarget.IsValid())PlayerTarget->OnTakeAnyDamage.AddDynamic(this,&UVTGAcidFlightComponent::PlayerDamaged);
 if(auto* Move=GetOwner()->FindComponentByClass<UProjectileMovementComponent>())
 {
  if(!Move->UpdatedComponent)Move->SetUpdatedComponent(GetOwner()->GetRootComponent());
  Move->bRotationFollowsVelocity=true;Move->ProjectileGravityScale=0;
  // The toolkit supplies the target and origin. Reuse its full 3D launch direction.
  if(!Move->Velocity.IsNearlyZero())GetOwner()->SetActorRotation(Move->Velocity.Rotation());
  AddTickPrerequisiteComponent(Move);
 }
}
void UVTGAcidFlightComponent::TickComponent(float Dt,ELevelTick T,FActorComponentTickFunction* Fn)
{
 Super::TickComponent(Dt,T,Fn);EmitMuzzle();const FVector Now=GetOwner()->GetActorLocation();Travelled+=FVector::Distance(LastPosition,Now);LastPosition=Now;
 if(Travelled>=MaxTravelDistance){bRangeExpired=true;GetOwner()->Destroy();}
}
void UVTGAcidFlightComponent::EmitMuzzle()
{
 if(bMuzzlePlayed || !MuzzleEffect)return;auto* Move=GetOwner()->FindComponentByClass<UProjectileMovementComponent>();if(!Move || Move->Velocity.IsNearlyZero())return;
 bMuzzlePlayed=true;const FQuat Aim=FQuat::FindBetweenNormals(FVector::UpVector,Move->Velocity.GetSafeNormal());
 MuzzleInstance=UNiagaraFunctionLibrary::SpawnSystemAtLocation(this,MuzzleEffect,LaunchPosition,Aim.Rotator(),FVector(MuzzleScale),true,true,ENCPoolMethod::None,true);
 if(MuzzleInstance){TWeakObjectPtr<UNiagaraComponent> FX=MuzzleInstance;FTimerHandle Cleanup;GetWorld()->GetTimerManager().SetTimer(Cleanup,FTimerDelegate::CreateLambda([FX](){if(FX.IsValid())FX->DestroyComponent();}),.6f,false);}
}
void UVTGAcidFlightComponent::PlayerDamaged(AActor* Actor,float Damage,const UDamageType* Type,AController* Controller,AActor* Causer)
{
 if(Damage<=0 || Causer!=GetOwner() || bImpactPlayed || !ImpactEffect)return;bImpactPlayed=true;EmitMuzzle();
 UNiagaraFunctionLibrary::SpawnSystemAtLocation(Actor,ImpactEffect,Actor->GetActorLocation()+FVector(0,0,25),FRotator::ZeroRotator,FVector(ImpactScale));
}
void UVTGAcidFlightComponent::EndPlay(const EEndPlayReason::Type Reason){if(PlayerTarget.IsValid())PlayerTarget->OnTakeAnyDamage.RemoveDynamic(this,&UVTGAcidFlightComponent::PlayerDamaged);Super::EndPlay(Reason);}

