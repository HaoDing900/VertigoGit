#include "VTGNoteWorldMaterialCommandlet.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
const FName MaterialField(TEXT("WorldStaticMaterial"));
bool Compile(UBlueprint* BP)
{
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Log);
    return Log.NumErrors == 0 && BP->GeneratedClass;
}
bool Save(UBlueprint* BP)
{
    BP->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    return UPackage::SavePackage(BP->GetOutermost(), BP,
        *FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}
bool Test(UBlueprint* Data, UBlueprint* ActorBP)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    auto* Source = LoadObject<UObject>(nullptr, TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/PDA_L3Sewer_UnionLeaflet.PDA_L3Sewer_UnionLeaflet"));
    if (!Source) return false;
    auto* Probe = DuplicateObject<UObject>(Source, GetTransientPackage());
    auto* Field = FindFProperty<FObjectPropertyBase>(Data->GeneratedClass, MaterialField);
    auto* NoteField = FindFProperty<FObjectPropertyBase>(ActorBP->GeneratedClass, TEXT("Note Data Asset"));
    if (!Field || !NoteField) return false;
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    FActorSpawnParameters Spawn;
    Spawn.bDeferConstruction = true;
    auto* Actor = World->SpawnActor<AActor>(ActorBP->GeneratedClass, FTransform::Identity, Spawn);
    bool Passed = Actor != nullptr;
    if (Actor)
    {
        NoteField->SetObjectPropertyValue_InContainer(Actor, Probe);
        auto* Override = NewObject<UMaterial>(GetTransientPackage());
        Field->SetObjectPropertyValue_InContainer(Probe, Override);
        Actor->FinishSpawning(FTransform::Identity);
        auto* Mesh = Actor->FindComponentByClass<UStaticMeshComponent>();
        Passed = Mesh && Mesh->GetStaticMesh() && Mesh->GetMaterial(0) == Override;
        UE_LOG(LogTemp, Display, TEXT("NOTE WORLD MATERIAL: construction override %s"), Passed ? TEXT("PASS") : TEXT("FAIL"));
        // Reuse the same actor: clearing the optional field must not retain an old override.
        Field->SetObjectPropertyValue_InContainer(Probe, nullptr);
        auto* LoadMesh = Actor->FindFunction(TEXT("LoadMesh"));
        if (LoadMesh) Actor->ProcessEvent(LoadMesh, nullptr);
        const bool Restored = LoadMesh && Mesh && Mesh->GetStaticMesh() && Mesh->GetMaterial(0) == Mesh->GetStaticMesh()->GetMaterial(0);
        UE_LOG(LogTemp, Display, TEXT("NOTE WORLD MATERIAL: empty restores mesh default %s"), Restored ? TEXT("PASS") : TEXT("FAIL"));
        Passed &= Restored;
    }
    World->DestroyWorld(false);
    return Passed;
}
}

UVTGNoteWorldMaterialCommandlet::UVTGNoteWorldMaterialCommandlet()
{
    IsEditor = true;
    IsClient = IsServer = false;
    LogToConsole = true;
}

int32 RunNoteBackgroundMaterial(const FString& Params);

