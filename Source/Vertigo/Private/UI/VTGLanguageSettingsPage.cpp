#include "UI/VTGLanguageSettingsPage.h"
#include "UI/VTGLocalizationSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "UObject/ConstructorHelpers.h"

namespace VTGLanguagePage
{
FSlateFontInfo Font(const TCHAR* Weight, int32 Size)
{
 return FSlateFontInfo(LoadObject<UFont>(nullptr, *(FString(TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-")) + Weight + TEXT("_Font"))), Size);
}
FText Text(const TCHAR* Value) { return FText::AsCultureInvariant(Value); }
FString LanguageName(const FString& Code)
{
 if (Code == TEXT("en-US")) return TEXT("English");
 if (Code == TEXT("zh-Hans")) return TEXT("简体中文");
 if (Code == TEXT("origin")) return TEXT("Origin (Dev)");
 auto Culture = FInternationalization::Get().GetCulture(Code);
 return Culture.IsValid() ? Culture->GetNativeName() : Code;
}
}

void UVTGLanguageComboBox::SetMenuOpen(bool bOpen)
{
 if (MyComboBox.IsValid()) MyComboBox->SetIsOpen(bOpen);
}

UVTGLanguageSettingsPage::UVTGLanguageSettingsPage(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
 static ConstructorHelpers::FObjectFinder<UFont> Regular(TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Regular_Font"));
 static ConstructorHelpers::FObjectFinder<UFont> Medium(TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Medium_Font"));
 static ConstructorHelpers::FObjectFinder<UFont> Semibold(TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-SemiBold_Font"));
 FontAssets={Regular.Object,Medium.Object,Semibold.Object};
}

void UVTGLanguageSettingsPage::Configure(UVTGLocalizationSubsystem* InLanguages, UTexture2D* Frame)
{
 Languages = InLanguages;
 if (!WidgetTree->RootWidget) BuildPage(Frame);
 RefreshLanguage(Languages ? Languages->GetLanguage() : TEXT("en-US"));
}

void UVTGLanguageSettingsPage::BuildPage(UTexture2D* Frame)
{
 using namespace VTGLanguagePage;
 auto* Root = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("SettingsScale"));
 Root->SetStretch(EStretch::ScaleToFit);
 Root->SetStretchDirection(EStretchDirection::DownOnly);
 WidgetTree->RootWidget = Root;
 auto* Size = WidgetTree->ConstructWidget<USizeBox>();
 Size->SetWidthOverride(940); Size->SetHeightOverride(800);
 Root->SetContent(Size);
 auto* ScaleSlot = CastChecked<UScaleBoxSlot>(Size->Slot);
 ScaleSlot->SetHorizontalAlignment(HAlign_Center); ScaleSlot->SetVerticalAlignment(VAlign_Center);
 auto* Panel = WidgetTree->ConstructWidget<UCanvasPanel>(); Size->SetContent(Panel);
 auto Place = [Panel](UWidget* Child, FVector2D Position, FVector2D Dimensions)
 {
  auto* Slot = Panel->AddChildToCanvas(Child); Slot->SetPosition(Position); Slot->SetSize(Dimensions);
 };
 auto* Art = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LanguageFrame"));
 Art->SetBrushFromTexture(Frame); Art->SetVisibility(ESlateVisibility::HitTestInvisible);
 Place(Art, FVector2D::ZeroVector, FVector2D(940,800));
 auto MakeText = [&](const TCHAR* Name, const TCHAR* Value, int32 SizeValue, const TCHAR* Weight, FVector2D Position, FVector2D Dimensions)
 {
  auto* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
  Label->SetText(Text(Value)); Label->SetFont(Font(Weight, SizeValue));
  Label->SetColorAndOpacity(FSlateColor(FLinearColor(.90f,.95f,.95f,1)));
  Label->SetAutoWrapText(true); Place(Label, Position, Dimensions); return Label;
 };
 auto* Breadcrumb = MakeText(TEXT("SettingsBreadcrumb"),TEXT("PAUSE / SETTINGS"),16,TEXT("Medium"),FVector2D(100,94),FVector2D(740,30));
 Breadcrumb->SetColorAndOpacity(FSlateColor(FLinearColor(.55f,.73f,.76f,1)));
 MakeText(TEXT("LanguageHeading"),TEXT("Language"),36,TEXT("SemiBold"),FVector2D(100,142),FVector2D(740,70));
 auto* Line = WidgetTree->ConstructWidget<UBorder>();
 Line->SetBrushColor(FLinearColor(.36f,.65f,.68f,.55f)); Line->SetVisibility(ESlateVisibility::HitTestInvisible);
 Place(Line,FVector2D(100,235),FVector2D(740,1));
 FieldLabel = MakeText(TEXT("DisplayLanguageLabel"),TEXT("Display Language"),24,TEXT("Medium"),FVector2D(100,280),FVector2D(740,46));
 Selection = WidgetTree->ConstructWidget<UVTGLanguageComboBox>(UVTGLanguageComboBox::StaticClass(),TEXT("LanguageSelection"));
 Selection->SetEnableGamepadNavigationMode(true);Selection->SetHasDownArrow(false);
 Selection->SetMaxListHeight(240); Selection->SetContentPadding(FMargin(16,12,60,12));
 auto Style = Selection->GetWidgetStyle();
 Style.ComboButtonStyle.ButtonStyle.SetNormal(FSlateNoResource());
 Style.ComboButtonStyle.ButtonStyle.SetDisabled(FSlateNoResource());
 Style.ComboButtonStyle.ButtonStyle.SetHovered(FSlateColorBrush(FLinearColor(.35f,.70f,.74f,.15f)));
 Style.ComboButtonStyle.ButtonStyle.SetPressed(FSlateColorBrush(FLinearColor(.35f,.70f,.74f,.24f)));
 Style.ComboButtonStyle.MenuBorderBrush = FSlateNoResource();
 Style.ComboButtonStyle.SetShadowOffset(FVector2D::ZeroVector);
 Style.ComboButtonStyle.MenuBorderPadding = FMargin(4);
 Style.ComboButtonStyle.DownArrowImage.TintColor = FSlateColor(FLinearColor(.75f,.9f,.9f,1));
 Selection->SetWidgetStyle(Style);
 auto ItemStyle = Selection->GetItemStyle();
 ItemStyle.SetEvenRowBackgroundBrush(FSlateNoResource());
 ItemStyle.SetOddRowBackgroundBrush(FSlateNoResource());
 ItemStyle.SetEvenRowBackgroundHoveredBrush(FSlateColorBrush(FLinearColor(.35f,.70f,.74f,.15f)));
 ItemStyle.SetOddRowBackgroundHoveredBrush(FSlateColorBrush(FLinearColor(.35f,.70f,.74f,.15f)));
 ItemStyle.SetActiveBrush(FSlateColorBrush(FLinearColor(.35f,.70f,.74f,.24f)));
 ItemStyle.SetInactiveBrush(FSlateColorBrush(FLinearColor(.35f,.70f,.74f,.15f)));
 ItemStyle.SetSelectorFocusedBrush(FSlateColorBrush(FLinearColor(.35f,.70f,.74f,.24f)));
 Selection->SetItemStyle(ItemStyle);
 Selection->OnGenerateWidgetEvent.BindDynamic(this,&ThisClass::GenerateOption);
 LanguageCodes = UVTGLocalizationSubsystem::GetLanguages();
 // Keep player-facing languages first; the original source text remains editor-only.
 if (LanguageCodes.Remove(TEXT("origin"))) LanguageCodes.Add(TEXT("origin"));
 for (const auto& Code : LanguageCodes) Selection->AddOption(LanguageName(Code));
 Selection->OnSelectionChanged.AddUniqueDynamic(this,&ThisClass::SelectLanguage);
 Place(Selection,FVector2D(100,340),FVector2D(740,68));
 // Two strokes avoid relying on a symbol glyph or an editor-only Slate brush.
 for(int32 StrokeIndex=0;StrokeIndex<2;++StrokeIndex)
 {
  auto* Stroke=WidgetTree->ConstructWidget<UBorder>();
  Stroke->SetBrushColor(FLinearColor(.75f,.9f,.9f,1));Stroke->SetVisibility(ESlateVisibility::HitTestInvisible);
  Place(Stroke,FVector2D(795+StrokeIndex*8,372),FVector2D(13,2));
  Stroke->SetRenderTransformAngle(StrokeIndex==0?45.f:-45.f);
 }
 HelpLabel = MakeText(TEXT("LanguageHelp"),TEXT("Changes apply immediately and are saved automatically."),18,TEXT("Regular"),FVector2D(100,434),FVector2D(740,76));
 HelpLabel->SetColorAndOpacity(FSlateColor(FLinearColor(.65f,.76f,.78f,1)));
 StatusLabel = MakeText(TEXT("LanguageStatus"),TEXT(""),18,TEXT("Regular"),FVector2D(100,525),FVector2D(740,60));
 StatusLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f,.59f,.42f,1)));
 BackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("LanguageBack"));
 auto ButtonStyle = Style.ComboButtonStyle.ButtonStyle;
 BackButton->SetStyle(ButtonStyle);
 BackLabel = WidgetTree->ConstructWidget<UTextBlock>();
 BackLabel->SetFont(Font(TEXT("Medium"),22)); BackLabel->SetColorAndOpacity(FSlateColor(FLinearColor(.9f,.95f,.95f,1)));
 BackLabel->SetJustification(ETextJustify::Center); BackButton->SetContent(BackLabel);
 BackButton->OnClicked.AddUniqueDynamic(this,&ThisClass::RequestBack);
 Place(BackButton,FVector2D(100,626),FVector2D(200,62));
 MakeText(TEXT("LanguageBackHint"),TEXT("Esc / B"),16,TEXT("Regular"),FVector2D(710,646),FVector2D(130,32))->SetJustification(ETextJustify::Right);
 Selection->SetNavigationRuleExplicit(EUINavigation::Down,BackButton);
 Selection->SetNavigationRuleExplicit(EUINavigation::Up,BackButton);
 BackButton->SetNavigationRuleExplicit(EUINavigation::Up,Selection);
 BackButton->SetNavigationRuleExplicit(EUINavigation::Down,Selection);
}

UWidget* UVTGLanguageSettingsPage::GenerateOption(FString Option)
{
 auto* OptionBorder = WidgetTree->ConstructWidget<UBorder>();
 OptionBorder->SetBrushColor(FLinearColor::Transparent); OptionBorder->SetPadding(FMargin(10,6));
 auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
 Label->SetFont(VTGLanguagePage::Font(TEXT("Regular"),24));
 Label->SetText(FText::AsCultureInvariant(Option));
 Label->SetColorAndOpacity(FSlateColor(FLinearColor(.9f,.95f,.95f,1)));
 OptionBorder->SetContent(Label); return OptionBorder;
}

void UVTGLanguageSettingsPage::NativeConstruct()
{
 Super::NativeConstruct();
 if (Languages)
 {
  Languages->OnLanguageChanged.AddUniqueDynamic(this,&ThisClass::RefreshLanguage);
  RefreshLanguage(Languages->GetLanguage());
 }
}

void UVTGLanguageSettingsPage::NativeDestruct()
{
 CloseDropdown();
 if (Languages) Languages->OnLanguageChanged.RemoveDynamic(this,&ThisClass::RefreshLanguage);
 Super::NativeDestruct();
}

void UVTGLanguageSettingsPage::SelectLanguage(FString SelectedItem, ESelectInfo::Type SelectionType)
{
 if (bSynchronizing || !Languages) return;
 // Arrow-key highlighting inside the open list must not change/save the language.
 if (SelectionType == ESelectInfo::OnNavigation) return;
 const int32 Index = Selection->FindOptionIndex(SelectedItem);
 if (!LanguageCodes.IsValidIndex(Index)) return;
 if (LanguageCodes[Index] == Languages->GetLanguage()) return;
 if (!Languages->SetLanguage(LanguageCodes[Index]))
 {
  RefreshLanguage(Languages->GetLanguage());
  StatusLabel->SetText(VTGLanguagePage::Text(Languages->GetLanguage()==TEXT("zh-Hans") ? TEXT("语言切换失败，请重试。") : TEXT("Could not change language. Please try again.")));
 }
}

void UVTGLanguageSettingsPage::RefreshLanguage(const FString& Language)
{
 if (!Selection) return;
 TGuardValue<bool> Guard(bSynchronizing,true);
 const int32 Index = LanguageCodes.IndexOfByKey(Language);
 if (Index != INDEX_NONE && Selection->GetSelectedIndex()!=Index) Selection->SetSelectedIndex(Index);
 const bool Chinese = Language==TEXT("zh-Hans");
 FieldLabel->SetText(VTGLanguagePage::Text(Chinese ? TEXT("显示语言") : TEXT("Display Language")));
 HelpLabel->SetText(VTGLanguagePage::Text(Chinese ? TEXT("选择后立即生效并自动保存。") : TEXT("Changes apply immediately and are saved automatically.")));
 BackLabel->SetText(VTGLanguagePage::Text(Chinese ? TEXT("返回") : TEXT("Back")));
 Selection->SetToolTipText(FieldLabel->GetText());
 StatusLabel->SetText(FText::GetEmpty());
}

void UVTGLanguageSettingsPage::FocusSelection() { if (Selection) Selection->SetKeyboardFocus(); }
bool UVTGLanguageSettingsPage::CloseDropdown()
{
 if (!Selection || !Selection->IsOpen()) return false;
 Selection->SetMenuOpen(false);
 // Restore any uncommitted navigation highlight when cancelling the popup.
 if (Languages) RefreshLanguage(Languages->GetLanguage());
 FocusSelection(); return true;
}
void UVTGLanguageSettingsPage::RequestBack() { CloseDropdown(); OnBackRequested.Broadcast(); }
