#include "UI/VTGSlideshow.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Components/SceneComponent.h"

FWidgetTransform FVTGSlideMotion::Evaluate(float Age, FVector2D ImageSize, FVector2D CanvasSize) const
{
 float T = Age / FMath::Max(Duration, 0.01f);
 T = bLoopPingPong ? 1.f - FMath::Abs(FMath::Fmod(T, 2.f) - 1.f) : FMath::Clamp(T, 0.f, 1.f);
 if (bEaseInOut) T = T * T * (3.f - 2.f * T);
 FVector2D Offset = FMath::Lerp(StartOffset, EndOffset, T);
 float Shake = FMath::Max(0.f, ShakeAmplitude);
 if (ShakeDuration > 0) Shake *= 1.f - FMath::Clamp(Age / ShakeDuration, 0.f, 1.f);
 const float Phase = Age * FMath::Max(0.f, ShakeFrequency) * 2.f * PI;
 Offset += FVector2D(FMath::Sin(Phase), FMath::Sin(Phase * 1.37f)) * Shake;
 FWidgetTransform Transform;
 Transform.Translation = Offset;
 Transform.Scale = FMath::Lerp(StartScale, EndScale, T);
 Transform.Angle = FMath::Lerp(StartAngle, EndAngle, T);
 if (ImageFit != EVTGSlideFit::Stretch && ImageSize.X > 0 && ImageSize.Y > 0 && CanvasSize.X > 0 && CanvasSize.Y > 0)
 {
  const FVector2D Ratios = CanvasSize / ImageSize;
  const double Fit = ImageFit == EVTGSlideFit::Cover ? FMath::Max(Ratios.X, Ratios.Y) : FMath::Min(Ratios.X, Ratios.Y);
  Transform.Scale *= ImageSize * Fit / CanvasSize;
 }
 if (bPanTopToBottom && CanvasSize.Y > 0)
 {
  const double Overflow = FMath::Max(0.0, CanvasSize.Y * (Transform.Scale.Y - 1.0));
  Transform.Translation.Y += FMath::Lerp(Overflow * .5, -Overflow * .5, static_cast<double>(T));
 }
 return Transform;
}
void UVTGSlideshowWidget::NativeOnInitialized()
{
 Super::NativeOnInitialized();
 auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
 // Keep the slideshow on a fixed logical canvas. The whole frame is uniformly
 // scaled into the viewport, independently of its aspect ratio or DPI.
 auto* Outer = WidgetTree->ConstructWidget<UCanvasPanel>();
 WidgetTree->RootWidget = Outer;
 auto AddOuter = [Outer](UWidget* Child)
 {
  auto* Slot = Outer->AddChildToCanvas(Child);
  Slot->SetAnchors(FAnchors(0, 0, 1, 1));
  Slot->SetOffsets(FMargin(0));
 };
 auto* Letterbox = WidgetTree->ConstructWidget<UImage>();
 Letterbox->SetColorAndOpacity(FLinearColor::Black);
 AddOuter(Letterbox);
 auto* FitFrame = WidgetTree->ConstructWidget<UScaleBox>();
 FitFrame->SetStretch(EStretch::ScaleToFit);
 FitFrame->SetStretchDirection(EStretchDirection::Both);
 AddOuter(FitFrame);
 auto* ReferenceFrame = WidgetTree->ConstructWidget<USizeBox>();
 ReferenceFrame->SetWidthOverride(1920.f);
 ReferenceFrame->SetHeightOverride(1080.f);
 ReferenceFrame->AddChild(Canvas);
 FitFrame->AddChild(ReferenceFrame);
 Canvas->SetClipping(EWidgetClipping::ClipToBoundsAlways);
 SetVisibility(ESlateVisibility::HitTestInvisible);
 auto AddFullScreen = [Canvas](UWidget* Child)
 {
  auto* Slot = Canvas->AddChildToCanvas(Child);
  Slot->SetAnchors(FAnchors(0, 0, 1, 1));
  Slot->SetOffsets(FMargin(0));
 };
 auto* Black = WidgetTree->ConstructWidget<UImage>();
 Black->SetColorAndOpacity(FLinearColor::Black);
 AddFullScreen(Black);
 for (int32 i = 0; i < 2; ++i)
 {
  Images[i] = WidgetTree->ConstructWidget<UImage>();
  Images[i]->SetRenderTransformPivot(FVector2D(0.5, 0.5));
  Images[i]->SetRenderOpacity(0);
  AddFullScreen(Images[i]);
 }
}

