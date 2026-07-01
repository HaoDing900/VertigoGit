// Copyright 2026 TOXIC STOCK All rights reserved.

#include "FilmEmulatorStyle.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Fonts/CompositeFont.h"
#include "Interfaces/IPluginManager.h"
#include "Slate/SlateGameResources.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FSlateStyleSet> FFilmEmulatorStyle::StyleInstance = nullptr;

void FFilmEmulatorStyle::Initialize()
{
    if (!StyleInstance.IsValid())
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
    }
}

void FFilmEmulatorStyle::Shutdown()
{
    if (StyleInstance.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
        ensure(StyleInstance.IsUnique());
        StyleInstance.Reset();
    }
}

const ISlateStyle& FFilmEmulatorStyle::Get()
{
    return *StyleInstance;
}

FName FFilmEmulatorStyle::GetStyleSetName()
{
    static const FName StyleSetName(TEXT("FilmEmulatorStyleSet"));
    return StyleSetName;
}

TSharedRef<FSlateStyleSet> FFilmEmulatorStyle::Create()
{
    const FString ResourcesDir = IPluginManager::Get().FindPlugin(TEXT("FilmEmulator"))->GetBaseDir() / TEXT("Resources");
    const FString FontsDir = ResourcesDir / TEXT("Fonts");

    TSharedRef<FSlateStyleSet> Style = FSlateGameResources::New(GetStyleSetName(), ResourcesDir, ResourcesDir);

    const FLinearColor TXCGold(0.910f, 0.773f, 0.278f, 1.0f);
    const FLinearColor TXCDarker(0.020f, 0.020f, 0.016f, 1.0f);
    const FLinearColor TXCCard(0.078f, 0.078f, 0.063f, 1.0f);
    const FLinearColor TXCTextDim(0.439f, 0.439f, 0.439f, 1.0f);

    Style->Set("FilmEmulator.Brush.Background", new FSlateColorBrush(TXCDarker));
    Style->Set("FilmEmulator.Brush.Section", new FSlateColorBrush(TXCCard));
    Style->Set("FilmEmulator.Brush.Header", new FSlateColorBrush(TXCDarker));

    Style->Set("FilmEmulator.Color.Accent", TXCGold);
    Style->Set("FilmEmulator.Color.TextDim", TXCTextDim);
    Style->Set("FilmEmulator.Color.Panel", TXCCard);

    Style->Set("FilmEmulator.Icon.Tab", new FSlateImageBrush(ResourcesDir / TEXT("Icon16.png"), FVector2D(16.f, 16.f)));
    Style->Set("FilmEmulator.Icon.Toolbar", new FSlateImageBrush(ResourcesDir / TEXT("Icon40.png"), FVector2D(40.f, 40.f)));

    const FString RegularFontPath = FontsDir / TEXT("TXCFont_Monoline-Regular.otf");
    const FString BoldFontPath = FontsDir / TEXT("TXCFont_Monoline-Bold.otf");

    auto MakeFontCF = [](const FString& FilePath) -> TSharedRef<FStandaloneCompositeFont>
    {
        TSharedRef<FStandaloneCompositeFont> CF = MakeShared<FStandaloneCompositeFont>();
        FTypefaceEntry& Entry = CF->DefaultTypeface.Fonts.AddDefaulted_GetRef();
        Entry.Name = TEXT("Default");
        Entry.Font = FFontData(FilePath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
        return CF;
    };

    TSharedRef<FStandaloneCompositeFont> RegularCF = MakeFontCF(RegularFontPath);
    TSharedRef<FStandaloneCompositeFont> BoldCF = MakeFontCF(BoldFontPath);

    Style->Set("FilmEmulator.Font.Regular", FSlateFontInfo(RegularCF, 11));
    Style->Set("FilmEmulator.Font.Small", FSlateFontInfo(RegularCF, 10));
    Style->Set("FilmEmulator.Font.Bold", FSlateFontInfo(BoldCF, 11));
    Style->Set("FilmEmulator.Font.Header", FSlateFontInfo(BoldCF, 12));
    Style->Set("FilmEmulator.Font.Title", FSlateFontInfo(BoldCF, 15));

    return Style;
}
