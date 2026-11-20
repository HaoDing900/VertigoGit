#include "UI/VTGNoteTextWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/UnrealType.h"

UTextBlock* UVTGNoteTextWidget::Body() const
{
    return WidgetTree ? Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text"))) : nullptr;
}
USizeBox* UVTGNoteTextWidget::Frame() const
{
    return WidgetTree ? Cast<USizeBox>(WidgetTree->FindWidget(TEXT("SizeBox_22"))) : nullptr;
}
int32 UVTGNoteTextWidget::BaseSize()
{
    if (const auto* P = FindFProperty<FIntProperty>(GetClass(), TEXT("FontSize")))
        return FMath::Max(1, P->GetPropertyValue_InContainer(this));
    if (!OriginalSize && Body()) OriginalSize = Body()->GetFont().Size;
    return FMath::Max(1, OriginalSize);
}
FVector2D UVTGNoteTextWidget::AvailableSize() const
{
    const auto* Box = Frame();
    if (!Box || !Body()) return FVector2D::ZeroVector;
    const auto* FrameSlot = Cast<USizeBoxSlot>(Box->GetContent() ? Box->GetContent()->Slot : nullptr);
    const FMargin ContentPadding = FrameSlot ? FrameSlot->GetPadding() : FMargin();
    FVector2D Size(Box->GetWidthOverride() - ContentPadding.GetTotalSpaceAlong<Orient_Horizontal>(),
        Box->GetHeightOverride() - ContentPadding.GetTotalSpaceAlong<Orient_Vertical>());
    if (OverflowScroll) Size.X -= 16.f; // Reserve the vertical scrollbar's space.
    if (Body()->GetWrapTextAt() > 0) Size.X = FMath::Min(Size.X, double(Body()->GetWrapTextAt()));
    return Size;
}
FVector2D UVTGNoteTextWidget::Measure(int32 Size) const
{
    const auto* Text = Body();
    FSlateFontInfo Font = Text->GetFont();
    Font.Size = Size;
    // Use the same Slate text layout, including per-character wrapping, margins and line spacing.
    const auto* Policy = FindFProperty<FEnumProperty>(Text->GetClass(), TEXT("WrappingPolicy"));
    const auto* Margin = FindFProperty<FStructProperty>(Text->GetClass(), TEXT("Margin"));
    const auto* LineHeight = FindFProperty<FFloatProperty>(Text->GetClass(), TEXT("LineHeightPercentage"));
    auto Probe = SNew(STextBlock).Text(Text->GetText()).Font(Font)
        .WrapTextAt(float(AvailableSize().X)).AutoWrapText(false)
        .WrappingPolicy(Policy ? ETextWrappingPolicy(Policy->GetUnderlyingProperty()->GetSignedIntPropertyValue(Policy->ContainerPtrToValuePtr<void>(Text))) : ETextWrappingPolicy::DefaultWrapping)
        .Margin(Margin ? *Margin->ContainerPtrToValuePtr<FMargin>(Text) : FMargin())
        .LineHeightPercentage(LineHeight ? LineHeight->GetPropertyValue_InContainer(Text) : 1.f);
    Probe->SlatePrepass(1.f);
    return Probe->GetDesiredSize();
}
void UVTGNoteTextWidget::Apply(int32 Reduction)
{
    auto* Text = Body();
    auto Font = Text->GetFont();
    Font.Size = FMath::Max(1, BaseSize() - Reduction);
    if (Font.Size != Text->GetFont().Size) Text->SetFont(Font);
    const FVector2D Desired = Measure(Font.Size), Available = AvailableSize();
    if (!OverflowScroll && (Desired.Y > Available.Y + .5f || Desired.X > Available.X + .5f))
    {
        auto* Box = Frame();
        const FMargin ContentPadding = CastChecked<USizeBoxSlot>(Text->Slot)->GetPadding();
        Box->ClearChildren();
        OverflowScroll = WidgetTree->ConstructWidget<UScrollBox>();
        OverflowScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
        Box->AddChild(OverflowScroll);
        CastChecked<USizeBoxSlot>(OverflowScroll->Slot)->SetPadding(ContentPadding);
        OverflowScroll->AddChild(Text);
        CastChecked<UScrollBoxSlot>(Text->Slot)->SetHorizontalAlignment(HAlign_Fill);
    }
    if (OverflowScroll) OverflowScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
}
void UVTGNoteTextWidget::FitDocument()
{
    TArray<UVTGNoteTextWidget*> Pages;
    if (auto* Parent = GetParent())
        for (auto* Child : Parent->GetAllChildren())
            if (auto* Page = Cast<UVTGNoteTextWidget>(Child)) Pages.Add(Page);
    if (Pages.IsEmpty()) Pages.Add(this);
    uint32 Hash = 0;
    for (auto* Page : Pages)
    {
        if (!Page->Body() || !Page->Frame() || Page->AvailableSize().GetMin() <= 0) return;
        Hash = HashCombine(Hash, GetTypeHash(Page));
        Hash = HashCombine(Hash, GetTypeHash(Page->Body()->GetText().ToString()));
        Hash = HashCombine(Hash, GetTypeHash(Page->AvailableSize()));
        Hash = HashCombine(Hash, GetTypeHash(Page->BaseSize()));
    }
    if (bHasLayout && Hash == LastLayoutHash) return;
    for (auto* Page : Pages)
    {
        if (!Page->OverflowScroll) continue;
        auto* Box = Page->Frame();
        auto* Text = Page->Body();
        const FMargin ContentPadding = CastChecked<USizeBoxSlot>(Page->OverflowScroll->Slot)->GetPadding();
        Page->OverflowScroll->ClearChildren();
        Box->ClearChildren();
        Box->AddChild(Text);
        CastChecked<USizeBoxSlot>(Text->Slot)->SetPadding(ContentPadding);
        Page->OverflowScroll = nullptr;
    }
    int32 Reduction = 0;
    for (; Reduction < 4; ++Reduction)
    {
        bool Fits = true;
        for (auto* Page : Pages)
        {
            const FVector2D Desired = Page->Measure(FMath::Max(1, Page->BaseSize() - Reduction));
            const FVector2D Available = Page->AvailableSize();
            Fits &= Desired.Y <= Available.Y + .5f && Desired.X <= Available.X + .5f;
        }
        if (Fits) break;
    }
    for (auto* Page : Pages)
    {
        Page->Apply(Reduction);
        Page->LastLayoutHash = Hash;
        Page->bHasLayout = true;
    }
}
void UVTGNoteTextWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    FitDocument();
}