void UVTGSlideshowWidget::Show(const FVTGSlide& Slide)
{
 bReturningToGame = false;
 SetRenderOpacity(1.f);
 // Resolve interrupted fades to the most recent requested slide before switching.
 if (Previous >= 0) Images[Previous]->SetRenderOpacity(0);
 Previous = Active;
 Active = Active == 0 ? 1 : 0;
 Images[Active]->SetBrushFromTexture(Slide.Image, false);
 Images[Active]->SetColorAndOpacity(Slide.Image ? FLinearColor::White : FLinearColor::Transparent);
 CastChecked<UCanvasPanelSlot>(Images[Active]->Slot)->SetZOrder(2);
 if (Previous >= 0) CastChecked<UCanvasPanelSlot>(Images[Previous]->Slot)->SetZOrder(1);
 Motions[Active] = Slide.Motion;
 Ages[Active] = 0;
 Moving[Active] = true;
 FadeAge = 0;
 FadeDuration = FMath::Max(0.f, Slide.FadeSeconds);
 Images[Active]->SetRenderOpacity(FadeDuration > 0 ? 0 : 1);
 if (Previous >= 0) Images[Previous]->SetRenderOpacity(FadeDuration > 0 ? 1 : 0);
 NativeTick(GetCachedGeometry(), 0.f);
}

void UVTGSlideshowWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
 Super::NativeTick(Geometry, DeltaTime);
 if (Active < 0) return;
 if (bReturningToGame)
 {
  ReturnAge += DeltaTime;
  SetRenderOpacity(ReturnDuration <= 0.f ? 0.f : 1.f - FMath::Clamp(ReturnAge / ReturnDuration, 0.f, 1.f));
 }
 FadeAge += DeltaTime;
 const float Fade = FadeDuration <= 0 ? 1 : FMath::Clamp(FadeAge / FadeDuration, 0.f, 1.f);
 Images[Active]->SetRenderOpacity(Fade);
 // Fade both layers to black when the requested slide is blank.
 const bool bBlack = !Images[Active]->GetBrush().GetResourceObject();
 if (Previous >= 0)
 {
  Images[Previous]->SetRenderOpacity(bBlack ? 1 - Fade : (Fade < 1 ? 1 : 0));
  if (Fade >= 1) Previous = -1;
 }
 for (int32 i = 0; i < 2; ++i)
 {
  if (!Moving[i] || (i != Active && i != Previous)) continue;
  Ages[i] += DeltaTime;
  const UTexture2D* Texture = Cast<UTexture2D>(Images[i]->GetBrush().GetResourceObject());
  const FVector2D Size = Texture ? FVector2D(Texture->GetSizeX(),Texture->GetSizeY()) : FVector2D(1920,1080);
  Images[i]->SetRenderTransform(Motions[i].Evaluate(Ages[i], Size, FVector2D(1920,1080)));
 }
}
void UVTGSlideshowWidget::ChangeMotion(const FVTGSlideMotion& Motion)
{
 if (Active < 0) return;
 Motions[Active] = Motion;
 Ages[Active] = 0;
 Moving[Active] = true;
 NativeTick(GetCachedGeometry(), 0);
}
void UVTGSlideshowWidget::StopMotion()
{
 for (bool& bMoving : Moving) bMoving = false;
}

void UVTGSlideshowWidget::FadeBackToGame(float Duration)
{
 bReturningToGame = true;
 ReturnAge = 0.f;
 ReturnDuration = FMath::Max(0.f, Duration);
 SetRenderOpacity(ReturnDuration <= 0.f ? 0.f : 1.f);
}

