#include "UI/VTGSaveIndicator.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/CircularThrobber.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"

TSharedRef<SWidget> UVTGSaveIndicator::RebuildWidget()
{
 if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
 if (!WidgetTree->RootWidget)
 {
  auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
  WidgetTree->RootWidget = Canvas;
  Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);

  auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SaveIndicatorRow"));
  auto* RowSlot = Canvas->AddChildToCanvas(Row);
  // Bottom right, inset from the corner by the same margin the rest of the HUD uses.
  RowSlot->SetAnchors(FAnchors(1.f, 1.f));
  RowSlot->SetAlignment(FVector2D(1.f, 1.f));
  RowSlot->SetPosition(FVector2D(-48.f, -48.f));
  RowSlot->SetAutoSize(true);

  Spinner = WidgetTree->ConstructWidget<UCircularThrobber>(UCircularThrobber::StaticClass(), TEXT("SaveIndicatorSpinner"));
  Spinner->SetRadius(14.f);
  Row->AddChildToHorizontalBox(Spinner)->SetPadding(FMargin(0, 0, 10, 0));

  Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SaveIndicatorLabel"));
  Label->SetText(NSLOCTEXT("VTGTerminal", "Saving", "SAVING"));
  FSlateFontInfo Font = Label->GetFont();
  Font.Size = 17;
  Font.TypefaceFontName = NAME_None;
  Font.FontObject = LoadObject<UFont>(nullptr, TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Medium_Font"));
  Label->SetFont(Font);
  Label->SetColorAndOpacity(FSlateColor(FLinearColor(.95f, .94f, .90f)));
  Row->AddChildToHorizontalBox(Label);
 }
 return Super::RebuildWidget();
}

void UVTGSaveIndicator::Begin()
{
 Elapsed = 0.f;
 bFinished = false;
 SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UVTGSaveIndicator::Finish()
{
 bFinished = true;
}

void UVTGSaveIndicator::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
 Super::NativeTick(Geometry, DeltaTime);
 if (GetVisibility() == ESlateVisibility::Collapsed) return;
 Elapsed += DeltaTime;
 // The write blocks the game thread, so DeltaTime on the frame after it is large - which is fine, it
 // means the stall itself counts towards the time the player had the spinner on screen.
 if (bFinished && Elapsed >= MinimumVisibleTime)
 {
  SetVisibility(ESlateVisibility::Collapsed);
  RemoveFromParent();
 }
}
