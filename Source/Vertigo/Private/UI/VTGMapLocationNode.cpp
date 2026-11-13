#include "UI/VTGMapLocationNode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/Image.h"
#include "Engine/Font.h"
#include "UObject/UnrealType.h"

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

bool UVTGMapLocationNode::IsNodeSelected() const
{
    const auto* Selected=FindFProperty<FBoolProperty>(GetClass(),TEXT("IsSelected?"));
    return Selected && Selected->GetPropertyValue_InContainer(this);
}

void UVTGMapLocationNode::BuildLocationPresentation()
{
    if(LocationNameButton || !WidgetTree)return;
    auto* Layer=Cast<UOverlay>(GetWidgetFromName(TEXT("Overlay_0")));if(!Layer)return;
    LocationNameButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("LocationNameButton"));
    LocationNameButton->IsFocusable=false;
    FButtonStyle Style=LocationNameButton->GetStyle();
    for(auto* Brush:{&Style.Normal,&Style.Hovered,&Style.Pressed}){Brush->DrawAs=ESlateBrushDrawType::Box;Brush->TintColor=FSlateColor(FLinearColor(0,0,0,.96f));}
    Style.NormalPadding=FMargin(12,7);Style.PressedPadding=Style.NormalPadding;LocationNameButton->SetStyle(Style);
    LocationNameLabel=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("LocationName"));
    auto Font=LocationNameLabel->GetFont();Font.FontObject=LoadObject<UFont>(nullptr,TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Medium_Font"));Font.TypefaceFontName=NAME_None;Font.Size=16;Font.OutlineSettings.OutlineSize=0;
    LocationNameLabel->SetFont(Font);LocationNameLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));LocationNameLabel->SetJustification(ETextJustify::Center);LocationNameLabel->SetAutoWrapText(false);
    LocationNameButton->SetContent(LocationNameLabel);
    auto* SlotForName=Layer->AddChildToOverlay(LocationNameButton);SlotForName->SetHorizontalAlignment(HAlign_Center);SlotForName->SetVerticalAlignment(VAlign_Center);
    LocationNameButton->OnClicked.AddUniqueDynamic(this,&UVTGMapLocationNode::NameClicked);
}

void UVTGMapLocationNode::RefreshLocationText()
{
    BuildLocationPresentation();
    // The old caption, subarea placeholder and hover tooltip are independent
    // legacy presentation paths. None may paint over the selected name badge.
    if(auto* Label=Cast<UTextBlock>(GetWidgetFromName(TEXT("Txt_NodeName")))){Label->SetText(LocationDisplayName);Label->SetVisibility(ESlateVisibility::Collapsed);}
    if(auto* List=GetWidgetFromName(TEXT("VB_SubAreaList")))List->SetVisibility(ESlateVisibility::Collapsed);
    if(auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("Btn_NodeInteractor")))){Button->SetToolTip(nullptr);Button->SetToolTipText(FText::GetEmpty());}
    SetToolTip(nullptr);SetToolTipText(FText::GetEmpty());
    const bool Selected=IsNodeSelected();
    if(LocationNameLabel)LocationNameLabel->SetText(LocationDisplayName);
    if(LocationNameButton)LocationNameButton->SetVisibility(Selected?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(!Selected)ShowDescription=false;
    RefreshDescriptionPanel();
}