AVTGSlideshowDirector::AVTGSlideshowDirector()
{
 PrimaryActorTick.bCanEverTick = false;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
}
int32 AVTGSlideshowDirector::GetImageIndex(int32 i) const
{
 return Slides[i].ImageIndex > 0 ? Slides[i].ImageIndex : i + 1;
}
void AVTGSlideshowDirector::AssignAutomaticImageIndices()
{
 TSet<int32> Used;
 for (const auto& Slide : Slides) if (Slide.ImageIndex > 0) Used.Add(Slide.ImageIndex);
 for (int32 i = 0; i < Slides.Num(); ++i)
 {
  if (Slides[i].ImageIndex > 0) continue;
  int32 Number = i + 1;
  while (Used.Contains(Number)) ++Number;
  Slides[i].ImageIndex = Number;
  Used.Add(Number);
 }
}
void AVTGSlideshowDirector::PostLoad()
{
 Super::PostLoad();
 AssignAutomaticImageIndices();
}
#if WITH_EDITOR
void AVTGSlideshowDirector::PostEditChangeChainProperty(FPropertyChangedChainEvent& Event)
{
 if (Event.MemberProperty && Event.MemberProperty->GetFName() == FName("Slides"))
 {
  if (Event.ChangeType & EPropertyChangeType::Duplicate)
  {
   const int32 i = Event.GetArrayIndex(TEXT("Slides"));
   if (Slides.IsValidIndex(i)) Slides[i].ImageIndex = 0;
  }
  AssignAutomaticImageIndices();
 }
 Super::PostEditChangeChainProperty(Event);
}
#endif
bool AVTGSlideshowDirector::ShowSlide(int32 SlideNumber)
{
 int32 FoundIndex = INDEX_NONE;
 for (int32 i = 0; i < Slides.Num(); ++i)
 {
  if (GetImageIndex(i) != SlideNumber) continue;
  if (FoundIndex != INDEX_NONE)
  {
   UE_LOG(LogTemp, Warning, TEXT("Slideshow %s: duplicate Image Index %d"), *SlideshowID.ToString(), SlideNumber);
   return false;
  }
  FoundIndex = i;
 }
 if (!Slides.IsValidIndex(FoundIndex) || !Slides[FoundIndex].Image)
 {
  UE_LOG(LogTemp, Warning, TEXT("Slideshow %s: slide %d is missing or has no image"), *SlideshowID.ToString(), SlideNumber);
  return false;
 }
 auto* Player = OwningPlayer ? OwningPlayer.Get() : UGameplayStatics::GetPlayerController(this, 0);
 if (!Player || !Player->IsLocalController()) return false;
 if (!Widget)
 {
  Widget = CreateWidget<UVTGSlideshowWidget>(Player);
  if (!Widget) return false;
  Widget->AddToViewport(ViewportZOrder);
 }
 GetWorldTimerManager().ClearTimer(CloseTimer);
 Widget->Show(Slides[FoundIndex]);
 CurrentSlideNumber = SlideNumber;
 OnSlideChanged.Broadcast(SlideNumber);
 return true;
}
bool AVTGSlideshowDirector::NextSlide()
{
 if (CurrentSlideNumber == 0) return !Slides.IsEmpty() && ShowSlide(GetImageIndex(0));
 for (int32 i = 0; i + 1 < Slides.Num(); ++i)
  if (GetImageIndex(i) == CurrentSlideNumber) return ShowSlide(GetImageIndex(i + 1));
 return false;
}
bool AVTGSlideshowDirector::PreviousSlide()
{
 for (int32 i = 1; i < Slides.Num(); ++i)
  if (GetImageIndex(i) == CurrentSlideNumber) return ShowSlide(GetImageIndex(i - 1));
 return false;
}
void AVTGSlideshowDirector::SetCurrentMotion(FVTGSlideMotion Motion) { if (Widget) Widget->ChangeMotion(Motion); }
void AVTGSlideshowDirector::StopCurrentMotion() { if (Widget) Widget->StopMotion(); }
void AVTGSlideshowDirector::CloseSlideshow(float FadeSeconds)
{
 GetWorldTimerManager().ClearTimer(CloseTimer);
 if (!Widget) { FinishClose(); return; }
 const float Duration = FMath::Max(0.f, FadeSeconds);
 if (bKeepBlackScreenOnClose)
 {
  FVTGSlide Black;
  Black.FadeSeconds = Duration;
  Widget->Show(Black);
 }
 else Widget->FadeBackToGame(Duration);
 CurrentSlideNumber = 0;
 if (Duration <= 0.f) FinishClose();
 else GetWorldTimerManager().SetTimer(CloseTimer, this, &AVTGSlideshowDirector::FinishClose, Duration, false);
}
void AVTGSlideshowDirector::FinishClose()
{
 // Remove before broadcasting: a listener can immediately start a new slideshow.
 if (!bKeepBlackScreenOnClose) RemoveSlideshow();
 OnSlideshowClosed.Broadcast();
}
void AVTGSlideshowDirector::RemoveSlideshow()
{
 GetWorldTimerManager().ClearTimer(CloseTimer);
 if (Widget) Widget->RemoveFromParent();
 Widget = nullptr;
 CurrentSlideNumber = 0;
}
void AVTGSlideshowDirector::EndPlay(const EEndPlayReason::Type Reason)
{
 RemoveSlideshow();
 Super::EndPlay(Reason);
}

