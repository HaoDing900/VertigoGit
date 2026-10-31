#include "VTGUIFeedbackCommandlet.h"
#include "Audio/VTGUIFeedbackLibrary.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace VTGUIFeedback
{
void Compile(UBlueprint* BP)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &Log);
    check(Log.NumErrors == 0 && BP->Status != BS_Error);
}
void Backup(UBlueprint* BP)
{
    const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    const FString Target = FPaths::ProjectDir() / TEXT("Scripts/UIFeedback/Backup") / FPaths::GetCleanFilename(File);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target), true);
    if (!IFileManager::Get().FileExists(*Target)) check(IFileManager::Get().Copy(*Target, *File) == COPY_OK);
}
void Save(UBlueprint* BP)
{
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
    check(UPackage::SavePackage(BP->GetOutermost(), BP, *FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args));
}
template<class T> T* Add(UEdGraph* Graph)
{
    auto* Node = NewObject<T>(Graph); Graph->AddNode(Node, false, false); Node->CreateNewGuid(); return Node;
}
void Link(UEdGraphPin* A, UEdGraphPin* B) { check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A, B)); }

void Install(UBlueprint* BP, UEdGraphNode* Entry, bool Interaction, bool Verify)
{
    const FName Function = Interaction ? TEXT("PlayInteractionFeedback") : TEXT("PlayQuestFeedback");
    auto* Graph = Entry->GetGraph();
    UK2Node_CallFunction* Call = nullptr;
    int32 Count = 0;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (auto* C = Cast<UK2Node_CallFunction>(Node))
            if (C->FunctionReference.GetMemberName() == Function) { Call = C; ++Count; }
    check(Count <= 1);
    if (!Call)
    {
        check(!Verify);
        Call = Add<UK2Node_CallFunction>(Graph);
        Call->SetFromFunction(UVTGUIFeedbackLibrary::StaticClass()->FindFunctionByName(Function));
        Call->AllocateDefaultPins(); Call->NodePosX = Entry->NodePosX + 240; Call->NodePosY = Entry->NodePosY - 160;
        Call->NodeComment = TEXT("Independent interaction / quest UI feedback (not dialogue audio)");
        Call->bCommentBubbleVisible = true;
        auto* Out = Entry->FindPinChecked(TEXT("then"));
        const auto OldLinks = Out->LinkedTo; check(!OldLinks.IsEmpty()); Out->BreakAllPinLinks();
        Link(Out, Call->FindPinChecked(TEXT("execute")));
        for (auto* Pin : OldLinks) Link(Call->FindPinChecked(TEXT("then")), Pin);
        if (Interaction)
        {
            auto* Self = Add<UK2Node_Self>(Graph); Self->AllocateDefaultPins();
            Link(Self->FindPinChecked(TEXT("self")), Call->FindPinChecked(TEXT("WorldContextObject")));
            for (const auto& Pair : TArray<TPair<FName,FName>>{{TEXT("InteractionSoundEnabled"),TEXT("bEnabled")},{TEXT("InteractionSoundOverride"),TEXT("SoundOverride")}})
            {
                auto* Get = Add<UK2Node_VariableGet>(Graph); Get->VariableReference.SetSelfMember(Pair.Key); Get->AllocateDefaultPins();
                Link(Get->FindPinChecked(Pair.Key), Call->FindPinChecked(Pair.Value));
            }
        }
        else
            Link(Entry->FindPinChecked(TEXT("NarrativeComponent")), Call->FindPinChecked(TEXT("WorldContextObject")));
    }
    check(Entry->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Call->FindPinChecked(TEXT("execute"))));
    check(!Call->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty());
    check(!Call->FindPinChecked(TEXT("WorldContextObject"))->LinkedTo.IsEmpty());
    if (Interaction)
    {
        check(!Call->FindPinChecked(TEXT("bEnabled"))->LinkedTo.IsEmpty());
        check(!Call->FindPinChecked(TEXT("SoundOverride"))->LinkedTo.IsEmpty());
    }
    Compile(BP);
    if (!Verify) Save(BP);
    UE_LOG(LogTemp, Display, TEXT("UI FEEDBACK PASS: %s trigger wired once; existing execution preserved"), *BP->GetName());
}
}

