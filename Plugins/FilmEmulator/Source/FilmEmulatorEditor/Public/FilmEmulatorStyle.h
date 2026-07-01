// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Textures/SlateIcon.h"

class FILMEMULATOREDITOR_API FFilmEmulatorStyle
{
public:
    static void Initialize();
    static void Shutdown();
    static const ISlateStyle& Get();
    static FName GetStyleSetName();

private:
    static TSharedRef<FSlateStyleSet> Create();
    static TSharedPtr<FSlateStyleSet> StyleInstance;
};
