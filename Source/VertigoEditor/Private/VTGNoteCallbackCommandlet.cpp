#include "VTGNoteCallbackCommandlet.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
const FName Callback(TEXT(" OnStoryNoteFinished")); // Existing parent function includes a leading space.
UEdGraph* EventGraph(UBlueprint* BP)
{
    for (const auto& Graph : BP->UbergraphPages) if (Graph->GetFName() == TEXT("EventGraph")) return Graph.Get();
    return nullptr;
}

bool TestDispatch(UBlueprint* Saved)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    const bool Dirty = Saved->GetOutermost()->IsDirty();
    auto* Probe = DuplicateObject<UBlueprint>(Saved, GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("NoteCallbackProbe")));
    auto* Graph = EventGraph(Probe);
    if (!Graph) return false;
    int32 Markers = 0;
    const auto Nodes = Graph->Nodes;
    for (const auto& Node : Nodes)
        if (auto* Event = Cast<UK2Node_CustomEvent>(Node.Get()))
            if (Event->CustomFunctionName == TEXT("On Close Note_L3PoliceMail") || Event->CustomFunctionName == TEXT("On Close Note_L3Overdue"))
            {
                // Only the transient test copy changes: replace story/cinematic playback with a native marker.
                auto* Marker = NewObject<UK2Node_CallFunction>(Graph);
                Graph->AddNode(Marker, false, false);
                Marker->CreateNewGuid();
                Marker->SetFromFunction(AActor::StaticClass()->FindFunctionByName(TEXT("SetActorTickEnabled")));
                Marker->AllocateDefaultPins();
                GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Marker->FindPin(TEXT("bEnabled")), TEXT("true"));
                auto* Then = Event->FindPin(UEdGraphSchema_K2::PN_Then);
                Then->BreakAllPinLinks();
                if (!GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Then, Marker->FindPin(UEdGraphSchema_K2::PN_Execute))) return false;
                ++Markers;
            }
    if (Markers != 2) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Probe);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Probe, EBlueprintCompileOptions::None, &Results);
    if (Results.NumErrors || !Probe->GeneratedClass) return false;
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Actor = World->SpawnActor<AActor>(Probe->GeneratedClass, FTransform::Identity, Spawn);
    bool Passed = Actor != nullptr;
    if (Actor)
    {
        Actor->PrimaryActorTick.bCanEverTick = true;
        auto* Function = Actor->FindFunction(Callback);
        auto* ID = Function ? FindFProperty<FNameProperty>(Function, TEXT("NoteID")) : nullptr;
        Passed = Function && ID && Function->GetOuterUClass() == Probe->GeneratedClass;
        if (Passed) for (const FName Row : {FName(TEXT("L3PoliceMail")), FName(TEXT("L3Overdue")), FName(TEXT("UnknownNote"))})
        {
            Actor->SetActorTickEnabled(false);
            FStructOnScope Params(Function);
            ID->SetPropertyValue_InContainer(Params.GetStructMemory(), Row);
            Actor->ProcessEvent(Function, Params.GetStructMemory());
            const bool Expected = Row != TEXT("UnknownNote");
            const bool OK = Actor->IsActorTickEnabled() == Expected;
            UE_LOG(LogTemp, Display, TEXT("NOTE CALLBACK TEST %s: %s"), *Row.ToString(), OK ? TEXT("PASS") : TEXT("FAIL"));
            Passed &= OK;
        }
    }
    World->DestroyWorld(false);
    return Passed && Saved->GetOutermost()->IsDirty() == Dirty;
}
}

UVTGNoteCallbackCommandlet::UVTGNoteCallbackCommandlet()
{
    IsEditor = true;
    IsClient = IsServer = false;
    LogToConsole = true;
}

int32 UVTGNoteCallbackCommandlet::Main(const FString& Params)
{
    const bool Verify = FParse::Param(*Params, TEXT("VerifyOnly"));
    auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt.BPLM_SewerUnderApt"));
    auto* Graph = BP ? EventGraph(BP) : nullptr;
    if (!Graph || !BP->ParentClass || !BP->ParentClass->FindFunctionByName(Callback)) return 1;
    UK2Node_Event* Override = nullptr;
    UK2Node_CustomEvent* Orphan = nullptr;
    for (const auto& Node : Graph->Nodes)
    {
        if (auto* Event = Cast<UK2Node_Event>(Node.Get()))
            if (!Cast<UK2Node_CustomEvent>(Event) && Event->bOverrideFunction && Event->EventReference.GetMemberName() == Callback) Override = Event;
        if (auto* Event = Cast<UK2Node_CustomEvent>(Node.Get()))
            if (Event->GetFName() == TEXT("K2Node_Event_2") && Event->CustomFunctionName == TEXT("CustomEvent")) Orphan = Event;
    }
    if (!Override && Verify) return 2;
    if (!Override)
    {
        if (!Orphan || !Orphan->FindPin(TEXT("NoteID"))) return 3;
        const FString Source = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        const FString Backup = FPaths::ProjectSavedDir()/TEXT("Backups/NoteInputOwnership/BPLM_SewerUnderApt-before-note-callback.uasset");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
        if (!IFileManager::Get().FileExists(*Backup) && IFileManager::Get().Copy(*Backup, *Source) != COPY_OK) return 4;
        Override = NewObject<UK2Node_Event>(Graph);
        Graph->AddNode(Override, false, false);
        Override->CreateNewGuid();
        Override->EventReference.SetExternalMember(Callback, BP->ParentClass);
        Override->bOverrideFunction = true;
        Override->NodePosX = Orphan->NodePosX;
        Override->NodePosY = Orphan->NodePosY;
        Override->AllocateDefaultPins();
        for (const FName PinName : {UEdGraphSchema_K2::PN_Then, FName(TEXT("NoteID"))})
        {
            auto* Old = Orphan->FindPin(PinName);
            auto* New = Override->FindPin(PinName);
            if (!Old || !New) return 5;
            const auto Links = Old->LinkedTo;
            Old->BreakAllPinLinks();
            for (auto* Link : Links) if (!GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(New, Link)) return 6;
        }
        FBlueprintEditorUtils::RemoveNode(BP, Orphan, true);
        Override->NodeComment = TEXT("Parent BPLM Note-close callback. Route NoteID to this level's story continuation.");
        Override->bCommentBubbleVisible = true;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Results);
        if (Results.NumErrors || !TestDispatch(BP)) return 7;
        FSavePackageArgs Save;
        Save.TopLevelFlags = RF_Public | RF_Standalone;
        Save.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(BP->GetOutermost(), BP, *Source, Save)) return 8;
    }
    if (!TestDispatch(BP)) return 9;
    UE_LOG(LogTemp, Display, TEXT("NOTE CALLBACK: saved Sewer override and Police/Overdue runtime dispatch verified"));
    return 0;
}
