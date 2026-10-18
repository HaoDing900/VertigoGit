#include "UI/VTGHintLayer.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

UVTGHintLayer::UVTGHintLayer(const FObjectInitializer& Initializer):Super(Initializer)
{
    SkipHoldText=NSLOCTEXT("VTGHint","SkipHold","Hold");
    SkipActionText=NSLOCTEXT("VTGHint","SkipAction","to skip");
    NewMailHintText=NSLOCTEXT("VTGHint","NewMail","New Message Received. ");
}
FText UVTGHintLayer::ResolveHintDescription(FName ID) const
{
    for(const auto& Entry:HintDescriptions)if(Entry.ID==ID && !Entry.Text.IsEmpty())return Entry.Text;
    return FText::FromName(ID);
}
void UVTGHintLayer::UpdateSkipHint(bool Visible,float Progress,const FSlateBrush& KeyBrush)
{
    if(auto* Panel=GetWidgetFromName(TEXT("SkipHintPanel")))Panel->SetVisibility(Visible?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    if(!Visible)return;
    if(auto* Hold=Cast<UTextBlock>(GetWidgetFromName(TEXT("SkipHoldTextBlock"))))Hold->SetText(SkipHoldText);
    if(auto* Action=Cast<UTextBlock>(GetWidgetFromName(TEXT("SkipActionTextBlock"))))Action->SetText(SkipActionText);
    if(auto* Image=Cast<UImage>(GetWidgetFromName(TEXT("SkipKeyImage"))))
    {
        auto Brush=Image->GetBrush();Brush.SetResourceObject(KeyBrush.GetResourceObject());Image->SetBrush(Brush);
    }
    if(auto* Bar=Cast<UProgressBar>(GetWidgetFromName(TEXT("SkipHoldProgress"))))Bar->SetPercent(FMath::Clamp(Progress,0.f,1.f));
}
