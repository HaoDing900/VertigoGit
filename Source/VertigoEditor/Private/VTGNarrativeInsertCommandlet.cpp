#include "VTGNarrativeInsertCommandlet.h"
#include "VTGNarrativeInsert.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_IfThenElse.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace
{
bool ConfigureInstantEvents(bool VerifyOnly)
{
    const auto* Schema=GetDefault<UEdGraphSchema_K2>();
    for (const TCHAR* Name : {TEXT("NE_PlayMontage_Start"), TEXT("NE_PlayMontage_InterruptRes")})
    {
        const FString Path=FString(TEXT("/Game/Narrative/Events/Montage/"))+Name;
        auto* BP=LoadObject<UBlueprint>(nullptr,*Path);
        if (!BP) return false;
        const FString File=FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension());
        if (!VerifyOnly)
        {
            const FString Backup=FPaths::ProjectSavedDir()/TEXT("Backups/NarrativeInstant")/(FString(Name)+TEXT(".uasset"));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
            if (!IFileManager::Get().FileExists(*Backup) && IFileManager::Get().Copy(*Backup,*File)!=COPY_OK) return false;
            if (FBlueprintEditorUtils::FindNewVariableIndex(BP,TEXT("Instant"))==INDEX_NONE)
            {
                FEdGraphPinType Type; Type.PinCategory=UEdGraphSchema_K2::PC_Boolean;
                if (!FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("Instant"),Type,TEXT("false"))) return false;
            }
            FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP,TEXT("Instant"),false);
            FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,TEXT("Instant"),nullptr,TEXT("Tooltip"),TEXT("Immediately replace the montage in the same slot group with no blend-in. Other slot groups and montage assets are unchanged. Insert restoration keeps the original montage settings."));
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        }
        if (FBlueprintEditorUtils::FindNewVariableIndex(BP,TEXT("Instant"))==INDEX_NONE) return false;
        TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
        int32 Count=0;
        for (auto* G:Graphs)
        {
            const auto Nodes=G->Nodes;
            for (UEdGraphNode* N:Nodes) if (auto* Old=Cast<UK2Node_CallFunction>(N))
            {
                const FName Function=Old->FunctionReference.GetMemberName();
                const bool Start=FCString::Strcmp(Name,TEXT("NE_PlayMontage_Start"))==0;
                if (Start && Function==TEXT("Delay"))
                {
                    auto* Exec=Old->FindPinChecked(TEXT("execute"));
                    const bool Wired=Exec->LinkedTo.Num()==1 && Exec->LinkedTo[0]->GetOwningNode()->NodeComment==TEXT("Instant skips this authored startup delay");
                    if (VerifyOnly && !Wired) return false;
                    if (!VerifyOnly && !Wired)
                    {
                        auto* Gate=NewObject<UK2Node_IfThenElse>(G); G->AddNode(Gate,false,false); Gate->CreateNewGuid(); Gate->AllocateDefaultPins();
                        Gate->NodePosX=Old->NodePosX-240; Gate->NodePosY=Old->NodePosY;
                        Gate->NodeComment=TEXT("Instant skips this authored startup delay");
                        const auto Inputs=Exec->LinkedTo; Exec->BreakAllPinLinks();
                        for (auto* Link:Inputs) if (!Schema->TryCreateConnection(Link,Gate->GetExecPin())) return false;
                        if (!Schema->TryCreateConnection(Gate->GetElsePin(),Exec)) return false;
                        for (auto* Link:Old->FindPinChecked(TEXT("then"))->LinkedTo)
                            if (!Schema->TryCreateConnection(Gate->GetThenPin(),Link)) return false;
                        auto* Get=NewObject<UK2Node_VariableGet>(G); G->AddNode(Get,false,false); Get->CreateNewGuid();
                        Get->VariableReference.SetSelfMember(TEXT("Instant")); Get->AllocateDefaultPins();
                        Get->NodePosX=Gate->NodePosX-180; Get->NodePosY=Gate->NodePosY+100;
                        if (!Schema->TryCreateConnection(Get->FindPinChecked(TEXT("Instant")),Gate->GetConditionPin())) return false;
                    }
                    continue;
                }
                if ((Start && Function!=TEXT("PlayAnimMontage") && Function!=TEXT("PlayNarrativeMontage")) ||
                    (!Start && Function!=TEXT("PlayNarrativeInsert"))) continue;
                ++Count;
                UK2Node_CallFunction* Call=Old;
                if (!VerifyOnly && Start && Function==TEXT("PlayAnimMontage"))
                {
                    Call=NewObject<UK2Node_CallFunction>(G); G->AddNode(Call,false,false); Call->CreateNewGuid();
                    Call->SetFromFunction(UVTGNarrativeInsertLibrary::StaticClass()->FindFunctionByName(TEXT("PlayNarrativeMontage")));
                    Call->AllocateDefaultPins(); Call->NodePosX=Old->NodePosX; Call->NodePosY=Old->NodePosY;
                    for (auto* From:Old->Pins)
                    {
                        if (From->bOrphanedPin) continue;
                        auto* To=Call->FindPin(From->PinName==UEdGraphSchema_K2::PN_Self?FName(TEXT("Character")):From->PinName);
                        if (!To) { if (!From->LinkedTo.IsEmpty()) return false; continue; }
                        To->DefaultValue=From->DefaultValue; To->DefaultObject=From->DefaultObject; To->DefaultTextValue=From->DefaultTextValue;
                        const auto Links=From->LinkedTo; From->BreakAllPinLinks();
                        for (auto* Link:Links) if (!Schema->TryCreateConnection(Link,To)) return false;
                    }
                    FBlueprintEditorUtils::RemoveNode(BP,Old,true);
                }
                if (!VerifyOnly)
                {
                    Call->ReconstructNode();
                    auto* Pin=Call->FindPin(TEXT("Instant")); if (!Pin) return false;
                    if (Pin->LinkedTo.IsEmpty())
                    {
                        auto* Get=NewObject<UK2Node_VariableGet>(G); G->AddNode(Get,false,false); Get->CreateNewGuid();
                        Get->VariableReference.SetSelfMember(TEXT("Instant")); Get->AllocateDefaultPins();
                        Get->NodePosX=Call->NodePosX-220; Get->NodePosY=Call->NodePosY+240;
                        if (!Schema->TryCreateConnection(Get->FindPinChecked(TEXT("Instant")),Pin)) return false;
                    }
                }
                auto* Pin=Call->FindPin(TEXT("Instant"));
                if (!Pin || Pin->LinkedTo.Num()!=1 || !Cast<UK2Node_VariableGet>(Pin->LinkedTo[0]->GetOwningNode())) return false;
            }
        }
        if (!Count) return false;
        FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);
        if (Log.NumErrors) return false;
        if (!VerifyOnly)
        {
            FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
            if (!UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args)) return false;
        }
        UE_LOG(LogTemp,Display,TEXT("INSTANT EVENT PASS %s: %d playback calls wired, Blueprint compiled"),Name,Count);
    }
    return true;
}
}

