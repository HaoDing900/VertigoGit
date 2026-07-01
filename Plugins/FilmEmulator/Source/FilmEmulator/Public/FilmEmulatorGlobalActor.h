// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FilmEmulatorSettings.h"
#include "FilmStockPreset.h"
#include "GameFramework/Actor.h"
#include "FilmEmulatorGlobalActor.generated.h"

UCLASS(hidecategories = (Collision, Volume, Brush, Attachment))
class FILMEMULATOR_API AFilmEmulatorGlobalActor : public AActor
{
    GENERATED_BODY()

public:
    AFilmEmulatorGlobalActor();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Film Emulator")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Film Emulator")
    bool bHighPriority = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Film Emulator|Output", meta = (DisplayName = "Apply After Tonemapper"))
    bool bApplyAfterTonemapper = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Film Emulator")
    FFilmEmulatorParams Params;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Film Emulator|Presets", meta = (EditCondition = "!bUseOverridePreset"))
    TSoftObjectPtr<UFilmStockPreset> PresetAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Film Emulator|Presets", meta = (DisplayName = "Preset Id (JSON)", EditCondition = "!bUseOverridePreset"))
    FName PresetId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Film Emulator|Presets")
    bool bUseOverridePreset = false;

    UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Film Emulator|Presets", meta = (EditCondition = "bUseOverridePreset"))
    TObjectPtr<UFilmStockPresetInline> OverridePreset;

    UFUNCTION(CallInEditor, Category = "Film Emulator|Presets")
    void SyncOverrideFromPreset();

    UFilmStockPreset* GetEffectivePreset() const;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    static AFilmEmulatorGlobalActor* FindHighPriorityActor(UWorld* World);

private:
    UFilmStockPreset* ResolvePresetAsset() const;
    void EnsureOverridePreset(bool bCopyFromAsset);
    static void CopyPresetData(const UFilmStockPreset* Source, UFilmStockPreset* Dest);
};
