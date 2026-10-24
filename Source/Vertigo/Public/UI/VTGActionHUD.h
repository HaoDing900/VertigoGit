#pragma once
#include "CoreMinimal.h"
#include "Combat/VTGCombatPresentation.h"
#include "VTGActionHUD.generated.h"
class UInputAction;
class UAnimInstance;
class UAnimMontage;
class UCanvasPanel;
class UProgressBar;
UCLASS()
class VERTIGO_API UVTGActionHUD : public UVTGPlayerHearts
{
 GENERATED_BODY()
public:
 UPROPERTY(EditDefaultsOnly, Category="Combat Hints") TObjectPtr<UInputAction> DodgeAction;
 UPROPERTY(EditDefaultsOnly, Category="Combat Hints") TObjectPtr<UInputAction> PunchAction;
 void RefreshHints();
 void RefreshBloodOverlay(bool bInCombat);
 /** Presentation only: reads the authored finisher and existing movement/attack locks. */
 void RefreshRecovery(APawn* Pawn, float DeltaSeconds);
 UPROPERTY(EditDefaultsOnly, Category="Combat Hints")
 TSoftObjectPtr<UAnimMontage> RecoveryMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/Characters/Sa/Anm/FistFight/ANM_Sa_FightFist03_Inplace1_Montage.ANM_Sa_FightFist03_Inplace1_Montage")));
 FKey MappedKeyForAction(const UInputAction* Action) const;
 FText KeyForAction(const UInputAction* Action) const;
protected:
 virtual void NativeDestruct() override;
private:
 friend struct FVTGRecoveryHUDRegression;
 void EnsureRecoveryWidgets();
 void ShowRecovery(float Fraction, float Opacity);
 UFUNCTION() void RecoveryEnded(UAnimMontage* Montage, bool Interrupted);
 UFUNCTION() void RecoveryBlendingOut(UAnimMontage* Montage, bool Interrupted);
 UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RecoveryBox;
 UPROPERTY(Transient) TObjectPtr<UProgressBar> RecoveryBar;
 TWeakObjectPtr<UAnimInstance> RecoveryAnim;
 int32 RecoveryInstanceID = INDEX_NONE;
 float RecoveryStart = 0.f;
 float ReadyFlash = 0.f;
 bool bRecovering = false;
 bool bInterrupted = false;
 bool bPendingReady = false;
};
