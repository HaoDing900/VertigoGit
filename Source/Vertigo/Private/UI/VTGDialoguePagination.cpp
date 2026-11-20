#include "UI/VTGDialoguePagination.h"
#include "UI/VTGLocalizationSubsystem.h"
#include "UI/VTGLocalizationSettings.h"
#include "UI/VTGDialogueHistorySubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "NarrativeComponent.h"
#include "Dialogue.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Internationalization/BreakIterator.h"
#include "Internationalization/IBreakIterator.h"
#include "Widgets/SWidget.h"
#include "Engine/DataTable.h"

TArray<FString> VTGDialoguePagination::Split(const FString& InText,TFunctionRef<bool(const FString&)> Fits,bool AllowWordSplit)
{
 FString Text=InText;TArray<int32> Forced;
 for(int32 At=Text.Find(TEXT("[page]"));At!=INDEX_NONE;At=Text.Find(TEXT("[page]"))){Forced.Add(At);Text.RemoveAt(At,6,false);}
 if(Text.IsEmpty() || (Forced.IsEmpty() && Fits(Text)))return {Text};
 struct FBoundary{int32 Offset;TArray<FString> Open;};
 TArray<FBoundary> Bounds;Bounds.Add({0,{}});TArray<FString> Open;
 auto Graphemes=FBreakIterator::CreateCharacterBoundaryIterator();Graphemes->SetString(Text);
 for(int32 Pos=0;Pos<Text.Len();)
 {
  if(Text[Pos]==TEXT('<'))
  {
   int32 End=Pos+1;bool Quoted=false;TCHAR Quote=0;
   for(;End<Text.Len();++End){const TCHAR C=Text[End];if(Quoted){if(C==Quote)Quoted=false;}else if(C==TEXT('"') || C==TEXT('\'')){Quoted=true;Quote=C;}else if(C==TEXT('>'))break;}
   if(End==Text.Len())return {Text};
   const FString Tag=Text.Mid(Pos,End-Pos+1);
   if(Tag.StartsWith(TEXT("</"))){if(Open.IsEmpty())return {Text};Open.Pop();}
   else if(!Tag.EndsWith(TEXT("/>")))Open.Add(Tag);
   Pos=End+1;
  }
  else {const int32 Next=Graphemes->MoveToCandidateAfter(Pos);Pos=Next>Pos?Next:Pos+1;}
  Bounds.Add({Pos,Open});
 }
 if(!Open.IsEmpty())return {Text}; // Leave malformed authoring untouched.
 auto Slice=[&](int32 Start,int32 End){FString Result;for(const auto& Tag:Bounds[Start].Open)Result+=Tag;Result+=Text.Mid(Bounds[Start].Offset,Bounds[End].Offset-Bounds[Start].Offset);for(int32 I=0;I<Bounds[End].Open.Num();++I)Result+=TEXT("</>");return Result;};
 TArray<FString> Pages;
 for(int32 Start=0;Start<Bounds.Num()-1;)
 {
  int32 Low=Start+1,High=Bounds.Num()-1,Best=Start;
  for(int32 I=Start+1;I<Bounds.Num();++I)if(Forced.Contains(Bounds[I].Offset)){High=I;break;}
  const int32 SegmentEnd=High;
  while(Low<=High){const int32 Mid=(Low+High)/2;if(Fits(Slice(Start,Mid))){Best=Mid;Low=Mid+1;}else High=Mid-1;}
  if(Best==Start)return {Text}; // Even one rendered unit cannot fit: do not lose text.
  if(Best<Bounds.Num()-1)
  {
   // Prefer whole sentences over filling every last line. Avoid decimals and common abbreviations.
   if(Best<SegmentEnd)
   {
    int32 Sentence=INDEX_NONE,Clause=INDEX_NONE,Word=INDEX_NONE;
    const TArray<FString> Abbreviations={TEXT("mr."),TEXT("mrs."),TEXT("ms."),TEXT("dr."),TEXT("prof."),TEXT("sr."),TEXT("jr."),TEXT("etc."),TEXT("e.g."),TEXT("i.e.")};
    for(int32 I=Best;I>Start;--I)
    {
     const int32 Offset=Bounds[I].Offset;const TCHAR C=Text[Offset-1];
     if(FChar::IsWhitespace(C)){if(Word==INDEX_NONE)Word=I;continue;}
     if((C==TEXT(',') || C==TEXT(';') || C==TEXT(':')) && Clause==INDEX_NONE)Clause=I;
     bool End=C==TEXT('!') || C==TEXT('?') || C==TEXT('.');
     if(C==TEXT('.'))
     {
      if(Offset<Text.Len() && !FChar::IsWhitespace(Text[Offset]) && Text[Offset]!=TEXT('"') && Text[Offset]!=TEXT('\''))End=false;
      int32 TokenStart=Offset-2;while(TokenStart>=0 && !FChar::IsWhitespace(Text[TokenStart]))--TokenStart;
      const FString Token=Text.Mid(TokenStart+1,Offset-TokenStart-1).ToLower();
      if(Abbreviations.Contains(Token) || (Token.Len()==2 && FChar::IsAlpha(Token[0])))End=false;
     }
     if(End){Sentence=I;break;}
    }
    if(Sentence!=INDEX_NONE)Best=Sentence;
    else if(Clause!=INDEX_NONE)Best=Clause;
    else if(Word!=INDEX_NONE)Best=Word;
    else if(!AllowWordSplit)return {Text}; // Main dialogue preserves words; side overflow may use a safe grapheme boundary.
   }
  }
  Pages.Add(Slice(Start,Best));Start=Best;
 }
 return Pages;
}