static AVTGSlideshowDirector* FindDirector(UWorld* World, FName ID)
{
 if (!World) return nullptr;
 AVTGSlideshowDirector* Found = nullptr;
 for (TActorIterator<AVTGSlideshowDirector> It(World); It; ++It)
 {
  if (It->SlideshowID != ID) continue;
  if (Found) { UE_LOG(LogTemp, Warning, TEXT("Duplicate Slideshow ID: %s"), *ID.ToString()); return nullptr; }
  Found = *It;
 }
 if (!Found) UE_LOG(LogTemp, Warning, TEXT("Slideshow director not found: %s"), *ID.ToString());
 return Found;
}
AVTGSlideshowDirector* AVTGSlideshowDirector::FindSlideshowByID(const UObject* WorldContextObject, FName ID)
{
 return FindDirector(WorldContextObject ? WorldContextObject->GetWorld() : nullptr, ID);
}
UVTGShowSlideEvent::UVTGShowSlideEvent(const FObjectInitializer& Initializer) : Super(Initializer)
{ EventRuntime = EEventRuntime::Start; bRefireOnLoad = true; }
void UVTGShowSlideEvent::ExecuteEvent_Implementation(APawn* Pawn, APlayerController* Controller, UNarrativeComponent* NarrativeComponent)
{
 UWorld* World = Controller ? Controller->GetWorld() : (Pawn ? Pawn->GetWorld() : GetWorld());
 if (auto* Director = FindDirector(World, SlideshowID)) Director->ShowSlide(SlideNumber);
}
FString UVTGShowSlideEvent::GetGraphDisplayText_Implementation()
{ return FString::Printf(TEXT("%s: slide %d"), *SlideshowID.ToString(), SlideNumber); }
UVTGCloseSlideshowEvent::UVTGCloseSlideshowEvent(const FObjectInitializer& Initializer) : Super(Initializer)
{ EventRuntime = EEventRuntime::End; bRefireOnLoad = false; }
void UVTGCloseSlideshowEvent::ExecuteEvent_Implementation(APawn* Pawn, APlayerController* Controller, UNarrativeComponent* NarrativeComponent)
{
 UWorld* World = Controller ? Controller->GetWorld() : (Pawn ? Pawn->GetWorld() : GetWorld());
 if (auto* Director = FindDirector(World, SlideshowID)) Director->CloseSlideshow(FadeSeconds);
}
FString UVTGCloseSlideshowEvent::GetGraphDisplayText_Implementation()
{ return FString::Printf(TEXT("%s: close slideshow"), *SlideshowID.ToString()); }







