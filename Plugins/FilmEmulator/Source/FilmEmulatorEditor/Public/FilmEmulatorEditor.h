// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleInterface.h"
#include "WorkspaceMenuStructureModule.h"

#include "SlateBasics.h"

class SDockTab;
class FSpawnTabArgs;
class IAssetTypeActions;

class FILMEMULATOREDITOR_API FFilmEmulatorEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterTabSpawner();
    void UnregisterTabSpawner();
    TSharedRef<SDockTab> OnSpawnFilmEmulatorTab(const FSpawnTabArgs& SpawnTabArgs);

    void RegisterAssetTypeActions();
    void UnregisterAssetTypeActions();
    void RegisterDetailCustomizations();
    void UnregisterDetailCustomizations();
    void AutoImportAssets();
    void AutoImportLUTAssets();
    void AutoImportPresetAssets();


    FName TabName = TEXT("FilmEmulator");
    TArray<TSharedPtr<IAssetTypeActions>> RegisteredAssetActions;
    FDelegateHandle PostEngineInitHandle;
};
