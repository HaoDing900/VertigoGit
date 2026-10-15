#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VTGEnemyIdentityComponent.generated.h"
/** Supplies the editable EnemyDisplayName on the owner to its existing overhead health widget. */
UCLASS(ClassGroup=(Vertigo),meta=(BlueprintSpawnableComponent))
class VERTIGO_API UVTGEnemyIdentityComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVTGEnemyIdentityComponent();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Enemy UI") FText FallbackName;
 void RefreshName();
protected:
 virtual void BeginPlay() override;
 virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Fn) override;
};
