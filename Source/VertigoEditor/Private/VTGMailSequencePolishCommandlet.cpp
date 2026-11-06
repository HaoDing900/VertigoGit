#include "VTGMailSequencePolishCommandlet.h"
#include "VTGMailReadingComponent.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "Sound/SoundBase.h"

namespace MailSequencePolish
{
const UEdGraphSchema_K2* Schema(){return GetDefault<UEdGraphSchema_K2>();}
UEdGraphNode* Find(UEdGraph* G,const TCHAR* Name){for(UEdGraphNode* N:G->Nodes)if(N->GetFName()==Name)return N;checkf(false,TEXT("Missing %s"),Name);return nullptr;}
UEdGraphPin* P(UEdGraphNode* N,const TCHAR* Name){auto* Pin=N->FindPin(Name);checkf(Pin,TEXT("Missing pin %s on %s"),Name,*N->GetName());return Pin;}
void Link(UEdGraphPin* A,UEdGraphPin* B){check(Schema()->TryCreateConnection(A,B));}
UK2Node_CallFunction* Call(UEdGraph* G,const TCHAR* Name,const TCHAR* Fn,int X,int Y)
{
    auto* N=NewObject<UK2Node_CallFunction>(G,FName(Name));G->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;
    N->SetFromFunction(UVTGMailReadingComponent::StaticClass()->FindFunctionByName(Fn));N->AllocateDefaultPins();return N;
}
UK2Node_Self* Self(UEdGraph* G,int X,int Y)
{auto* N=NewObject<UK2Node_Self>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;N->AllocateDefaultPins();return N;}
void Backup(UBlueprint* BP)
{
    const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    const FString Dest=FPaths::ProjectDir()/TEXT("Scripts/MailSequencePolish/Backup")/(BP->GetName()+TEXT(".uasset"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest),true);
    if(!IFileManager::Get().FileExists(*Dest))check(IFileManager::Get().Copy(*Dest,*File,false)==COPY_OK);
}
void CompileSave(UBlueprint* BP,bool Verify)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);check(Log.NumErrors==0);
    if(!Verify){const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args));}
    UE_LOG(LogTemp,Display,TEXT("MAIL_SEQUENCE_ASSET_PASS %s errors=%d warnings=%d"),*BP->GetName(),Log.NumErrors,Log.NumWarnings);
}
void Patch(bool Verify)
{
    auto* Player=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(Player);
    auto* Fight=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight"));check(Fight);
    if(!Verify){Backup(Player);Backup(Fight);}
    auto* G=FBlueprintEditorUtils::FindEventGraph(Player);check(G);
    for(const TCHAR* Name:{TEXT("K2Node_CallFunction_0"),TEXT("K2Node_CallFunction_70"),TEXT("K2Node_CallFunction_210")})
    {
        auto* Blend=CastChecked<UK2Node_CallFunction>(Find(G,Name));check(Blend->FunctionReference.GetMemberName()==TEXT("SetViewTargetWithBlend"));
        const FString HelperName=TEXT("VTG_MailBlendTime_")+FString(Name);
        UK2Node_CallFunction* Helper=nullptr;for(UEdGraphNode* N:G->Nodes)if(N->GetName()==HelperName)Helper=Cast<UK2Node_CallFunction>(N);
        if(!Helper && !Verify)
        {
            Helper=Call(G,*HelperName,TEXT("GetMailCameraBlendTime"),Blend->NodePosX-300,Blend->NodePosY+240);
            Link(P(Self(G,Helper->NodePosX-200,Helper->NodePosY),TEXT("self")),P(Helper,TEXT("Reader")));
            Schema()->TrySetDefaultValue(*P(Helper,TEXT("bEnteringMail")),FString(Name)==TEXT("K2Node_CallFunction_0")?TEXT("true"):TEXT("false"));
            for(auto* Target:P(Blend,TEXT("NewViewTarget"))->LinkedTo)Link(Target,P(Helper,TEXT("Target")));
            P(Blend,TEXT("BlendTime"))->BreakAllPinLinks();Link(P(Helper,TEXT("ReturnValue")),P(Blend,TEXT("BlendTime")));
        }
        check(Helper && P(Blend,TEXT("BlendTime"))->LinkedTo.Contains(P(Helper,TEXT("ReturnValue"))));
    }
    UEdGraph* MailGraph=nullptr;for(UEdGraph* Graph:Player->FunctionGraphs)if(Graph->GetFName()==TEXT("SetMailData"))MailGraph=Graph;check(MailGraph);
    auto* Entry=Find(MailGraph,TEXT("K2Node_FunctionEntry_0"));
    UK2Node_CallFunction* Notify=nullptr;for(UEdGraphNode* N:MailGraph->Nodes)if(N->GetFName()==TEXT("VTG_NotifyNewMail"))Notify=Cast<UK2Node_CallFunction>(N);
    if(!Notify && !Verify)
    {
        Notify=Call(MailGraph,TEXT("VTG_NotifyNewMail"),TEXT("NotifyNewMail"),Entry->NodePosX+240,Entry->NodePosY+220);
        auto Old=P(Entry,TEXT("then"))->LinkedTo;P(Entry,TEXT("then"))->BreakAllPinLinks();Link(P(Entry,TEXT("then")),P(Notify,TEXT("execute")));for(auto* To:Old)Link(P(Notify,TEXT("then")),To);
        Link(P(Self(MailGraph,Entry->NodePosX,Entry->NodePosY+380),TEXT("self")),P(Notify,TEXT("Reader")));
        Link(P(Entry,TEXT("MailID")),P(Notify,TEXT("MailID")));Link(P(Entry,TEXT("NewMail?")),P(Notify,TEXT("bHasNewMail")));
        auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/SFX/Character/Ting_Coins.Ting_Coins"));check(Sound);Schema()->TrySetDefaultObject(*P(Notify,TEXT("NotificationSound")),Sound);
    }
    check(Notify && P(Entry,TEXT("then"))->LinkedTo.Contains(P(Notify,TEXT("execute"))) && P(Notify,TEXT("NotificationSound"))->DefaultObject);
    auto* FG=FBlueprintEditorUtils::FindEventGraph(Fight);check(FG);
    auto* Activate=Find(FG,TEXT("K2Node_CallFunction_71"));auto* Finished=Find(FG,TEXT("K2Node_CustomEvent_5"));auto* Bind=Find(FG,TEXT("K2Node_AssignDelegate_0"));
    if(!Verify && !P(Finished,TEXT("then"))->LinkedTo.Contains(P(Activate,TEXT("execute"))))
    {
        check(P(Bind,TEXT("then"))->LinkedTo.Contains(P(Activate,TEXT("execute"))));
        P(Activate,TEXT("execute"))->BreakAllPinLinks();auto Old=P(Finished,TEXT("then"))->LinkedTo;P(Finished,TEXT("then"))->BreakAllPinLinks();
        Link(P(Finished,TEXT("then")),P(Activate,TEXT("execute")));for(auto* To:Old)Link(P(Activate,TEXT("then")),To);
        Activate->NodePosX=Finished->NodePosX+240;Activate->NodePosY=Finished->NodePosY;
        Activate->NodeComment=TEXT("Only reveal/enable the arena after the combat intro Sequence finishes. Checkpoint resume keeps its separate immediate activation.");
    }
    check(P(Finished,TEXT("then"))->LinkedTo.Contains(P(Activate,TEXT("execute"))));check(!P(Bind,TEXT("then"))->LinkedTo.Contains(P(Activate,TEXT("execute"))));
    check(P(Find(FG,TEXT("K2Node_CallFunction_60")),TEXT("then"))->LinkedTo.Contains(P(Find(FG,TEXT("VTG_CheckpointActivateArena")),TEXT("execute"))));
    CompileSave(Player,Verify);CompileSave(Fight,Verify);
}
}
extern bool VTGVerifyMailSequencePolish();
extern bool VTGVerifyCorvosSequenceRegression();
extern int32 VTGPolishPauseMenu(const FString& Params);
extern int32 VTGPoliceFootsteps(const FString& Params);
extern int32 VTGPoliceContractProbe(const FString& Params);
extern int32 VTGRepairDialogueDodgeReset(const FString& Params);
extern int32 VTGSaFootstepPolish(const FString& Params);
UVTGMailSequencePolishCommandlet::UVTGMailSequencePolishCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGMailSequencePolishCommandlet::Main(const FString& Params)
{
    if(FParse::Param(*Params,TEXT("FootstepPolish")))return VTGSaFootstepPolish(Params);
    if(FParse::Param(*Params,TEXT("DodgeReset")))return VTGRepairDialogueDodgeReset(Params);
    if(FParse::Param(*Params,TEXT("ContractProbe")))return VTGPoliceContractProbe(Params);
    if(FParse::Param(*Params,TEXT("PoliceFootsteps")))return VTGPoliceFootsteps(Params);
    if(FParse::Param(*Params,TEXT("PauseMenu")))return VTGPolishPauseMenu(Params);
    if(FParse::Param(*Params,TEXT("CorvosRegression")))return VTGVerifyCorvosSequenceRegression()?0:1;
    if(FParse::Param(*Params,TEXT("RuntimeTests")))return VTGVerifyMailSequencePolish()?0:1;
    MailSequencePolish::Patch(FParse::Param(*Params,TEXT("Verify")));return 0;
}
