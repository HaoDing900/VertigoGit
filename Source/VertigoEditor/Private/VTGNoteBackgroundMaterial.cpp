#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "Engine/LatentActionManager.h"
#include "Containers/Ticker.h"

namespace
{
bool CompileBackground(UBlueprint* BP)
{
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Log);
    return !Log.NumErrors && BP->GeneratedClass;
}
bool TestBackground(UBlueprint* BP)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    auto* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/InventorySystemX/Modules/Notes/UMG/NoteMesh.NoteMesh"));
    if (!Mesh) return false;
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto* Actor = World->SpawnActor<AActor>(BP->GeneratedClass);
    auto* Function = Actor ? Actor->FindFunction(TEXT("SetMesh")) : nullptr;
    bool Passed = false;
    auto CompleteLoad = [&]()
    {
        FlushAsyncLoading();
        for (int32 Frame = 0; Frame < 4; ++Frame)
        {
            FTSTicker::GetCoreTicker().Tick(0.1f);
            World->GetLatentActionManager().BeginFrame();
            World->GetLatentActionManager().ProcessLatentActions(nullptr, 0.1f);
        }
    };
    if (Function)
    {
        FStructOnScope Params(Function);
        auto* MeshParam = FindFProperty<FSoftObjectProperty>(Function, TEXT("Mesh"));
        auto* MaterialParam = FindFProperty<FObjectPropertyBase>(Function, TEXT("Material"));
        if (MeshParam && MaterialParam)
        {
            MeshParam->SetPropertyValue_InContainer(Params.GetStructMemory(), FSoftObjectPtr(Mesh));
            auto* Override = NewObject<UMaterial>(GetTransientPackage());
            MaterialParam->SetObjectPropertyValue_InContainer(Params.GetStructMemory(), Override);
            Actor->ProcessEvent(Function, Params.GetStructMemory());
            CompleteLoad();
            auto* Component = Actor->FindComponentByClass<UStaticMeshComponent>();
            Passed = Component && Component->GetStaticMesh() == Mesh && Component->GetMaterial(0) == Override;
            UE_LOG(LogTemp, Display, TEXT("NOTE BACKGROUND MATERIAL: loaded mesh override %s"), Passed ? TEXT("PASS") : TEXT("FAIL"));
            MaterialParam->SetObjectPropertyValue_InContainer(Params.GetStructMemory(), nullptr);
            Actor->ProcessEvent(Function, Params.GetStructMemory());
            CompleteLoad();
            const bool Cleared = Component && Component->GetMaterial(0) == Mesh->GetMaterial(0);
            Passed &= Cleared;
            UE_LOG(LogTemp, Display, TEXT("NOTE BACKGROUND MATERIAL: clear previous override %s"), Cleared ? TEXT("PASS") : TEXT("FAIL"));
        }
    }
    World->DestroyWorld(false);
    return Passed;
}
}

