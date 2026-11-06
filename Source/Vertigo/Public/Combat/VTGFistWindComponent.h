#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGFistWindComponent.generated.h"
class UNiagaraSystem;
class UNiagaraComponent;
class UAnimMontage;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(ClassGroup=(Combat),meta=(BlueprintSpawnableComponent))
class VERTIGO_API UVTGFistWindComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UVTGFistWindComponent();
    UPROPERTY(EditAnywhere,Category="Fist Wind") TObjectPtr<UNiagaraSystem> WindSystem;
    UPROPERTY(EditAnywhere,Category="Fist Wind") TArray<TObjectPtr<UAnimMontage>> PunchMontages;
    UPROPERTY(EditAnywhere,Category="Fist Wind",meta=(ClampMin="0.01")) float WindScale=1.f;
    UPROPERTY(EditAnywhere,Category="Hammer Wind") TObjectPtr<UNiagaraSystem> HammerWindSystem;
    UPROPERTY(EditAnywhere,Category="Hammer Wind") TObjectPtr<UStaticMesh> HammerMesh;
    UPROPERTY(EditAnywhere,Category="Hammer Wind") TArray<TObjectPtr<UAnimMontage>> HammerMontages;
    UPROPERTY(EditAnywhere,Category="Hammer Wind") FVector HammerWindOffset=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,Category="Hammer Wind") FRotator HammerWindRotation=FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere,Category="Hammer Wind",meta=(ClampMin="0.01")) float HammerWindScale=1.35f;
    UPROPERTY(EditAnywhere,Category="Recovery",meta=(ClampMin="0.1")) float RecoveryDelay=30.f;
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
private:
    friend bool VTGVerifyFistWind();
    friend bool VTGVerifyEnhancedWind();
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> Winds;
    UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> HammerWind;
    FTimerHandle RecoveryTimer;
    bool WasInCombat=false;
    bool WantsWind() const;
    bool WantsHammerWind() const;
    UStaticMeshComponent* FindHammerMesh() const;
    void SetHammerWindActive(bool Active);
    void UpdateCombatState(bool InCombat);
    void SetWindActive(bool Active);
    void RecoverHealth();
};

UCLASS()
class VERTIGO_API UVTGCombatBoundaryLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Combat|Boundary") static void SetBoundaryStyleVisible(AActor* Boundary,bool Visible);
};
