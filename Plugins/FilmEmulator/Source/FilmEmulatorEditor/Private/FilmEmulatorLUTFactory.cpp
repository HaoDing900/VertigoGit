// Copyright 2026 TOXIC STOCK All rights reserved.

#include "FilmEmulatorLUTFactory.h"

#include "FilmEmulatorLUT.h"
#include "FilmEmulatorLUTUtils.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

UFilmEmulatorLUTFactory::UFilmEmulatorLUTFactory()
{
    bEditorImport = true;
    bCreateNew = false;
    SupportedClass = UFilmEmulatorLUT::StaticClass();
    Formats.Add(TEXT("cube;Film Emulator LUT"));
    ImportPriority = DefaultImportPriority + 20;
}

bool UFilmEmulatorLUTFactory::FactoryCanImport(const FString& Filename)
{
    return FPaths::GetExtension(Filename).Equals(TEXT("cube"), ESearchCase::IgnoreCase);
}

UObject* UFilmEmulatorLUTFactory::FactoryCreateFile(
    UClass* InClass,
    UObject* InParent,
    FName InName,
    EObjectFlags Flags,
    const FString& Filename,
    const TCHAR* Parms,
    FFeedbackContext* Warn,
    bool& bOutOperationCanceled)
{
    bOutOperationCanceled = false;

    UFilmEmulatorLUT* LutAsset = NewObject<UFilmEmulatorLUT>(InParent, InClass, InName, Flags);
    if (!LutAsset)
    {
        return nullptr;
    }

    if (!ImportCubeFile(Filename, LutAsset, Warn))
    {
        return nullptr;
    }

#if WITH_EDITORONLY_DATA
    if (LutAsset->AssetImportData)
    {
        LutAsset->AssetImportData->Update(Filename);
    }
#endif

    return LutAsset;
}

bool UFilmEmulatorLUTFactory::ImportCubeFile(const FString& Filename, UFilmEmulatorLUT* LutAsset, FFeedbackContext* Warn)
{
    if (!LutAsset)
    {
        return false;
    }

    int32 Size = 0;
    TArray<FVector3f> Samples;
    FVector3f DomainMin;
    FVector3f DomainMax;
    if (!FilmEmulatorLUTUtils::ParseCubeFile(Filename, Size, Samples, DomainMin, DomainMax))
    {
        return false;
    }

    LutAsset->SetFromSamples(Size, Samples, DomainMin, DomainMax);
    return true;
}

bool UFilmEmulatorLUTFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
    UFilmEmulatorLUT* LutAsset = Cast<UFilmEmulatorLUT>(Obj);
    if (!LutAsset)
    {
        return false;
    }

#if WITH_EDITORONLY_DATA
    if (LutAsset->AssetImportData)
    {
        LutAsset->AssetImportData->ExtractFilenames(OutFilenames);
        return OutFilenames.Num() > 0;
    }
#endif

    return false;
}

void UFilmEmulatorLUTFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
    UFilmEmulatorLUT* LutAsset = Cast<UFilmEmulatorLUT>(Obj);
    if (!LutAsset || NewReimportPaths.Num() == 0)
    {
        return;
    }

#if WITH_EDITORONLY_DATA
    if (LutAsset->AssetImportData)
    {
        LutAsset->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
    }
#endif
}

EReimportResult::Type UFilmEmulatorLUTFactory::Reimport(UObject* Obj)
{
    UFilmEmulatorLUT* LutAsset = Cast<UFilmEmulatorLUT>(Obj);
    if (!LutAsset)
    {
        return EReimportResult::Failed;
    }

#if WITH_EDITORONLY_DATA
    const FString Filename = LutAsset->AssetImportData ? LutAsset->AssetImportData->GetFirstFilename() : FString();
    if (Filename.IsEmpty() || !FPaths::FileExists(Filename))
    {
        return EReimportResult::Failed;
    }

    if (!ImportCubeFile(Filename, LutAsset, GWarn))
    {
        return EReimportResult::Failed;
    }

    if (LutAsset->AssetImportData)
    {
        LutAsset->AssetImportData->Update(Filename);
    }

    LutAsset->MarkPackageDirty();
    return EReimportResult::Succeeded;
#else
    return EReimportResult::Failed;
#endif
}