void UVTGMapLocationNode::RefreshDescriptionPanel()
{
    auto* Canvas=Cast<UCanvasPanel>(GetParent());auto* NodeSlot=Cast<UCanvasPanelSlot>(Slot);
    if(!ShowDescription || !IsNodeSelected() || !Canvas || !NodeSlot){if(DescriptionPanel)DescriptionPanel->SetVisibility(ESlateVisibility::Collapsed);return;}
    if(!DescriptionPanel)
    {
        DescriptionPanel=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("LocationDescriptionPanel"));DescriptionPanel->SetBrushColor(FLinearColor(0,0,0,.94f));DescriptionPanel->SetPadding(FMargin(16,12));
        auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();DescriptionPanel->SetContent(Scroll);
        DescriptionLabel=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("LocationDescription"));auto Font=DescriptionLabel->GetFont();Font.FontObject=LoadObject<UFont>(nullptr,TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Regular_Font"));Font.TypefaceFontName=NAME_None;Font.Size=16;Font.OutlineSettings.OutlineSize=0;DescriptionLabel->SetFont(Font);DescriptionLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));DescriptionLabel->SetAutoWrapText(true);Scroll->AddChild(DescriptionLabel);
        Canvas->AddChildToCanvas(DescriptionPanel);
    }
    DescriptionLabel->SetText(LocationDescription.IsEmpty()?NSLOCTEXT("VTGMapLocations","NoInformation","No information available."):LocationDescription);
    auto* PanelSlot=CastChecked<UCanvasPanelSlot>(DescriptionPanel->Slot);PanelSlot->SetZOrder(20);
    const auto& Geometry=Canvas->GetCachedGeometry();const FVector2D CanvasSize=Geometry.GetLocalSize();
    FVector2D NodePosition=NodeSlot->GetPosition()+NodeSlot->GetAnchors().Minimum*CanvasSize;
    if(GetCachedGeometry().GetLocalSize().X>0)NodePosition=Geometry.AbsoluteToLocal(GetCachedGeometry().GetAbsolutePosition());
    const float NodeHeight=GetCachedGeometry().GetLocalSize().Y>0?GetCachedGeometry().GetLocalSize().Y:202.f;
    FVector2D Position=NodePosition+FVector2D(-496,NodeHeight*.5f-75.f);
    // WBP_Map_1 is authored on a near-zero-size centered canvas with an oversized
    // background image. Use that image's drawn rectangle, not canvas size, for
    // bounds; otherwise the left edge is incorrectly clamped to the screen center.
    FVector2D FramePosition(-960,-540),FrameSize(1920,1080);
    for(auto* Child:Canvas->GetAllChildren())if(Cast<UImage>(Child))if(auto* ImageSlot=Cast<UCanvasPanelSlot>(Child->Slot)){FramePosition=ImageSlot->GetPosition()+ImageSlot->GetAnchors().Minimum*CanvasSize;FrameSize=ImageSlot->GetSize();break;}
    const float Left=FramePosition.X+FrameSize.X*.198f+12,Right=FramePosition.X+FrameSize.X*.80f-12;
    // Center minus 20% of the full terminal width is the 30% line. Test the
    // proposed panel's left edge before applying any screen-bound clamping.
    const float SwitchLine=FramePosition.X+FrameSize.X*.30f;
    float PanelWidth=480.f;
    if(Position.X<SwitchLine)
    {
        const float NodeWidth=GetCachedGeometry().GetLocalSize().X>0?GetCachedGeometry().GetLocalSize().X:NodeSlot->GetSize().X;
        Position.X=NodePosition.X+NodeWidth+16;
        PanelWidth=FMath::Min(480.f,FMath::Max(120.f,static_cast<float>(Right-Position.X)));
    }
    Position.X=FMath::Clamp(Position.X,Left,FMath::Max(Left,Right-PanelWidth));Position.Y=FMath::Clamp(Position.Y,FramePosition.Y+FrameSize.Y*.185f+12,FramePosition.Y+FrameSize.Y*.815f-162);
    FAnchorData Layout;Layout.Anchors=FAnchors(0,0);Layout.Offsets=FMargin(Position.X,Position.Y,PanelWidth,150);PanelSlot->SetLayout(Layout);
    DescriptionPanel->SetVisibility(ESlateVisibility::Visible);
}

void UVTGMapLocationNode::NodePressed()
{
    CarryDescription=false;
    if(auto* Canvas=Cast<UCanvasPanel>(GetParent()))for(auto* Child:Canvas->GetAllChildren())if(auto* Node=Cast<UVTGMapLocationNode>(Child))CarryDescription|=Node->ShowDescription;
}

void UVTGMapLocationNode::NodeClicked()
{
    ShowDescription=IsNodeSelected() && CarryDescription;CarryDescription=false;RefreshLocationText();
}

void UVTGMapLocationNode::NameClicked()
{
    if(IsNodeSelected()){ShowDescription=true;RefreshDescriptionPanel();}
}

void UVTGMapLocationNode::NativePreConstruct()
{
    Super::NativePreConstruct();RefreshLocationText();
}

void UVTGMapLocationNode::NativeConstruct()
{
    Super::NativeConstruct();
    if(auto* Button=Cast<UButton>(GetWidgetFromName(TEXT("Btn_NodeInteractor")))){Button->OnPressed.AddUniqueDynamic(this,&UVTGMapLocationNode::NodePressed);Button->OnClicked.AddUniqueDynamic(this,&UVTGMapLocationNode::NodeClicked);}
    RefreshLocationText();
}

void UVTGMapLocationNode::NativeDestruct()
{
    if(DescriptionPanel)DescriptionPanel->RemoveFromParent();DescriptionPanel=nullptr;DescriptionLabel=nullptr;ShowDescription=false;
    Super::NativeDestruct();
}