int32 RunNoteBackgroundMaterial(const FString& Params)
{
    if (FParse::Param(*Params, TEXT("SewerProbe")))
    {
        TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
        auto* World = UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/ThirdPerson/Maps/L_SewerUnderApartment"));
        if (!World) return 20;
        for (TActorIterator<AActor> It(World); It; ++It)
            if (It->GetClass()->GetName() == TEXT("BPLM_SewerUnderApt_C"))
            {
                auto* Ref = FindFProperty<FObjectPropertyBase>(It->GetClass(), TEXT("L3_SwitchColi"));
                auto* Target = Ref ? Cast<AActor>(Ref->GetObjectPropertyValue_InContainer(*It)) : nullptr;
                auto* Function = It->FindFunction(TEXT("L3_AfterBossIsDefeated"));
                if (!Target || !Function) return 21;
                UE_LOG(LogTemp, Display, TEXT("SEWER SWITCH PROBE: target=%s before=%s"), *Target->GetPathName(), *Target->GetActorLocation().ToString());
                It->ProcessEvent(Function, nullptr);
                UE_LOG(LogTemp, Display, TEXT("SEWER SWITCH PROBE: after AfterBoss=%s"), *Target->GetActorLocation().ToString());
                return FMath::IsNearlyEqual(Target->GetActorLocation().Z, -692.8291, 0.01) ? 0 : 22;
            }
        return 23;
    }
    auto* Data = LoadObject<UBlueprint>(nullptr, TEXT("/InventorySystemX/Modules/Notes/DataAssets/PDA_NoteText.PDA_NoteText"));
    auto* Background = LoadObject<UBlueprint>(nullptr, TEXT("/InventorySystemX/Modules/Notes/Actors/BP_NoteMeshBackground.BP_NoteMeshBackground"));
    auto* Widget = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Inventory/InventoryXBased/UMG/WB_Note.WB_Note"));
    if (!Data || !Background || !Widget) return 1;
    if (FParse::Param(*Params, TEXT("VerifyOnly"))) return TestBackground(Background) ? 0 : 2;
    for (auto* BP : {Data, Background, Widget})
    {
        const auto Source = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        const auto Backup = FPaths::ProjectSavedDir() / TEXT("Backups/NoteBackgroundMaterial") / (BP->GetName() + TEXT(".uasset"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
        if (!IFileManager::Get().FileExists(*Backup) && IFileManager::Get().Copy(*Backup, *Source) != COPY_OK) return 3;
    }
    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_Object;
    Type.PinSubCategoryObject = UMaterialInterface::StaticClass();
    const FName Field(TEXT("BackgroundMaterial"));
    if (!FindFProperty<FObjectPropertyBase>(Data->GeneratedClass, Field))
    {
        if (!FBlueprintEditorUtils::AddMemberVariable(Data, Field, Type)) return 4;
        FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(Data, Field, false);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Data, Field, nullptr, FText::FromString(TEXT("Background|Material")));
        FBlueprintEditorUtils::SetBlueprintVariableMetaData(Data, Field, nullptr, TEXT("ToolTip"), TEXT("Optional background mesh material slot 0 override. None restores the mesh default. Does not change World Static Material."));
        const int32 Index = FBlueprintEditorUtils::FindNewVariableIndex(Data, Field);
        Data->NewVariables[Index].PropertyFlags |= CPF_AdvancedDisplay;
    }
    if (!CompileBackground(Data)) return 5;
    auto* Graph = Background->UbergraphPages[0].Get();
    UK2Node_CustomEvent* Event = nullptr;
    for (const auto& Node : Graph->Nodes)
        if (auto* E = Cast<UK2Node_CustomEvent>(Node.Get())) if (E->CustomFunctionName == TEXT("SetMesh")) Event = E;
    if (!Event) return 6;
    auto* MaterialPin = Event->FindPin(TEXT("Material"));
    if (!MaterialPin) MaterialPin = Event->CreateUserDefinedPin(TEXT("Material"), Type, EGPD_Output);
    if (!MaterialPin) return 7;
    const auto* Schema = GetDefault<UEdGraphSchema_K2>();
    auto Connect = [&](UEdGraphPin* A, UEdGraphPin* B) { return A && B && Schema->TryCreateConnection(A, B); };
    const auto Nodes = Graph->Nodes;
    int32 Paths = 0;
    for (const auto& Node : Nodes)
        if (auto* SetMesh = Cast<UK2Node_CallFunction>(Node.Get()))
            if (SetMesh->FunctionReference.GetMemberName() == TEXT("SetStaticMesh") && SetMesh->FindPin(TEXT("NewMesh"))->LinkedTo.Num())
            {
                ++Paths;
                const FName Name(*(TEXT("VTG_BackgroundMaterial_") + SetMesh->GetName()));
                bool Exists = false;
                for (const auto& N : Graph->Nodes) Exists |= N->GetFName() == Name;
                if (Exists) continue;
                auto* Call = NewObject<UK2Node_CallFunction>(Graph, Name);
                Graph->AddNode(Call, false, false);
                Call->CreateNewGuid();
                Call->SetFromFunction(UPrimitiveComponent::StaticClass()->FindFunctionByName(TEXT("SetMaterial")));
                Call->AllocateDefaultPins();
                Call->NodePosX = SetMesh->NodePosX + 300;
                Call->NodePosY = SetMesh->NodePosY + 160;
                auto* Target = SetMesh->FindPin(UEdGraphSchema_K2::PN_Self);
                if (Target->LinkedTo.Num() != 1 || !Connect(Target->LinkedTo[0], Call->FindPin(UEdGraphSchema_K2::PN_Self)) || !Connect(MaterialPin, Call->FindPin(TEXT("Material")))) return 8;
                auto* Then = SetMesh->FindPin(UEdGraphSchema_K2::PN_Then);
                const auto Links = Then->LinkedTo;
                Then->BreakAllPinLinks();
                if (!Connect(Then, Call->FindPin(UEdGraphSchema_K2::PN_Execute))) return 9;
                for (auto* Link : Links) if (!Connect(Call->FindPin(UEdGraphSchema_K2::PN_Then), Link)) return 10;
            }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Background);
    if (Paths != 1 || !CompileBackground(Background)) return 11;
    int32 Callers = 0;
    TArray<UEdGraph*> Graphs;
    Widget->GetAllGraphs(Graphs);
    for (auto* G : Graphs)
    {
        const auto WidgetNodes = G->Nodes;
        for (const auto& Node : WidgetNodes)
            if (auto* Call = Cast<UK2Node_CallFunction>(Node.Get()))
                if (Call->FunctionReference.GetMemberName() == TEXT("SetMesh"))
                {
                    ++Callers;
                    Call->ReconstructNode();
                    auto* Pin = Call->FindPin(TEXT("Material"));
                    if (!Pin) return 12;
                    if (Pin->LinkedTo.Num()) continue;
                    auto* Getter = NewObject<UK2Node_VariableGet>(G);
                    G->AddNode(Getter, false, false);
                    Getter->CreateNewGuid();
                    Getter->VariableReference.SetExternalMember(Field, Data->GeneratedClass);
                    Getter->AllocateDefaultPins();
                    Getter->NodePosX = Call->NodePosX - 300;
                    Getter->NodePosY = Call->NodePosY + 320;
                    auto* DataGetter = NewObject<UK2Node_VariableGet>(G);
                    G->AddNode(DataGetter, false, false);
                    DataGetter->CreateNewGuid();
                    DataGetter->VariableReference.SetSelfMember(TEXT("DataAsset"));
                    DataGetter->AllocateDefaultPins();
                    DataGetter->NodePosX = Getter->NodePosX - 200;
                    DataGetter->NodePosY = Getter->NodePosY;
                    if (!Connect(DataGetter->GetValuePin(), Getter->FindPin(UEdGraphSchema_K2::PN_Self)) || !Connect(Getter->GetValuePin(), Pin)) return 13;
                }
    }
    if (!Callers) return 14;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Widget);
    if (!CompileBackground(Widget) || !TestBackground(Background)) return 15;
    for (auto* BP : {Data, Background, Widget})
    {
        BP->MarkPackageDirty();
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(BP->GetOutermost(), BP, *FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args)) return 16;
    }
    UE_LOG(LogTemp, Display, TEXT("NOTE BACKGROUND MATERIAL: saved, %d mesh paths and %d widget callers"), Paths, Callers);
    return 0;
}