int32 UVTGNoteWorldMaterialCommandlet::Main(const FString& Params)
{
    if (FParse::Param(*Params, TEXT("Background"))) return RunNoteBackgroundMaterial(Params);
    auto* Data = LoadObject<UBlueprint>(nullptr, TEXT("/InventorySystemX/Modules/Notes/DataAssets/PDA_NoteText.PDA_NoteText"));
    auto* ActorBP = LoadObject<UBlueprint>(nullptr, TEXT("/InventorySystemX/Modules/Notes/Actors/BP_ReadableNote.BP_ReadableNote"));
    if (!Data || !ActorBP) return 1;
    if (FParse::Param(*Params, TEXT("VerifyOnly"))) return Test(Data, ActorBP) ? 0 : 2;
    for (auto* BP : {Data, ActorBP})
    {
        const FString Source = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        const FString Backup = FPaths::ProjectSavedDir() / TEXT("Backups/NoteWorldMaterial") / (BP->GetName() + TEXT(".uasset"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
        if (!IFileManager::Get().FileExists(*Backup) && IFileManager::Get().Copy(*Backup, *Source) != COPY_OK) return 3;
    }
    if (!FindFProperty<FObjectPropertyBase>(Data->GeneratedClass, MaterialField))
    {
        FEdGraphPinType Type;
        Type.PinCategory = UEdGraphSchema_K2::PC_Object;
        Type.PinSubCategoryObject = UMaterialInterface::StaticClass();
        if (!FBlueprintEditorUtils::AddMemberVariable(Data, MaterialField, Type)) return 4;
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Data, MaterialField, nullptr, FText::FromString(TEXT("Document")));
        FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(Data, MaterialField, false);
        FBlueprintEditorUtils::SetBlueprintVariableMetaData(Data, MaterialField, nullptr, TEXT("ToolTip"), TEXT("Optional material override for World Static Mesh slot 0. None uses the mesh default material. Does not affect the reading background."));
    }
    if (!Compile(Data)) return 5;
    UEdGraph* Graph = nullptr;
    for (const auto& Candidate : ActorBP->FunctionGraphs) if (Candidate->GetFName() == TEXT("LoadMesh")) Graph = Candidate.Get();
    if (!Graph) return 6;
    UK2Node_CallFunction* SetMesh = nullptr;
    bool AlreadyPatched = false;
    for (const auto& Node : Graph->Nodes)
        if (auto* Call = Cast<UK2Node_CallFunction>(Node.Get()))
        {
            if (Call->FunctionReference.GetMemberName() == TEXT("SetStaticMesh")) SetMesh = Call;
            if (Call->GetFName() == TEXT("VTG_SetWorldMaterial")) AlreadyPatched = true;
        }
    if (!SetMesh) return 7;
    if (!AlreadyPatched)
    {
        const auto* Schema = GetDefault<UEdGraphSchema_K2>();
        auto* GetData = NewObject<UK2Node_VariableGet>(Graph);
        Graph->AddNode(GetData, false, false);
        GetData->CreateNewGuid();
        GetData->VariableReference.SetSelfMember(TEXT("Note Data Asset"));
        GetData->AllocateDefaultPins();
        GetData->NodePosX = SetMesh->NodePosX;
        GetData->NodePosY = SetMesh->NodePosY + 260;
        auto* GetMaterial = NewObject<UK2Node_VariableGet>(Graph);
        Graph->AddNode(GetMaterial, false, false);
        GetMaterial->CreateNewGuid();
        GetMaterial->VariableReference.SetExternalMember(MaterialField, Data->GeneratedClass);
        GetMaterial->AllocateDefaultPins();
        GetMaterial->NodePosX = SetMesh->NodePosX + 260;
        GetMaterial->NodePosY = SetMesh->NodePosY + 240;
        auto* SetMaterial = NewObject<UK2Node_CallFunction>(Graph, TEXT("VTG_SetWorldMaterial"));
        Graph->AddNode(SetMaterial, false, false);
        SetMaterial->CreateNewGuid();
        SetMaterial->SetFromFunction(UPrimitiveComponent::StaticClass()->FindFunctionByName(TEXT("SetMaterial")));
        SetMaterial->AllocateDefaultPins();
        SetMaterial->NodePosX = SetMesh->NodePosX + 560;
        SetMaterial->NodePosY = SetMesh->NodePosY;
        auto Connect = [&](UEdGraphPin* A, UEdGraphPin* B) { return A && B && Schema->TryCreateConnection(A, B); };
        auto* MeshTarget = SetMesh->FindPin(UEdGraphSchema_K2::PN_Self);
        if (!MeshTarget || MeshTarget->LinkedTo.Num() != 1) return 8;
        if (!Connect(GetData->GetValuePin(), GetMaterial->FindPin(UEdGraphSchema_K2::PN_Self)) ||
            !Connect(GetMaterial->GetValuePin(), SetMaterial->FindPin(TEXT("Material"))) ||
            !Connect(MeshTarget->LinkedTo[0], SetMaterial->FindPin(UEdGraphSchema_K2::PN_Self))) return 9;
        auto* Then = SetMesh->FindPin(UEdGraphSchema_K2::PN_Then);
        const auto OldLinks = Then->LinkedTo;
        Then->BreakAllPinLinks();
        if (!Connect(Then, SetMaterial->FindPin(UEdGraphSchema_K2::PN_Execute))) return 10;
        for (auto* Link : OldLinks) if (!Connect(SetMaterial->FindPin(UEdGraphSchema_K2::PN_Then), Link)) return 11;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ActorBP);
    }
    if (!Compile(ActorBP) || !Test(Data, ActorBP)) return 12;
    if (!Save(Data) || !Save(ActorBP)) return 13;
    UE_LOG(LogTemp, Display, TEXT("NOTE WORLD MATERIAL: saved optional Document.WorldStaticMaterial and LoadMesh wiring"));
    return 0;
}
