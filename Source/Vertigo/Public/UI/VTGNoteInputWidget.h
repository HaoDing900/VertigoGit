#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VTGNoteInputWidget.generated.h"

/** Note root: logical open/close controls input; destruction is a fallback for forced removal. */
UCLASS()
class VERTIGO_API UVTGNoteInputWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    /** Called from the Note component's state-change event, including cached/animated closure. */
    UFUNCTION(BlueprintCallable, Category="Note|Input")
    void SetNoteInputActive(bool bActive);
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
private:
    TWeakObjectPtr<class UNarrativeUIInputComponent> InputOwner;
    TWeakObjectPtr<class UVTGMailReadingComponent> MailReadingOwner;
    FDelegateHandle OpeningFocusHandle;
    void FinishOpeningFocus(float DeltaTime);
    void CancelOpeningFocus();
};
