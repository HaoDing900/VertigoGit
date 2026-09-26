#include "VTGDoorMaterialCommandlet.h"

#include "Components/StaticMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "K2Node_MakeArray.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace DoorMaterial
{
const TCHAR* AssetPath = TEXT("/Game/EnvArt/LevelPrototyping/BlockingStarterPack/Blueprints/AutomaticDoor/B_AutoLateralDoorDouble.B_AutoLateralDoorDouble");
const FName VariableName(TEXT("DoorMaterial"));

bool VerifyInstances(UBlueprint* Blueprint)
{
    auto* Property = FindFProperty<FObjectProperty>(Blueprint->GeneratedClass, VariableName);
    if (!Property || Property->PropertyClass != UMaterialInterface::StaticClass()
        || !Property->HasAnyPropertyFlags(CPF_Edit)
        || Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance)
        || Property->GetObjectPropertyValue_InContainer(Blueprint->GeneratedClass->GetDefaultObject()))
    {
        UE_LOG(LogTemp, Error, TEXT("DOOR MATERIAL: invalid instance-editable property/default"));
        return false;
    }

    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("DoorMaterialVerification"),
        nullptr, true, ERHIFeatureLevel::Num, &Init);
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* First = World->SpawnActor<AActor>(Blueprint->GeneratedClass, FTransform::Identity, Spawn);
    AActor* Second = World->SpawnActor<AActor>(Blueprint->GeneratedClass, FTransform(FVector(1000, 0, 0)), Spawn);
    if (!First || !Second) { World->DestroyWorld(false); return false; }

    // The inherited array identifies the two moving panels, excluding the frame.
    auto* ArrayProperty = FindFProperty<FArrayProperty>(Blueprint->GeneratedClass, TEXT("DoorsComponnent"));
    if (!ArrayProperty) { World->DestroyWorld(false); return false; }
    auto* Inner = CastField<FObjectPropertyBase>(ArrayProperty->Inner);
    if (!Inner) { World->DestroyWorld(false); return false; }
    auto Panels = [&](AActor* Actor)
    {
        TArray<UStaticMeshComponent*> Result;
        FScriptArrayHelper Array(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Actor));
        for (int32 Index = 0; Index < Array.Num(); ++Index)
            if (auto* Panel = Cast<UStaticMeshComponent>(Inner->GetObjectPropertyValue(Array.GetRawPtr(Index))))
                Result.Add(Panel);
        return Result;
    };
    auto* TestMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
    bool bValid = TestMaterial && Panels(First).Num() == 2 && Panels(Second).Num() == 2;
    TArray<FTransform> ClosedTransforms;
    for (auto* Panel : Panels(First)) ClosedTransforms.Add(Panel->GetRelativeTransform());
    auto Check = [&](AActor* Actor, UMaterialInterface* ExpectedOverride)
    {
        const auto Components = Panels(Actor);
        if (Components.Num() != 2) return false;
        for (int32 Index = 0; Index < Components.Num(); ++Index)
        {
            auto* Panel = Components[Index];
            if (!Panel->GetStaticMesh()) return false;
            auto* Expected = ExpectedOverride ? ExpectedOverride : Panel->GetStaticMesh()->GetMaterial(0);
            if (Panel->GetMaterial(0) != Expected || !Panel->GetRelativeTransform().Equals(ClosedTransforms[Index])) return false;
        }
        // Other components, including the frame, must still use their mesh materials.
        TArray<UStaticMeshComponent*> All;
        Actor->GetComponents(All);
        for (auto* Component : All)
            if (!Components.Contains(Component) && Component->GetStaticMesh()
                && Component->GetMaterial(0) != Component->GetStaticMesh()->GetMaterial(0)) return false;
        return true;
    };
    bValid = bValid && Check(First, nullptr) && Check(Second, nullptr);
    Property->SetObjectPropertyValue_InContainer(First, TestMaterial);
    First->RerunConstructionScripts();
    bValid = bValid && Check(First, TestMaterial) && Check(Second, nullptr);
    Property->SetObjectPropertyValue_InContainer(First, nullptr);
    First->RerunConstructionScripts();
    bValid = bValid && Check(First, nullptr) && Check(Second, nullptr);
    World->DestroyWorld(false);
    UE_LOG(LogTemp, Display, TEXT("DOOR MATERIAL instance tests (both panels, reset, isolation, transforms, frame): %s"), bValid ? TEXT("PASS") : TEXT("FAIL"));
    return bValid;
}
}

