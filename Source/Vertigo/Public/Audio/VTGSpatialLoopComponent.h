#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGSpatialLoopComponent.generated.h"
class USoundWave;
class USoundCue;
class USoundAttenuation;
class UAudioComponent;
class UNarrativeComponent;
class UDialogue;

UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent))
class VERTIGO_API UVTGSpatialLoopComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UVTGSpatialLoopComponent();
    UPROPERTY(EditAnywhere, Category="Spatial Loop") TObjectPtr<USoundWave> Sound;
    UPROPERTY(EditAnywhere, Category="Spatial Loop") TObjectPtr<USoundAttenuation> Attenuation;
    UPROPERTY(EditAnywhere, Category="Spatial Loop") bool bLoopEnabled=true;
    UPROPERTY(EditAnywhere, Category="Spatial Loop") bool bReactToMovement=false;
    UPROPERTY(EditAnywhere, Category="Spatial Loop", meta=(ClampMin="0")) float IdleVolume=.08f;
    UPROPERTY(EditAnywhere, Category="Spatial Loop", meta=(ClampMin="0")) float MovingVolume=.13f;
    UPROPERTY(EditAnywhere, Category="Spatial Loop", meta=(ClampMin="0.1")) float IdlePitch=.95f;
    UPROPERTY(EditAnywhere, Category="Spatial Loop", meta=(ClampMin="0.1")) float MovingPitch=1.3f;
    UPROPERTY(EditAnywhere, Category="Spatial Loop", meta=(ClampMin="1")) float FullSpeed=300.f;
    UPROPERTY(EditAnywhere, Category="Spatial Loop", meta=(ClampMin="0")) float MovementDeadZone=10.f;
    UPROPERTY(EditAnywhere, Category="Spatial Loop", meta=(ClampMin="0.1")) float ResponseSpeed=4.f;
    void SetLoopEnabled(bool Enabled);
    void SuspendForDialogue(UNarrativeComponent* Narrative);
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
    static float MovementAmount(float Speed,float DeadZone,float MaxSpeed);
private:
    friend bool VTGVerifySpatialAudio();
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Audio;
    UPROPERTY(Transient) TObjectPtr<USoundCue> LoopCue;
    TWeakObjectPtr<UNarrativeComponent> BoundNarrative;
    TWeakObjectPtr<UDialogue> MutedDialogue;
    FVector LastLocation=FVector::ZeroVector;
    float MovementBlend=0.f;
    void RefreshPlayback();
    void UpdateMovement(float Speed,float DeltaTime);
    UFUNCTION() void DialogueEnded(UDialogue* Dialogue,bool StartingNewDialogue);
};

UCLASS()
class VERTIGO_API UVTGSpatialAudioLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Audio|Bello")
    static void SetBelloRepairLoop(AActor* Bello,bool Helped,USoundWave* RepairSound,USoundAttenuation* Attenuation);
    UFUNCTION(BlueprintCallable,Category="Audio|Bello")
    static void SuspendBelloRepairForDialogue(AActor* Bello,UNarrativeComponent* Narrative);
};
