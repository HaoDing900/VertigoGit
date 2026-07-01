// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UFilmStockPreset;

struct FILMEMULATOR_API FFilmPresetEntry
{
    FName PresetId;
    FString SourcePath;
    TStrongObjectPtr<UFilmStockPreset> Preset;
};

class FILMEMULATOR_API FFilmEmulatorPresetLibrary
{
public:
    static FFilmEmulatorPresetLibrary& Get();

    void Reload();

    const TArray<FFilmPresetEntry>& GetPresets();
    UFilmStockPreset* FindPresetById(FName PresetId);
    UFilmStockPreset* GetDefaultPreset();

private:
    void LoadIfNeeded();
    void LoadPresets();

    bool bLoaded = false;
    FName LoadedDefaultPresetId = NAME_None;
    TArray<FFilmPresetEntry> Presets;
};
