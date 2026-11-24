#include "UI/VTGInfoInGamePage.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "UI/VTGLocalizationSubsystem.h"

namespace VTGInfoPage
{
/** The pause menu's sub-page type, so the two read as one screen. */
FSlateFontInfo Font(const TCHAR* Weight, int32 Size)
{
	return FSlateFontInfo(LoadObject<UFont>(nullptr, *(FString(TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-")) + Weight + TEXT("_Font"))), Size);
}
const FLinearColor Ink(.90f, .95f, .95f, 1.f);
const FLinearColor Accent(.55f, .73f, .76f, 1.f);
/** WBP_Action's hint metrics: a 45x45 glyph and an 18pt Medium label. */
constexpr float KeyIconSize = 45.f;
constexpr int32 HintFontSize = 18;

bool FindPage(FName PageId, FVTGInfoPageRow& OutRow)
{
	const auto* Settings = GetDefault<UVTGInfoPageSettings>();
	UDataTable* Table = Settings ? Settings->Pages.LoadSynchronous() : nullptr;
	if (!Table || PageId.IsNone())
	{
		return false;
	}
	if (const auto* Row = Table->FindRow<FVTGInfoPageRow>(PageId, TEXT("VTGInfoPage"), /*bWarnIfMissing=*/false))
	{
		OutRow = *Row;
		return true;
	}
	UE_LOG(LogTemp, Warning, TEXT("[VTGInfoPage] %s has no page row \"%s\"."), *Table->GetName(), *PageId.ToString());
	return false;
}

void CollectLines(FName PageId, TArray<FVTGInfoPageLineRow>& OutLines)
{
	const auto* Settings = GetDefault<UVTGInfoPageSettings>();
	UDataTable* Table = Settings ? Settings->Lines.LoadSynchronous() : nullptr;
	if (!Table)
	{
		return;
	}
	for (const auto& Pair : Table->GetRowMap())
	{
		const auto* Row = reinterpret_cast<const FVTGInfoPageLineRow*>(Pair.Value);
		if (Row && Row->Page == PageId)
		{
			OutLines.Add(*Row);
		}
	}
	OutLines.Sort([](const FVTGInfoPageLineRow& A, const FVTGInfoPageLineRow& B) { return A.Order < B.Order; });
}

/** The same short names WBP_Action prints, so a keyboard prompt reads alike on both. */
FText ShortKeyName(const FKey& Key)
{
	if (!Key.IsValid())
	{
		return FText::FromString(TEXT("-"));
	}
	if (Key == EKeys::LeftMouseButton) return FText::FromString(TEXT("LMB"));
	if (Key == EKeys::RightMouseButton) return FText::FromString(TEXT("RMB"));
	if (Key == EKeys::MiddleMouseButton) return FText::FromString(TEXT("MMB"));
	if (Key == EKeys::LeftControl || Key == EKeys::RightControl) return FText::FromString(TEXT("Ctrl"));
	if (Key == EKeys::LeftShift || Key == EKeys::RightShift) return FText::FromString(TEXT("Shift"));
	if (Key == EKeys::Gamepad_RightTrigger || Key == EKeys::Gamepad_RightTriggerAxis) return FText::FromString(TEXT("RT"));
	if (Key == EKeys::Gamepad_LeftTrigger || Key == EKeys::Gamepad_LeftTriggerAxis) return FText::FromString(TEXT("LT"));
	if (Key == EKeys::Gamepad_RightShoulder) return FText::FromString(TEXT("RB"));
	if (Key == EKeys::Gamepad_LeftShoulder) return FText::FromString(TEXT("LB"));
	return Key.GetDisplayName();
}
}

void UVTGInfoInGamePage::SetInfoPageId(FName PageId)
{
	if (InfoPageId == PageId)
	{
		return;
	}
	InfoPageId = PageId;
	Refresh();
}

UVTGInfoInGamePage* UVTGInfoInGamePage::ShowInfoPage(const UObject* WorldContextObject, FName PageId, int32 ZOrder)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (!PC)
	{
		return nullptr;
	}
	// One asset serves every page; the id is what differs, so a caller never names a widget class.
	UClass* PageClass = LoadClass<UVTGInfoInGamePage>(nullptr, TEXT("/Game/Widget/InfoPage/WBP_InfoInGame.WBP_InfoInGame_C"));
	if (!PageClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VTGInfoPage] WBP_InfoInGame is missing."));
		return nullptr;
	}
	auto* Page = CreateWidget<UVTGInfoInGamePage>(PC, PageClass);
	if (!Page)
	{
		return nullptr;
	}
	Page->InfoPageId = PageId;
	Page->AddToViewport(ZOrder);
	return Page;
}

void UVTGInfoInGamePage::NativePreConstruct()
{
	Super::NativePreConstruct();
	// Fills the designer preview too, so the page is laid out against its real wording.
	Refresh();
}

void UVTGInfoInGamePage::NativeConstruct()
{
	Super::NativeConstruct();
	if (auto* GameInstance = GetGameInstance())
	{
		Languages = GameInstance->GetSubsystem<UVTGLocalizationSubsystem>();
	}
	if (Languages)
	{
		Languages->OnLanguageChanged.AddUniqueDynamic(this, &ThisClass::HandleLanguageChanged);
	}
	Refresh();
}

void UVTGInfoInGamePage::NativeDestruct()
{
	if (Languages)
	{
		Languages->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
	}
	Super::NativeDestruct();
}

void UVTGInfoInGamePage::HandleLanguageChanged(const FString& Language)
{
	Refresh();
}

void UVTGInfoInGamePage::Refresh()
{
	FVTGInfoPageRow Page;
	const bool bFound = VTGInfoPage::FindPage(InfoPageId, Page);

	auto Fill = [this](const TCHAR* Name, const FText& Value, bool bHideWhenEmpty)
	{
		auto* Label = Cast<UTextBlock>(GetWidgetFromName(Name));
		if (!Label)
		{
			return;
		}
		Label->SetText(Value);
		if (bHideWhenEmpty)
		{
			Label->SetVisibility(Value.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	};
	Fill(TEXT("InfoBreadcrumb"), Page.Breadcrumb, /*bHideWhenEmpty=*/true);
	Fill(TEXT("TutorialName"), Page.Title, /*bHideWhenEmpty=*/true);
	Fill(TEXT("TextBlock_115"), Page.Body, /*bHideWhenEmpty=*/true);
	Fill(TEXT("TextBlock_135"), Page.DismissLabel, /*bHideWhenEmpty=*/false);
	if (!bFound)
	{
		// The frame and the close button stay up: a missing row must not trap the player behind a
		// page with nothing to press.
		UE_LOG(LogTemp, Warning, TEXT("[VTGInfoPage] %s showing unknown page \"%s\"."), *GetName(), *InfoPageId.ToString());
	}
	BuildLines();
}

void UVTGInfoInGamePage::BuildLines()
{
	LineBox = Cast<UPanelWidget>(GetWidgetFromName(TEXT("InfoHints")));
	if (!LineBox || !WidgetTree)
	{
		return;
	}
	LineBox->ClearChildren();

	TArray<FVTGInfoPageLineRow> Lines;
	VTGInfoPage::CollectLines(InfoPageId, Lines);
	LineBox->SetVisibility(Lines.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		const FVTGInfoPageLineRow& Line = Lines[Index];
		auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
			*FString::Printf(TEXT("InfoHintRow_%d"), Index));

		// A fixed box for the glyph keeps every label on one left edge, whether the prompt came back
		// as a picture or as text.
		auto* KeySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			*FString::Printf(TEXT("InfoHintKeyBox_%d"), Index));
		KeySize->SetWidthOverride(VTGInfoPage::KeyIconSize);
		KeySize->SetHeightOverride(VTGInfoPage::KeyIconSize);
		auto* KeyStack = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
			*FString::Printf(TEXT("InfoHintKeyStack_%d"), Index));
		auto* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			*FString::Printf(TEXT("InfoHintKeyIcon_%d"), Index));
		auto* KeyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			*FString::Printf(TEXT("InfoHintKeyText_%d"), Index));
		KeyText->SetFont(VTGInfoPage::Font(TEXT("Medium"), VTGInfoPage::HintFontSize));
		KeyText->SetColorAndOpacity(FSlateColor(VTGInfoPage::Accent));
		KeyText->SetJustification(ETextJustify::Center);
		ApplyKeyPrompt(Icon, KeyText, Line);
		if (auto* IconSlot = Cast<UHorizontalBoxSlot>(KeyStack->AddChild(Icon)))
		{
			IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (auto* TextSlot = Cast<UHorizontalBoxSlot>(KeyStack->AddChild(KeyText)))
		{
			TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TextSlot->SetVerticalAlignment(VAlign_Center);
		}
		KeySize->AddChild(KeyStack);

		auto* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			*FString::Printf(TEXT("InfoHintLabel_%d"), Index));
		Label->SetText(Line.Label);
		Label->SetFont(VTGInfoPage::Font(TEXT("Medium"), VTGInfoPage::HintFontSize));
		Label->SetColorAndOpacity(FSlateColor(VTGInfoPage::Ink));
		Label->SetAutoWrapText(true);

