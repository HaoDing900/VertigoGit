#include "VTGSlideDetails.h"
#include "UI/VTGSlideshow.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Engine/Texture2D.h"
#include "ScopedTransaction.h"

// A fixed 1920 x 1080 logical UMG canvas, displayed at quarter scale.
class SVTGSlidePreview : public SCompoundWidget
{
public:
 SLATE_BEGIN_ARGS(SVTGSlidePreview) {} SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>, Slide) SLATE_END_ARGS()
 void Construct(const FArguments& Args)
 {
  Handle = Args._Slide;
  Brush.DrawAs = ESlateBrushDrawType::Image;
  ChildSlot
  [ SNew(SVerticalBox)
   + SVerticalBox::Slot().AutoHeight()
   [ SNew(SBox).HeightOverride(270)
    [ SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly).HAlign(HAlign_Left).VAlign(VAlign_Top)
     [ SNew(SBox).WidthOverride(480).HeightOverride(270)
    [ SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::Black).Clipping(EWidgetClipping::ClipToBoundsAlways)
     [ SNew(SImage).Image(&Brush).RenderTransformPivot(FVector2D(.5,.5)).RenderTransform_Lambda([this]() -> TOptional<FSlateRenderTransform>
      {
       const FVTGSlide Slide = Read();
       const FVector2D Size = Slide.Image ? FVector2D(Slide.Image->GetSizeX(),Slide.Image->GetSizeY()) : FVector2D(1920,1080);
       const FWidgetTransform T = Slide.Motion.Evaluate(Time, Size);
       return TOptional<FSlateRenderTransform>(Concatenate(FScale2D(T.Scale), FQuat2D(FMath::DegreesToRadians(T.Angle)), FVector2D(T.Translation * .25)));
      }) ] ] ] ] ]
   + SVerticalBox::Slot().AutoHeight().Padding(0,4)
   [ SNew(SHorizontalBox)
    + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString("Auto Fit (Cover)")).ToolTipText(FText::FromString("Keep aspect ratio and fill screen. Resets scale, offset and angle."))
     .IsEnabled_Lambda([this]() { return Read().Image != nullptr; }).OnClicked_Lambda([this]() { return ApplyFit(false,false); })]
    + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString("Fit Inside")).ToolTipText(FText::FromString("Show whole image with black bars. Resets scale, offset and angle."))
     .IsEnabled_Lambda([this]() { return Read().Image != nullptr; }).OnClicked_Lambda([this]() { return ApplyFit(true,false); })]
    + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString("Top to Bottom")).ToolTipText(FText::FromString("Auto Cover, pan from top to bottom. Resets scale, offset, angle, shake and loop."))
     .IsEnabled_Lambda([this]() { return Read().Image != nullptr; }).OnClicked_Lambda([this]() { return ApplyFit(false,true); })]
   ]
   + SVerticalBox::Slot().AutoHeight().Padding(0,4)
   [ SNew(SSlider).Value_Lambda([this]() { return FMath::Clamp(Time / FMath::Max(Read().Motion.Duration,.01f),0.f,1.f); })
    .OnValueChanged_Lambda([this](float Value) { bPlaying = false; Time = Value * FMath::Max(Read().Motion.Duration,.01f); }) ]
   + SVerticalBox::Slot().AutoHeight()
   [ SNew(SHorizontalBox)
    + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString("Start")).OnClicked_Lambda([this]() { bPlaying=false; Time=0; return FReply::Handled(); })]
    + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString("End")).OnClicked_Lambda([this]() { bPlaying=false; Time=Read().Motion.Duration; return FReply::Handled(); })]
    + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text_Lambda([this]() { return FText::FromString(bPlaying ? "Pause" : "Play"); }).OnClicked_Lambda([this]() { if (!bPlaying && Time >= Read().Motion.Duration) Time=0; bPlaying=!bPlaying; return FReply::Handled(); })]
    + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("  %.2fs / %.2fs"),Time,Read().Motion.Duration)); })]
   ]
   + SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(STextBlock).Text(FText::FromString("Preview: 1920 x 1080 UMG units (DPI scale 1). Edit Motion below; preview updates live.")).AutoWrapText(true)]
  ];
 }
 virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override
 {
  SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
  const FVTGSlide Slide = Read();
  Brush.SetResourceObject(Slide.Image);
  Brush.TintColor = Slide.Image ? FLinearColor::White : FLinearColor::Transparent;
  if (bPlaying)
  {
   Time += DeltaTime;
   if (!Slide.Motion.bLoopPingPong && Time >= FMath::Max(Slide.Motion.Duration,Slide.Motion.ShakeDuration))
   { Time = FMath::Max(Slide.Motion.Duration,Slide.Motion.ShakeDuration); bPlaying=false; }
  }
  Invalidate(EInvalidateWidgetReason::Paint);
 }