namespace
{
bool DialogueWidgetShown(UWidget* Widget)
{
    for(auto* W=Widget;W;W=W->GetParent())if(!W->IsVisible() || W->GetRenderOpacity()<=0.f)return false;
    return Widget!=nullptr;
}
FVector2D SideTextArea(UUserWidget* Overlay,URichTextBlock* Label)
{
    const auto& Geometry=Label->GetCachedGeometry();FVector2D Available=Geometry.GetLocalSize();
    auto* Frame=Overlay->GetWidgetFromName(TEXT("DialogueBox_Side"));
    const auto& FrameGeometry=Frame->GetCachedGeometry();
    if(FrameGeometry.GetLocalSize().X>20.f && FrameGeometry.GetLocalSize().Y>10.f)
    {
        const auto End=Geometry.AbsoluteToLocal(FrameGeometry.LocalToAbsolute(FrameGeometry.GetLocalSize()));
        if(End.X>20.f && End.Y>10.f)
        {
            Available.X=FMath::Min(Available.X,End.X-4.f);
            auto* Slot=Cast<UCanvasPanelSlot>(Label->Slot);
            Available.Y=Slot && Slot->GetAutoSize()?End.Y-4.f:FMath::Min(Available.Y,End.Y-4.f);
        }
    }
    return Available;
}
}
void UVTGDialogueHistorySubsystem::UpdatePagination()
{
    UDialogue* Dialogue=nullptr;
    for(TObjectIterator<UNarrativeComponent> It;It;++It)
        if(!It->IsTemplate() && It->GetWorld()==GetWorld() && It->IsInDialogue()){Dialogue=It->GetCurrentDialogue();if(Dialogue)break;}
    auto* Overlay=PromptOverlay.Get();
    if(!Overlay)for(TObjectIterator<UUserWidget> It;It;++It)
        if(!It->IsTemplate() && It->GetWorld()==GetWorld() && It->GetClass()->GetFName()==TEXT("BP_Narrative3Overlay_C") && DialogueWidgetShown(*It)){Overlay=*It;break;}
    auto* Side=Overlay?Cast<URichTextBlock>(Overlay->GetWidgetFromName(TEXT("Line_Side"))):nullptr;
    const bool IsSide=DialogueWidgetShown(Side) && DialogueWidgetShown(Overlay->GetWidgetFromName(TEXT("DialogueBox_Side")));
    auto* Label=IsSide?Side:Overlay?Cast<URichTextBlock>(Overlay->GetWidgetFromName(TEXT("Line_Main"))):nullptr;
    if(!Dialogue || !DialogueWidgetShown(Label))
    {
        SideTextFit.Restore();if(PageIndicator)PageIndicator->SetVisibility(ESlateVisibility::Collapsed);
        if(!Dialogue){PagedDialogue.Reset();PagedNode.Reset();PagedLabel.Reset();}return;
    }
    const auto* Languages=GetGameInstance()->GetSubsystem<UVTGLocalizationSubsystem>();
    const FString Language=Languages?Languages->GetLanguage():TEXT("origin");
    const bool Enabled=IsSide?Language!=TEXT("origin"):
        GetDefault<UVTGLocalizationSettings>()->bEnableEnglishDialoguePagination && Language.StartsWith(TEXT("en"));
    const FVector2D Available=IsSide?SideTextArea(Overlay,Label):Label->GetCachedGeometry().GetLocalSize();
    const FString Complete=Dialogue->GetCompletePresentationText().ToString();
    if(!IsSide && PagedLabel.Get()==Side && Dialogue->GetPresentationPageCount()>0)Dialogue->SetPresentationPages({});
    const bool CanPage=Dialogue->CanPaginateCurrentLine(IsSide);
    if(IsSide && Enabled)SideTextFit.Update(Label,Available,true,&Complete,CanPage);else SideTextFit.Restore();
    if(!Enabled || !CanPage)
    {
        if(Dialogue->GetPresentationPageCount()>0){Dialogue->SetPresentationPages({});Label->SetText(Dialogue->GetCompletePresentationText());}
        if(PageIndicator)PageIndicator->SetVisibility(ESlateVisibility::Collapsed);
        PagedDialogue.Reset();PagedNode.Reset();PagedLabel.Reset();return;
    }
    if(Available.X<40 || Available.Y<20)return;
    const bool Changed=PagedDialogue.Get()!=Dialogue || PagedNode.Get()!=Dialogue->GetPresentationNode() || PagedLabel.Get()!=Label ||
        PagedText!=Complete || PagedLanguage!=Language || !PagedSize.Equals(Available,1.f);
    if(Changed)
    {
        TArray<FString> Pages;
        if(IsSide)Pages=SideTextFit.GetPages();
        else
        {
            auto* Measure=DuplicateObject<URichTextBlock>(Label,this);Measure->SetVisibility(ESlateVisibility::HitTestInvisible);
            FindFProperty<FFloatProperty>(Measure->GetClass(),TEXT("WrapTextAt"))->SetPropertyValue_InContainer(Measure,Available.X-4.f);
            Measure->SetAutoWrapText(false);auto Slate=Measure->TakeWidget();
            Pages=VTGDialoguePagination::Split(Complete,[&](const FString& Candidate){Measure->SetText(FText::AsCultureInvariant(Candidate));Slate->SlatePrepass(1.f);const auto Desired=Slate->GetDesiredSize();return Desired.Y<=Available.Y-4.f && Desired.X<=Available.X+1.f;});
            Measure->ReleaseSlateResources(true);
        }
        Dialogue->SetPresentationPages(Pages,IsSide);
        PagedDialogue=Dialogue;PagedNode=const_cast<UDialogueNode*>(Dialogue->GetPresentationNode());PagedLabel=Label;
        PagedText=Complete;PagedLanguage=Language;PagedSize=Available;
    }
    const FText Page=Dialogue->GetPresentationPage();if(!Label->GetText().EqualTo(Page)){Label->SetText(Page);if(IsSide)Label->RefreshTextLayout();}
 if(PromptSurface && Dialogue->GetPresentationPageCount()>1)
 {
  if(!PageIndicator)
  {
   PageIndicator=Overlay->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("DialoguePageIndicator"));
   auto PageFont=Label->GetDefaultTextStyle().Font;PageFont.Size=14;PageIndicator->SetFont(PageFont);PageIndicator->SetColorAndOpacity(FLinearColor::White);
  }
  if(PageIndicator->GetParent()!=PromptSurface){PageIndicator->RemoveFromParent();auto* Slot=PromptSurface->AddChildToCanvas(PageIndicator);Slot->SetAnchors(FAnchors(.95f,.80f));Slot->SetAlignment(FVector2D(1,0));Slot->SetAutoSize(true);}
  PageIndicator->SetText(FText::AsCultureInvariant(FString::Printf(TEXT("%d / %d"),Dialogue->GetPresentationPageIndex()+1,Dialogue->GetPresentationPageCount())));PageIndicator->SetVisibility(ESlateVisibility::HitTestInvisible);
 }
 else if(PageIndicator)PageIndicator->SetVisibility(ESlateVisibility::Collapsed);
}