		if (auto* KeySlot = Cast<UHorizontalBoxSlot>(Row->AddChild(KeySize)))
		{
			KeySlot->SetVerticalAlignment(VAlign_Center);
		}
		if (auto* LabelSlot = Cast<UHorizontalBoxSlot>(Row->AddChild(Label)))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
			LabelSlot->SetPadding(FMargin(16, 0, 0, 0));
		}
		if (auto* RowSlot = Cast<UVerticalBoxSlot>(LineBox->AddChild(Row)))
		{
			RowSlot->SetPadding(FMargin(0, Index == 0 ? 0.f : 14.f, 0, 0));
		}
	}
}

FKey UVTGInfoInGamePage::ResolveKey(const FVTGInfoPageLineRow& Line) const
{
	auto* Local = GetOwningLocalPlayer();
	auto* Input = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	auto* Device = Local ? Local->GetSubsystem<UCommonInputSubsystem>() : nullptr;
	const bool bGamepad = Device && Device->GetCurrentInputType() == ECommonInputType::Gamepad;
	if (const UInputAction* Action = Line.Action.LoadSynchronous())
	{
		if (Input)
		{
			for (const FKey& Key : Input->QueryKeysMappedToAction(Action))
			{
				if (Key.IsGamepadKey() == bGamepad)
				{
					return Key;
				}
			}
		}
		// In the designer there is no mapping to query, so the override is the authored fallback.
	}
	return Line.KeyOverride;
}

void UVTGInfoInGamePage::ApplyKeyPrompt(UImage* Icon, UTextBlock* Text, const FVTGInfoPageLineRow& Line) const
{
	const FKey Key = ResolveKey(Line);
	auto* Local = GetOwningLocalPlayer();
	auto* Device = Local ? Local->GetSubsystem<UCommonInputSubsystem>() : nullptr;
	FSlateBrush Brush;
	const bool bHasIcon = UCommonInputPlatformSettings::Get()->TryGetInputBrush(Brush, Key,
		Device ? Device->GetCurrentInputType() : ECommonInputType::MouseAndKeyboard,
		Device ? Device->GetCurrentGamepadName() : NAME_None);
	if (Icon)
	{
		if (bHasIcon)
		{
			Icon->SetBrush(Brush);
		}
		Icon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Text)
	{
		Text->SetText(VTGInfoPage::ShortKeyName(Key));
		Text->SetVisibility(bHasIcon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}