UVTGDoorMaterialCommandlet::UVTGDoorMaterialCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UVTGDoorMaterialCommandlet::Main(const FString& Params)
{
    using namespace DoorMaterial;
    auto* Blueprint = LoadObject<UBlueprint>(nullptr, AssetPath);
    if (!Blueprint) return 1;
    UE_LOG(LogTemp, Display, TEXT("DOOR MATERIAL resolved asset: %s"), *Blueprint->GetPathName());
    const bool bVerifyOnly = Params.Contains(TEXT("Verify"));
    const FString Filename = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    if (!bVerifyOnly)
    {
        // Preserve the user's current asset, including any prior uncommitted edits.
        const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("DoorMaterial");
        IFileManager::Get().MakeDirectory(*BackupDir, true);
        const FString Backup = BackupDir / TEXT("B_AutoLateralDoorDouble.before_material.uasset");
        const bool bHasBackup = IFileManager::Get().FileExists(*Backup);
        if ((bHasBackup && FMD5Hash::HashFile(*Backup) != FMD5Hash::HashFile(*Filename))
            || FindFProperty<FProperty>(Blueprint->GeneratedClass, VariableName))
        {
            UE_LOG(LogTemp, Error, TEXT("DOOR MATERIAL: backup or property already exists; use Verify for an installed change"));
            return 2;
        }
        UEdGraph* Graph = FBlueprintEditorUtils::FindUserConstructionScript(Blueprint);
        if (!Graph) return 3;
        UK2Node_MakeArray* Materials = nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
            if (auto* Array = Cast<UK2Node_MakeArray>(Node))
            {
                auto* Out = Array->GetOutputPin();
                if (Out && Out->LinkedTo.Num() == 2 && Out->LinkedTo[0]->PinName.ToString().StartsWith(TEXT("OverrideMaterials_"))
                    && Out->LinkedTo[1]->PinName.ToString().StartsWith(TEXT("OverrideMaterials_"))) Materials = Array;
            }
        if (!Materials || Materials->NumInputs != 0) return 4;
        if (!bHasBackup && IFileManager::Get().Copy(*Backup, *Filename, false) != COPY_OK) return 5;

        FEdGraphPinType Type;
        Type.PinCategory = UEdGraphSchema_K2::PC_Object;
        Type.PinSubCategoryObject = UMaterialInterface::StaticClass();
        if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, VariableName, Type)) return 6;
        FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(Blueprint, VariableName, false);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, VariableName, nullptr, FText::FromString(TEXT("Door Appearance")));
        FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, VariableName, nullptr, TEXT("DisplayName"), TEXT("Door Material"));
        FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, VariableName, nullptr, TEXT("ToolTip"), TEXT("Drag a Material or Material Instance here to override slot 0 on both moving door panels. Clear to restore the mesh materials. The frame is unchanged."));

        auto* Get = NewObject<UK2Node_VariableGet>(Graph);
        Graph->AddNode(Get, false, false);
        Get->CreateNewGuid();
        Get->VariableReference.SetSelfMember(VariableName);
        Get->NodePosX = Materials->NodePosX - 300;
        Get->NodePosY = Materials->NodePosY;
        Get->AllocateDefaultPins();
        Materials->AddInputPin();
        auto* From = Get->FindPin(VariableName);
        auto* To = Materials->FindPin(TEXT("[0]"));
        if (!From || !To || !GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(From, To)) return 7;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    }
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave, &Results);
    UE_LOG(LogTemp, Display, TEXT("DOOR MATERIAL compile: errors=%d warnings=%d"), Results.NumErrors, Results.NumWarnings);
    if (Results.NumErrors || !VerifyInstances(Blueprint)) return 8;
    if (!bVerifyOnly)
    {
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, Args)) return 9;
        UE_LOG(LogTemp, Display, TEXT("DOOR MATERIAL SAVED %s"), *Filename);
    }
    return 0;
}