void FVTGDialogueTextFit::Restore()
{
    if(auto* Label=Target.Get())
    {
        Label->SetTextStyleSet(OriginalTable.Get());
        if(HadStyleOverride)Label->SetDefaultTextStyle(OriginalStyle);else Label->ClearAllDefaultStyleOverrides();
    }
    Target.Reset();OriginalTable.Reset();LastText.Empty();Pages.Reset();LastSize=FVector2D::ZeroVector;
}

void FVTGDialogueTextFit::Update(URichTextBlock* Label,const FVector2D& Available,bool Enabled,const FString* CompleteText,bool AllowPages)
{
    if(!Enabled || !Label){Restore();return;}
    if(Available.X<20.f || Available.Y<10.f)return; // Wait for layout before measuring.
    if(Target.Get()!=Label)
    {
        Restore();Label->TakeWidget();Target=Label;
        OriginalTable.Reset(Label->GetTextStyleSet());OriginalStyle=Label->GetCurrentDefaultTextStyle();
        HadStyleOverride=FindFProperty<FBoolProperty>(Label->GetClass(),TEXT("bOverrideDefaultStyle"))->GetPropertyValue_InContainer(Label);
    }
    const FString Text=CompleteText?*CompleteText:Label->GetText().ToString();
    if(LastText==Text && LastSize.Equals(Available,1.f) && LastAllowPages==AllowPages)return;
    auto* Measure=DuplicateObject<URichTextBlock>(Label,GetTransientPackage());
    Measure->SetVisibility(ESlateVisibility::HitTestInvisible);
    FindFProperty<FFloatProperty>(Measure->GetClass(),TEXT("WrapTextAt"))->SetPropertyValue_InContainer(Measure,Available.X-4.f);
    Measure->SetAutoWrapText(false);Measure->SetText(FText::AsCultureInvariant(Text));
    // Scale named rich-text styles as well as untagged text, without changing the shared asset.
    TStrongObjectPtr<UDataTable> WorkingTable;
    if(OriginalTable.IsValid())WorkingTable.Reset(DuplicateObject<UDataTable>(OriginalTable.Get(),GetTransientPackage()));
    auto ApplySize=[&](URichTextBlock* Block,int32 Size){
        if(WorkingTable.IsValid() && WorkingTable->GetRowStruct()->IsChildOf(FRichTextStyleRow::StaticStruct()))
        {
            const float Ratio=float(Size)/FMath::Max(1,OriginalStyle.Font.Size);
            for(const auto& Pair:WorkingTable->GetRowMap())
                if(const auto* Original=OriginalTable->FindRow<FRichTextStyleRow>(Pair.Key,TEXT("SideDialogueFit"),false))
                    reinterpret_cast<FRichTextStyleRow*>(Pair.Value)->TextStyle.Font.Size=FMath::Max(1,FMath::RoundToInt(Original->TextStyle.Font.Size*Ratio));
            Block->SetTextStyleSet(nullptr);Block->SetTextStyleSet(WorkingTable.Get());
        }
        else Block->SetTextStyleSet(OriginalTable.Get());
        auto Style=OriginalStyle;Style.Font.Size=Size;Block->SetDefaultTextStyle(Style);
    };
    const int32 BaseSize=FMath::Max(1,FMath::RoundToInt(OriginalStyle.Font.Size));
    const int32 Minimum=AllowPages?FMath::Min(BaseSize,FMath::Max(14,FMath::RoundToInt(BaseSize*.75f))):1;
    int32 Size=BaseSize;
    TSharedPtr<SWidget> Slate;
    for(;;)
    {
        // Rebuild the measurement widget after each style step: cached rich-text runs
        // otherwise retain metrics from the previous font size.
        Slate.Reset();Measure->ReleaseSlateResources(true);ApplySize(Measure,Size);Slate=Measure->TakeWidget();Slate->SlatePrepass(1.f);
        const auto Desired=Slate->GetDesiredSize();
        if((Desired.X<=Available.X-2.f && Desired.Y<=Available.Y-2.f) || Size<=Minimum)break;
        --Size;
    }
    Slate.Reset();Measure->ReleaseSlateResources(true);ApplySize(Label,Size);Label->RefreshTextLayout();
    // Split using the final applied style, including each independent page's reopened tags.
    // A fresh layout per probe avoids carrying a preceding candidate's rich-text run metrics.
    Pages=AllowPages?VTGDialoguePagination::Split(Text,[&](const FString& Candidate){
        auto* Probe=DuplicateObject<URichTextBlock>(Label,GetTransientPackage());
        FindFProperty<FFloatProperty>(Probe->GetClass(),TEXT("WrapTextAt"))->SetPropertyValue_InContainer(Probe,Available.X-4.f);
        Probe->SetAutoWrapText(false);Probe->SetText(FText::AsCultureInvariant(Candidate));
        auto ProbeSlate=Probe->TakeWidget();ProbeSlate->SlatePrepass(1.f);
        const auto Desired=ProbeSlate->GetDesiredSize();Probe->ReleaseSlateResources(true);
        return Desired.X<=Available.X-2.f && Desired.Y<=Available.Y-2.f;
    },true):TArray<FString>{Text};
    LastText=Text;LastSize=Available;LastAllowPages=AllowPages;
}
