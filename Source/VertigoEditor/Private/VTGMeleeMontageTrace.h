#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VTGMeleeMontageTrace.generated.h"
class UAnimInstance;
class UAnimMontage;
/** Opt-in, transient PIE diagnostics; never added to a saved character or map. */
UCLASS(Transient)
class UVTGMeleeMontageTrace : public UActorComponent
{
 GENERATED_BODY()
public:
 UVTGMeleeMontageTrace();
 virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* TickFunction) override;
private:
 TWeakObjectPtr<UAnimInstance> Bound;
 UFUNCTION() void Started(UAnimMontage* Montage);
 UFUNCTION() void BlendingOut(UAnimMontage* Montage, bool Interrupted);
 void Report(const TCHAR* Event, UAnimMontage* Montage, bool Interrupted);
};
