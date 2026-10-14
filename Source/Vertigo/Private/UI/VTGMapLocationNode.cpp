#include "UI/VTGMapLocationNode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UVTGMapLocationTooltip::SetLocation(const FText& Name, const FText& Description, const FSlateFontInfo& Font)
{
    Title = Name; Body = Description; TextFont = Font;
    if (TitleLabel) TitleLabel->SetText(Title);
    if (BodyLabel)
    {
        BodyLabel->SetText(Body);
        BodyLabel->SetVisibility(Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
}

TSharedRef<SWidget> UVTGMapLocationTooltip::RebuildWidget()
{
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    if (!WidgetTree->RootWidget)
    {
        auto* Width = WidgetTree->ConstructWidget<USizeBox>();
        Width->SetWidthOverride(320.f);
        WidgetTree->RootWidget = Width;
        auto* Background = WidgetTree->ConstructWidget<UBorder>();
        Background->SetBrushColor(FLinearColor(0, 0, 0, .96f));
        Background->SetPadding(FMargin(16.f, 12.f));
        Width->SetContent(Background);
        auto* Content = WidgetTree->ConstructWidget<UVerticalBox>();
        Background->SetContent(Content);
        TitleLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LocationTitle"));
        BodyLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LocationDescription"));
        for (auto* Text : {TitleLabel.Get(), BodyLabel.Get()})
        {
            Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
            Text->SetAutoWrapText(true);
        }
        auto Font = TextFont; Font.OutlineSettings.OutlineSize = 0; Font.Size = 18; TitleLabel->SetFont(Font);
        Font.Size = 16; BodyLabel->SetFont(Font);
        Content->AddChildToVerticalBox(TitleLabel);
        Content->AddChildToVerticalBox(BodyLabel)->SetPadding(FMargin(0, 8, 0, 0));
        SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    SetLocation(Title, Body, TextFont);
    return Super::RebuildWidget();
}

void UVTGMapLocationNode::RefreshLocationText()
{
    auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("Txt_NodeName")));
    auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("Btn_NodeInteractor")));
    if (Label)
    {
        Label->SetText(LocationDisplayName);
        Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    if (!Button) return;
    if (LocationDisplayName.IsEmpty() && LocationDescription.IsEmpty())
    {
        Button->SetToolTip(nullptr); return;
    }
    if (!LocationTooltip)
        LocationTooltip = CreateWidget<UVTGMapLocationTooltip>(this, UVTGMapLocationTooltip::StaticClass());
    if (LocationTooltip)
    {
        LocationTooltip->SetLocation(LocationDisplayName, LocationDescription, Label ? Label->GetFont() : FSlateFontInfo());
        Button->SetToolTip(LocationTooltip);
    }
}

void UVTGMapLocationNode::NativePreConstruct()
{
    Super::NativePreConstruct();
    RefreshLocationText();
}
void UVTGMapLocationNode::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshLocationText();
}
