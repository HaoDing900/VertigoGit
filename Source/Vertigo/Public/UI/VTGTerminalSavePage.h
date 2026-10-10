#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Save/VTGSaveTypes.h"
#include "VTGTerminalSavePage.generated.h"
class UTextBlock;
class UScrollBox;
class UVTGTerminalSavePage;

UCLASS()
class VERTIGO_API UVTGTerminalSlotButton : public UButton
{
 GENERATED_BODY()
public:
 void Setup(UVTGTerminalSavePage* Page,int32 Index);
private:
 UPROPERTY(Transient) TObjectPtr<UVTGTerminalSavePage> PageOwner;
 int32 SlotIndex=0;
 UFUNCTION() void Select();
};

UCLASS()
class VERTIGO_API UVTGTerminalSavePage : public UUserWidget
{
 GENERATED_BODY()
public:
 void OpenForTerminal(UUserWidget* Terminal,UUserWidget* ScreenSource);
 void SelectSlot(int32 Index);
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
 virtual void NativeDestruct() override;
 UFUNCTION() void SaveClicked();
 UFUNCTION() void LoadClicked();
 UFUNCTION() void CancelClicked();
 UFUNCTION() void CloseClicked();
 UFUNCTION() void TerminalVisibilityChanged(ESlateVisibility InVisibility);
private:
 friend struct FVTGTerminalSaveRegression;
 void RefreshTerminalFrame();
 void RefreshSlots();
 void UpdateSelection();
 void ShowResult(const FText& Message,bool Success);
 UTextBlock* Text(FName Name,const FText& Value,int32 Size);
 UButton* Action(FName Name,const FText& Label);
 UPROPERTY(Transient) TObjectPtr<UButton> SaveButton;
 UPROPERTY(Transient) TObjectPtr<UButton> LoadButton;
 UPROPERTY(Transient) TObjectPtr<UButton> CancelButton;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailText;
 UPROPERTY(Transient) TObjectPtr<UScrollBox> SlotList;
 UPROPERTY(Transient) TArray<TObjectPtr<UVTGTerminalSlotButton>> SlotButtons;
 UPROPERTY(Transient) TWeakObjectPtr<UUserWidget> TerminalOwner;
 TArray<FVTGSlotMeta> Metas;
 int32 SelectedSlot=INDEX_NONE;
 int32 PendingAction=0;
 bool Busy=false;
 UPROPERTY(Transient) FSlateBrush TerminalFrameBrush;
 FAnchorData TerminalFrameLayout;
 bool HasTerminalFrame=false;
 FAnchorData ScreenLayout;
 bool HasScreenLayout=false;
};
