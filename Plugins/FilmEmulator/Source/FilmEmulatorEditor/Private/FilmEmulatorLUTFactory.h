// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "Factories/Factory.h"
#include "EditorReimportHandler.h"
#include "FilmEmulatorLUTFactory.generated.h"

class UFilmEmulatorLUT;

UCLASS()
class UFilmEmulatorLUTFactory : public UFactory, public FReimportHandler
{
    GENERATED_BODY()

public:
    UFilmEmulatorLUTFactory();

    virtual UObject* FactoryCreateFile(
        UClass* InClass,
        UObject* InParent,
        FName InName,
        EObjectFlags Flags,
        const FString& Filename,
        const TCHAR* Parms,
        FFeedbackContext* Warn,
        bool& bOutOperationCanceled) override;

    virtual bool FactoryCanImport(const FString& Filename) override;

    // FReimportHandler
    virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
    virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
    virtual EReimportResult::Type Reimport(UObject* Obj) override;
    virtual int32 GetPriority() const override { return ImportPriority; }

private:
    bool ImportCubeFile(const FString& Filename, UFilmEmulatorLUT* LutAsset, FFeedbackContext* Warn);
};