int32 UVTGUIFeedbackCommandlet::Main(const FString& Params)
{
    using namespace VTGUIFeedback;
    const bool Verify = FParse::Param(*Params, TEXT("VerifyOnly"));
    const auto* Defaults = GetDefault<UVTGUIFeedbackLibrary>();
    check(Defaults->InteractionSounds.Num() == 5 && Defaults->QuestSounds.Num() == 3);
    auto* Override = Defaults->QuestSounds[0].Get();
    check(!UVTGUIFeedbackLibrary::ResolveInteractionSound(false, nullptr));
    check(!UVTGUIFeedbackLibrary::ResolveInteractionSound(false, Override));
    check(UVTGUIFeedbackLibrary::ResolveInteractionSound(true, Override) == Override);
    for (int32 I = 0; I < 100; ++I) check(Defaults->InteractionSounds.Contains(UVTGUIFeedbackLibrary::ResolveInteractionSound(true, nullptr)));
    UE_LOG(LogTemp, Display, TEXT("UI FEEDBACK PASS: five hover / three quest sources; disabled, override and default fallback"));

    auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/BP_0ISXInteractiveCollision")); check(BP);
    if (!Verify)
    {
        Backup(BP);
        for (const FName Name : {FName(TEXT("InteractionSoundEnabled")), FName(TEXT("InteractionSoundOverride"))})
        {
            if (FBlueprintEditorUtils::FindNewVariableIndex(BP, Name) != INDEX_NONE) continue;
            FEdGraphPinType Type;
            const bool Enabled = Name == TEXT("InteractionSoundEnabled");
            Type.PinCategory = Enabled ? UEdGraphSchema_K2::PC_Boolean : UEdGraphSchema_K2::PC_Object;
            if (!Enabled) Type.PinSubCategoryObject = USoundBase::StaticClass();
            check(FBlueprintEditorUtils::AddMemberVariable(BP, Name, Type, Enabled ? TEXT("true") : TEXT("None")));
            FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, Name, false);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, Name, nullptr, FText::FromString(TEXT("Interaction Audio")));
            FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, Name, nullptr, TEXT("ToolTip"), Enabled ? TEXT("Play audio when the player interacts. Enabled by default.") : TEXT("Optional interaction sound. None randomly uses Hover Magic 001-005."));
        }
        Compile(BP);
    }
    auto* Enabled = FindFProperty<FBoolProperty>(BP->GeneratedClass, TEXT("InteractionSoundEnabled"));
    auto* OverrideProperty = FindFProperty<FObjectPropertyBase>(BP->GeneratedClass, TEXT("InteractionSoundOverride"));
    check(Enabled && OverrideProperty && Enabled->GetPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject()));
    check(!OverrideProperty->GetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject()));
    for (FProperty* Property : {static_cast<FProperty*>(Enabled), static_cast<FProperty*>(OverrideProperty)})
        check(Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
    UEdGraphNode* InteractionEntry = nullptr;
    for (UEdGraph* Graph : BP->UbergraphPages)
        for (UEdGraphNode* Node : Graph->Nodes)
            if (auto* Event = Cast<UK2Node_ComponentBoundEvent>(Node))
                if (Event->DelegatePropertyName == TEXT("OnInteract_Server")) { check(!InteractionEntry); InteractionEntry = Event; }
    check(InteractionEntry); Install(BP, InteractionEntry, true, Verify);

    for (const TCHAR* Path : {TEXT("/Narrative/DefaultEvents/NE_BeginQuest"), TEXT("/Narrative/DefaultEvents/NE_CompleteNarrativeTask")})
    {
        auto* QuestBP = LoadObject<UBlueprint>(nullptr, Path); check(QuestBP);
        if (!Verify) Backup(QuestBP);
        UK2Node_FunctionEntry* Entry = nullptr;
        for (UEdGraph* Graph : QuestBP->FunctionGraphs)
            if (Graph->GetFName() == TEXT("ExecuteEvent"))
                for (UEdGraphNode* Node : Graph->Nodes) if (auto* E = Cast<UK2Node_FunctionEntry>(Node)) Entry = E;
        check(Entry); Install(QuestBP, Entry, false, Verify);
    }
    return 0;
}
