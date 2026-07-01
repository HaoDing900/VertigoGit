// Copyright 2026 TOXIC STOCK All rights reserved.

#include "FilmEmulatorSettings.h"

#include "FilmEmulatorColorProfiles.h"

UFilmEmulatorSettings::UFilmEmulatorSettings()
{
    FilmProfiles.Reset();
    FilmPrintProfiles.Reset();
    DefaultFilmProfileId = NAME_None;
    DefaultFilmPrintProfileId = NAME_None;
    DefaultParams.FilmProfileId = NAME_None;
    DefaultParams.FilmPrintProfileId = NAME_None;

    auto AddPrintProfile = [this](FName ProfileId, const TCHAR* DisplayName, const TCHAR* Description, const TCHAR* LutPath)
    {
        FFilmPrintProfile Profile;
        Profile.ProfileId = ProfileId;
        Profile.DisplayName = FText::FromString(DisplayName);
        Profile.Description = FText::FromString(Description);
        Profile.PrintLUTPath.FilePath = LutPath;
        FilmPrintProfiles.Add(MoveTemp(Profile));
    };

    if (FilmPrintProfiles.Num() == 0)
    {
        AddPrintProfile(TEXT("KodakVision2383"), TEXT("Kodak VISION 2383/3383"), TEXT("Classic motion picture color print film."), TEXT("Content/LUTs/FE_Kodak_2383_64.cube"));
        AddPrintProfile(TEXT("KodakVisionPremier2393"), TEXT("Kodak VISION Premier 2393/3393"), TEXT("High-density cinema print look with deeper blacks."), TEXT("Content/LUTs/FE_Kodak_2393_64.cube"));
        AddPrintProfile(TEXT("Kodak2302"), TEXT("Kodak B&W Print 2302/3302"), TEXT("Black-and-white release print film."), TEXT("Content/LUTs/FE_Kodak_2302_64.cube"));
        AddPrintProfile(TEXT("FujiEternaCP3513DI"), TEXT("Fuji Eterna CP 3513DI"), TEXT("Fuji Eterna color print film (3513DI)."), TEXT("Content/LUTs/FE_Fuji_3513DI_64.cube"));
        AddPrintProfile(TEXT("FujiEternaCP3523XD"), TEXT("Fuji Eterna CP 3523XD"), TEXT("Fuji Eterna color print film (3523XD)."), TEXT("Content/LUTs/FE_Fuji_3523XD_64.cube"));
    }

    if (DefaultFilmPrintProfileId.IsNone() && FilmPrintProfiles.Num() > 0)
    {
        DefaultFilmPrintProfileId = FilmPrintProfiles[0].ProfileId;
    }
}

void UFilmEmulatorSettings::SaveSettingsConfig()
{
    SaveConfig();
}

void UFilmEmulatorSettings::ReloadSettingsConfig()
{
    ReloadConfig();
}

#if WITH_EDITOR
void UFilmEmulatorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    SaveConfig();
}
#endif

const FFilmColorProfile* UFilmEmulatorSettings::FindFilmProfile(FName ProfileId) const
{
    for (const FFilmColorProfile& Profile : FilmProfiles)
    {
        if (Profile.ProfileId == ProfileId)
        {
            return &Profile;
        }
    }
    return nullptr;
}

const FFilmColorProfile& UFilmEmulatorSettings::GetDefaultFilmProfile() const
{
    if (const FFilmColorProfile* Profile = FindFilmProfile(DefaultFilmProfileId))
    {
        return *Profile;
    }

    static const FFilmColorProfile Dummy;
    return Dummy;
}

const FFilmPrintProfile* UFilmEmulatorSettings::FindFilmPrintProfile(FName ProfileId) const
{
    for (const FFilmPrintProfile& Profile : FilmPrintProfiles)
    {
        if (Profile.ProfileId == ProfileId)
        {
            return &Profile;
        }
    }
    return nullptr;
}

const FFilmPrintProfile& UFilmEmulatorSettings::GetDefaultFilmPrintProfile() const
{
    if (const FFilmPrintProfile* Profile = FindFilmPrintProfile(DefaultFilmPrintProfileId))
    {
        return *Profile;
    }

    static const FFilmPrintProfile Dummy;
    return Dummy;
}