private:
 FReply ApplyFit(bool bContain, bool bPan)
 {
  const auto Motion = Handle->GetChildHandle("Motion");
  if (!Motion.IsValid() || !Read().Image) return FReply::Handled();
  const FScopedTransaction Transaction(FText::FromString("Auto fit slideshow image"));
  Motion->GetChildHandle("ImageFit")->SetValue(static_cast<uint8>(bContain ? EVTGSlideFit::Contain : EVTGSlideFit::Cover));
  Motion->GetChildHandle("bPanTopToBottom")->SetValue(bPan);
  for (const TCHAR* Name : {TEXT("StartScale"),TEXT("EndScale"),TEXT("StartOffset"),TEXT("EndOffset")})
  {
   const bool bScale = FString(Name).Contains(TEXT("Scale"));
   auto Vector = Motion->GetChildHandle(Name);
   Vector->GetChildHandle("X")->SetValue(bScale ? 1.0 : 0.0);
   Vector->GetChildHandle("Y")->SetValue(bScale ? 1.0 : 0.0);
  }
  Motion->GetChildHandle("StartAngle")->SetValue(0.f);
  Motion->GetChildHandle("EndAngle")->SetValue(0.f);
  if (bPan)
  {
   Motion->GetChildHandle("ShakeAmplitude")->SetValue(0.f);
   Motion->GetChildHandle("bLoopPingPong")->SetValue(false);
  }
  bPlaying=false; Time=0;
  return FReply::Handled();
 }
 FVTGSlide Read() const
 {
  void* Data = nullptr;
  if (Handle.IsValid() && Handle->IsValidHandle() && Handle->GetValueData(Data)==FPropertyAccess::Success && Data) return *static_cast<FVTGSlide*>(Data);
  return FVTGSlide();
 }
 TSharedPtr<IPropertyHandle> Handle;
 FSlateBrush Brush;
 float Time=0;
 bool bPlaying=false;
};

TSharedRef<IPropertyTypeCustomization> FVTGSlideDetails::MakeInstance() { return MakeShared<FVTGSlideDetails>(); }
void FVTGSlideDetails::CustomizeHeader(TSharedRef<IPropertyHandle> Handle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils&)
{ Row.NameContent()[Handle->CreatePropertyNameWidget()].ValueContent()[Handle->CreatePropertyValueWidget()]; }
void FVTGSlideDetails::CustomizeChildren(TSharedRef<IPropertyHandle> Handle, IDetailChildrenBuilder& Builder, IPropertyTypeCustomizationUtils&)
{
 uint32 Count=0; Handle->GetNumChildren(Count);
 // Show image first, then preview, then fade and motion settings.
 if (auto Index=Handle->GetChildHandle("ImageIndex")) Builder.AddProperty(Index.ToSharedRef());
 if (auto Image=Handle->GetChildHandle("Image")) Builder.AddProperty(Image.ToSharedRef());
 Builder.AddCustomRow(FText::FromString("Motion Preview")).WholeRowContent().MinDesiredWidth(240)
 [SNew(SVTGSlidePreview).Slide(Handle)];
 for (uint32 i=0; i<Count; ++i)
 {
  auto Child=Handle->GetChildHandle(i);
  if (Child.IsValid() && Child->GetProperty()->GetFName()!=FName("Image") && Child->GetProperty()->GetFName()!=FName("ImageIndex")) Builder.AddProperty(Child.ToSharedRef());
 }
}



