#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NarrativeUIInputComponent.generated.h"

class APlayerController;
class UUserWidget;
class SWidget;

/** Shared ownership of local UI input. Releasing one dialogue must not unlock another modal. */
UCLASS()
class NARRATIVE_API UNarrativeUIInputComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    static UNarrativeUIInputComponent* Find(APlayerController* Controller);
    static UNarrativeUIInputComponent* GetOrCreate(APlayerController* Controller);

    void Acquire(UObject* Owner, UUserWidget* FocusWidget = nullptr, int32 Priority = 0);
    void Release(UObject* Owner);
    void Refresh();
    UFUNCTION(BlueprintPure, Category="Narrative|Input")
    bool HasOwners() const;
    /** Read-only diagnostic: identifies the modal currently holding player input. */
    UFUNCTION(BlueprintPure, Category="Narrative|Input")
    UObject* GetActiveOwner() const;

protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    struct FRequest
    {
        TWeakObjectPtr<UObject> Owner;
        TWeakObjectPtr<UUserWidget> Widget;
        TWeakPtr<SWidget> SavedFocus;
        int32 Priority = 0;
        uint64 Order = 0;
    };
    TArray<FRequest> Requests;
    uint64 NextOrder = 0;
    bool bLockedInput = false;
    FRequest* GetActiveRequest();
};
