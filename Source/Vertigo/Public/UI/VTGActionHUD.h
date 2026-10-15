#pragma once
#include "CoreMinimal.h"
#include "Combat/VTGCombatPresentation.h"
#include "VTGActionHUD.generated.h"
class UInputAction;
UCLASS()
class VERTIGO_API UVTGActionHUD : public UVTGPlayerHearts
{
 GENERATED_BODY()
public:
 UPROPERTY(EditDefaultsOnly, Category="Combat Hints") TObjectPtr<UInputAction> DodgeAction;
 UPROPERTY(EditDefaultsOnly, Category="Combat Hints") TObjectPtr<UInputAction> PunchAction;
 void RefreshHints();
 void RefreshBloodOverlay(bool bInCombat);
 FKey MappedKeyForAction(const UInputAction* Action) const;
 FText KeyForAction(const UInputAction* Action) const;
};
