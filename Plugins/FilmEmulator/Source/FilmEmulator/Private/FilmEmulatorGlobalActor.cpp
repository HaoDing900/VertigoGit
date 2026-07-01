// Copyright 2026 TOXIC STOCK All rights reserved.

#include "FilmEmulatorGlobalActor.h"
#include "FilmEmulatorPresetLibrary.h"
#include "EngineUtils.h"

AFilmEmulatorGlobalActor::AFilmEmulatorGlobalActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AFilmEmulatorGlobalActor::CopyPresetData(const UFilmStockPreset* Source, UFilmStockPreset* Dest)
{
    if (!Source || !Dest)
    {
        return;
    }

    Dest->DisplayName = Source->DisplayName;
    Dest->FilmType = Source->FilmType;
    Dest->FilmFormat = Source->FilmFormat;
    Dest->FilmFormatScale = Source->FilmFormatScale;
    Dest->Description = Source->Description;
    Dest->FilmLUTAsset = Source->FilmLUTAsset;
    Dest->FilmLUT = Source->FilmLUT;
    Dest->FilmLUTPath = Source->FilmLUTPath;
    Dest->PrintStrength = Source->PrintStrength;
    Dest->FilmPrintLUTAsset = Source->FilmPrintLUTAsset;
    Dest->FilmPrintLUT = Source->FilmPrintLUT;
    Dest->FilmPrintLUTPath = Source->FilmPrintLUTPath;
    Dest->SaturationBias = Source->SaturationBias;
    Dest->ContrastBias = Source->ContrastBias;
    Dest->ExposureBias = Source->ExposureBias;
    Dest->Grain = Source->Grain;
    Dest->GrainDefaults = Source->GrainDefaults;
    Dest->Halation = Source->Halation;
    Dest->GateWeave = Source->GateWeave;
    Dest->Flicker = Source->Flicker;
    Dest->GateScratch = Source->GateScratch;
    Dest->Dirt = Source->Dirt;
}

void AFilmEmulatorGlobalActor::EnsureOverridePreset(bool bCopyFromAsset)
{
    if (!OverridePreset)
    {
        OverridePreset = NewObject<UFilmStockPresetInline>(this, NAME_None, RF_Transactional);
    }

    if (bCopyFromAsset)
    {
        if (UFilmStockPreset* Source = ResolvePresetAsset())
        {
            CopyPresetData(Source, OverridePreset);
        }
    }
}

void AFilmEmulatorGlobalActor::SyncOverrideFromPreset()
{
    EnsureOverridePreset(true);
}

UFilmStockPreset* AFilmEmulatorGlobalActor::ResolvePresetAsset() const
{
    if (PresetAsset.IsValid())
    {
        return PresetAsset.Get();
    }
    if (PresetAsset.ToSoftObjectPath().IsValid())
    {
        return PresetAsset.LoadSynchronous();
    }
    if (PresetId != NAME_None)
    {
        return FFilmEmulatorPresetLibrary::Get().FindPresetById(PresetId);
    }
    return nullptr;
}

UFilmStockPreset* AFilmEmulatorGlobalActor::GetEffectivePreset() const
{
    if (bUseOverridePreset && OverridePreset)
    {
        return OverridePreset;
    }
    return ResolvePresetAsset();
}

AFilmEmulatorGlobalActor* AFilmEmulatorGlobalActor::FindHighPriorityActor(UWorld* World)
{
    if (!World)
    {
        return nullptr;
    }

    for (TActorIterator<AFilmEmulatorGlobalActor> It(World); It; ++It)
    {
        AFilmEmulatorGlobalActor* Actor = *It;
        if (IsValid(Actor) && Actor->bEnabled && Actor->bHighPriority)
        {
            return Actor;
        }
    }
    return nullptr;
}

#if WITH_EDITOR
void AFilmEmulatorGlobalActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    if (PropertyName == GET_MEMBER_NAME_CHECKED(AFilmEmulatorGlobalActor, bUseOverridePreset))
    {
        if (bUseOverridePreset)
        {
            EnsureOverridePreset(true);
        }
    }
}
#endif