bool VTGTestNarrativeInsert();
int32 UVTGNarrativeInsertCommandlet::Main(const FString& Params)
{
    if (Params.Contains(TEXT("InstantEvents"))) return ConfigureInstantEvents(Params.Contains(TEXT("VerifyOnly"))) ? 0 : 20;
    if (Params.Contains(TEXT("Test"))) return VTGTestNarrativeInsert() ? 0 : 10;
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/Events/Montage/NE_PlayMontage_InterruptRes.NE_PlayMontage_InterruptRes"));
    if (!BP) return 1;
    UEdGraph* G=BP->UbergraphPages[0];
    UK2Node_Event* Entry=nullptr;
    for (UEdGraphNode* N:G->Nodes) if (auto* E=Cast<UK2Node_Event>(N); E && E->EventReference.GetMemberName()==TEXT("ExecuteEvent")) Entry=E;
    if (!Entry) return 2;
    const auto* Schema=GetDefault<UEdGraphSchema_K2>();
    if (!Params.Contains(TEXT("VerifyOnly")))
    {
        Entry->FindPinChecked(TEXT("then"))->BreakAllPinLinks();
        auto* Call=NewObject<UK2Node_CallFunction>(G); G->AddNode(Call,false,false); Call->CreateNewGuid();
        Call->SetFromFunction(UVTGNarrativeInsertLibrary::StaticClass()->FindFunctionByName(TEXT("PlayNarrativeInsert")));
        Call->AllocateDefaultPins(); Call->NodePosX=Entry->NodePosX+380; Call->NodePosY=Entry->NodePosY-240;
        Call->NodeComment=TEXT("Newest insert replaces the previous insert; character owns restoration across event instances.");
        if (!Schema->TryCreateConnection(Entry->FindPinChecked(TEXT("then")),Call->FindPinChecked(TEXT("execute")))) return 3;
        for (const FName Name:{FName(TEXT("TargetIdentity")),FName(TEXT("InterruptMontage"))})
        {
            auto* Get=NewObject<UK2Node_VariableGet>(G); G->AddNode(Get,false,false); Get->CreateNewGuid();
            Get->VariableReference.SetSelfMember(Name); Get->AllocateDefaultPins();
            Get->NodePosX=Entry->NodePosX; Get->NodePosY=Call->NodePosY+(Name==TEXT("TargetIdentity")?100:180);
            if (!Schema->TryCreateConnection(Get->FindPinChecked(Name),Call->FindPinChecked(Name))) return 4;
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    }
    auto* Exec=Entry->FindPinChecked(TEXT("then"));
    if (Exec->LinkedTo.Num()!=1) return 5;
    auto* Call=Cast<UK2Node_CallFunction>(Exec->LinkedTo[0]->GetOwningNode());
    if (!Call || Call->FunctionReference.GetMemberName()!=TEXT("PlayNarrativeInsert")) return 6;
    FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);
    if (Log.NumErrors) return 7;
    if (!Params.Contains(TEXT("VerifyOnly")))
    {
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        const auto File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        if (!UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args)) return 8;
    }
    UE_LOG(LogTemp,Display,TEXT("NARRATIVE INSERT PASS: event routed to character-owned replacement, Blueprint compiled"));
    return 0;
}
