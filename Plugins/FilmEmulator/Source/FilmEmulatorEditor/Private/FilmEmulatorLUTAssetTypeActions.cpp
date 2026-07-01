// Copyright 2026 TOXIC STOCK All rights reserved.

#include "FilmEmulatorLUTAssetTypeActions.h"

#include "FilmEmulatorLUT.h"

#define LOCTEXT_NAMESPACE "FilmEmulatorLUTAsset"

FText FFilmEmulatorLUTAssetTypeActions::GetName() const
{
    return LOCTEXT("FilmEmulatorLUTAssetName", "Film Emulator LUT");
}

FColor FFilmEmulatorLUTAssetTypeActions::GetTypeColor() const
{
    return FColor(255, 190, 90);
}

UClass* FFilmEmulatorLUTAssetTypeActions::GetSupportedClass() const
{
    return UFilmEmulatorLUT::StaticClass();
}

uint32 FFilmEmulatorLUTAssetTypeActions::GetCategories()
{
    return EAssetTypeCategories::Textures;
}

#undef LOCTEXT_NAMESPACE
