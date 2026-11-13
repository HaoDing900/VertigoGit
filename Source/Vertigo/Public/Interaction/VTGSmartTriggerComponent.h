#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VTGSmartTriggerComponent.generated.h"
class UBoxComponent;

/** Per-volume activation state; independent of the Blueprint's lifetime DoOnce gate. */
UCLASS()
class VERTIGO_API UVTGSmartTriggerComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UVTGSmartTriggerComponent();
    UFUNCTION(BlueprintCallable, Category="Smart Trigger") static void InitializeTrigger(AActor* Trigger);
    UFUNCTION(BlueprintCallable, Category="Smart Trigger") static bool AcceptTrigger(AActor* Trigger,AActor* Player,bool FromInteraction);
    UFUNCTION(BlueprintCallable, Category="Smart Trigger") static void FinishTriggerDispatch(AActor* Trigger);
    // For asynchronous non-dialogue events, the level manager can keep the trigger busy explicitly.
    UFUNCTION(BlueprintCallable, Category="Smart Trigger") static void SetTriggerBusy(AActor* Trigger,bool Busy);
    UFUNCTION(BlueprintCallable, Category="Smart Trigger") static void SetAllowDuringDialogue(AActor* Trigger,bool Allow);
    UFUNCTION(BlueprintPure, Category="Smart Trigger") static bool IsManagedInteractionProxy(AActor* Actor);
    UFUNCTION(BlueprintPure, Category="Smart Trigger") static bool IsInteractionProxyVisible(AActor* Actor,AActor* Player,FVector CameraLocation);
    bool IsPlayerInside(AActor* Player) const;
    void Refresh();
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    static UVTGSmartTriggerComponent* Get(AActor* Trigger);
    bool CanAccept(AActor* Player) const;
    void SetPromptEnabled(bool Enabled);
    UFUNCTION() void ProxyInteracted();
    UFUNCTION() void OwnerDestroyed(AActor* Actor);
    UFUNCTION() void VolumeExited(UPrimitiveComponent* Component,AActor* OtherActor,UPrimitiveComponent* OtherComponent,int32 BodyIndex);
    UPROPERTY() TSoftClassPtr<AActor> ProxyClass;
    UPROPERTY(Transient) TObjectPtr<AActor> Proxy;
    TWeakObjectPtr<AActor> Occupant;
    TWeakObjectPtr<UBoxComponent> Volume;
    FTimerHandle RefreshTimer;
    bool bSpent=false,bEverFired=false,bDispatching=false,bExplicitBusy=false,bPromptEnabled=false;
    bool bAllowDuringDialogue=false;
};
