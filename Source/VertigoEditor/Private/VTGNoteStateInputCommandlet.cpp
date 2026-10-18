#include "VTGNoteStateInputCommandlet.h"
#include "UI/VTGNoteInputWidget.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"

UVTGNoteStateInputCommandlet::UVTGNoteStateInputCommandlet()
{
    IsEditor = true;
    IsClient = IsServer = false;
    LogToConsole = true;
}

int32 UVTGNoteStateInputCommandlet::Main(const FString& Params)
{
    const bool VerifyOnly = FParse::Param(*Params, TEXT("VerifyOnly"));
    auto* Note = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Inventory/InventoryXBased/UMG/WB_NoteMainWidget.WB_NoteMainWidget"));
    auto* Component = LoadObject<UBlueprint>(nullptr, TEXT("/InventorySystemX/Modules/Notes/Components/AC_NotesComponent.AC_NotesComponent"));
    if (!Note || !Component) return 1;

    // Derive Closed from the plugin's actual close route instead of a display label or hardcoded enum index.
    int64 Closed = INDEX_NONE;
    TArray<UEdGraph*> Graphs;
    Component->GetAllGraphs(Graphs);
    for (auto* Graph : Graphs) if (Graph->GetFName() == TEXT("RemoveBaseWidget"))
        for (const auto& Node : Graph->Nodes)
            if (auto* Call = Cast<UK2Node_CallFunction>(Node.Get()))
                if (Call->FunctionReference.GetMemberName() == TEXT("SetState"))
                    if (auto* Pin = Call->FindPin(TEXT("NewState")))
                        if (auto* Enum = Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get()))
                            Closed = Enum->GetValueByNameString(Pin->DefaultValue);
    if (Closed < 0 || Closed > 255) return 2;
    UE_LOG(LogTemp, Display, TEXT("NOTE STATE: Closed value from NotesComponent close route = %lld"), Closed);

    Graphs.Reset();
    Note->GetAllGraphs(Graphs);
    UK2Node_CustomEvent* Event = nullptr;
    for (auto* Graph : Graphs) if (Graph->GetFName() == TEXT("EventGraph"))
        for (const auto& Node : Graph->Nodes)
            if (auto* Candidate = Cast<UK2Node_CustomEvent>(Node.Get()))
                if (Candidate->CustomFunctionName == TEXT("OnStateChanged_Event")) Event = Candidate;
    if (!Event) return 3;
    auto* Graph = Event->GetGraph();
    const auto* Schema = GetDefault<UEdGraphSchema_K2>();
    auto MakeCall = [&](UClass* Owner, const TCHAR* Function, int32 X, int32 Y)
    {
        auto* Call = NewObject<UK2Node_CallFunction>(Graph);
        Graph->AddNode(Call, false, false);
        Call->CreateNewGuid();
        Call->SetFromFunction(Owner->FindFunctionByName(Function));
        Call->AllocateDefaultPins();
        Call->NodePosX = X;
        Call->NodePosY = Y;
        return Call;
    };
    UK2Node_CallFunction* Apply = nullptr;
    for (const auto& Node : Graph->Nodes)
        if (auto* Call = Cast<UK2Node_CallFunction>(Node.Get()))
            if (Call->FunctionReference.GetMemberName() == GET_FUNCTION_NAME_CHECKED(UVTGNoteInputWidget, SetNoteInputActive)) Apply = Call;
    if (!Apply && VerifyOnly) return 4;
    if (!Apply)
    {
        const FString Source = FPackageName::LongPackageNameToFilename(Note->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        const FString Backup = FPaths::ProjectSavedDir()/TEXT("Backups/NoteInputOwnership/WB_NoteMainWidget-before-state-input.uasset");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
        if (!IFileManager::Get().FileExists(*Backup) && IFileManager::Get().Copy(*Backup, *Source) != COPY_OK) return 5;
        Apply = MakeCall(UVTGNoteInputWidget::StaticClass(), TEXT("SetNoteInputActive"), Event->NodePosX + 420, Event->NodePosY);
        auto* Compare = MakeCall(UKismetMathLibrary::StaticClass(), TEXT("NotEqual_ByteByte"), Event->NodePosX + 180, Event->NodePosY + 180);
        Schema->TrySetDefaultValue(*Compare->FindPin(TEXT("B")), FString::FromInt(Closed));
        if (!Schema->TryCreateConnection(Event->FindPin(TEXT("State")), Compare->FindPin(TEXT("A"))) ||
            !Schema->TryCreateConnection(Compare->FindPin(TEXT("ReturnValue")), Apply->FindPin(TEXT("bActive")))) return 6;
        auto* Then = Event->FindPin(UEdGraphSchema_K2::PN_Then);
        const auto OldLinks = Then->LinkedTo;
        Then->BreakAllPinLinks();
        if (!Schema->TryCreateConnection(Then, Apply->FindPin(UEdGraphSchema_K2::PN_Execute))) return 7;
        for (auto* Link : OldLinks)
            if (!Schema->TryCreateConnection(Apply->FindPin(UEdGraphSchema_K2::PN_Then), Link)) return 8;
        Apply->NodeComment = TEXT("Release Note input on logical Closed, even while the root is cached or animating.");
        Apply->bCommentBubbleVisible = true;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Note);
        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(Note, EBlueprintCompileOptions::None, &Results);
        if (Results.NumErrors) return 9;
        FSavePackageArgs Save;
        Save.TopLevelFlags = RF_Public | RF_Standalone;
        Save.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Note->GetOutermost(), Note, *Source, Save)) return 10;
    }
    if (!Event->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(Apply->FindPin(UEdGraphSchema_K2::PN_Execute))) return 11;
    if (Apply->FindPin(TEXT("bActive"))->LinkedTo.Num() != 1) return 12;
    auto* Compare = Cast<UK2Node_CallFunction>(Apply->FindPin(TEXT("bActive"))->LinkedTo[0]->GetOwningNode());
    if (!Compare || Compare->FunctionReference.GetMemberName() != TEXT("NotEqual_ByteByte") ||
        FCString::Atoi(*Compare->FindPin(TEXT("B"))->DefaultValue) != Closed ||
        !Compare->FindPin(TEXT("A"))->LinkedTo.Contains(Event->FindPin(TEXT("State")))) return 13;
    UE_LOG(LogTemp, Display, TEXT("NOTE STATE: verified logical Note state drives UI ownership"));
    return 0;
}
