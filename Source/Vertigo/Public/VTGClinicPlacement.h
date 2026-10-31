#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VTGClinicPlacement.generated.h"
class ACharacter;
class APlayerController;
class UNarrativeComponent;
class UDialogue;

/** Owns only the short placement-to-dialogue input lease. */
UCLASS()
class VERTIGO_API UVTGClinicPlacementState : public UActorComponent
{
 GENERATED_BODY()
public:
 void Place(ACharacter* Player,AActor* FacingTarget,const FVector& Location,float FallbackYaw);
 void Release();
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 bool bHolding=false;
 bool bOrientToMovement=false;
 bool bControllerDesired=false;
 bool bControllerYaw=false;
 bool bControllerPitch=false;
 bool bControllerRoll=false;
 TWeakObjectPtr<ACharacter> Character;
 TWeakObjectPtr<APlayerController> Controller;
 TWeakObjectPtr<UNarrativeComponent> Narrative;
 FTimerHandle Deadline;
 FTimerHandle Handoff;
 UFUNCTION() void DialogueBegan(UDialogue* Dialogue);
};

UCLASS()
class VERTIGO_API UVTGClinicPlacementLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 /** Stop pending movement, face the placed NPC, and hold input until the next blocking dialogue owns it (3s fail-safe). */
 UFUNCTION(BlueprintCallable,Category="Clinic|Staging")
 static void PlaceClinicPlayer(ACharacter* Player,AActor* FacingTarget,FVector Location,float FallbackYaw=183.f);
};
