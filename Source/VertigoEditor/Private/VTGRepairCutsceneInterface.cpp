#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Engine/Blueprint.h"
#include "K2Node_Event.h"
#include "K2Node_VariableSet.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Editor.h"
#include "Components/SkeletalMeshComponent.h"

int32 VTGRepairCutsceneInterface(const FString& Params)
{
    auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/Characters/Sa/ABP_Sa"));
    auto* Interface=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/BPI_CineAnmInterface"));
    if(!BP || !Interface)return 1;
    const bool Verify=Params.Contains(TEXT("VerifyOnly"));
    UEdGraph* G=FBlueprintEditorUtils::FindEventGraph(BP);
    UK2Node_Event* Event=nullptr;
    for(UEdGraphNode* N:G->Nodes)if(auto* E=Cast<UK2Node_Event>(N);E && E->EventReference.GetMemberName()==TEXT("SetIsCutscene"))Event=E;
    if(!Verify)
    {
        if(!BP->GeneratedClass->ImplementsInterface(Interface->GeneratedClass))
            if(!FBlueprintEditorUtils::ImplementNewInterface(BP,Interface->GeneratedClass->GetClassPathName()))return 2;
        if(!Event)
        {
            Event=NewObject<UK2Node_Event>(G);G->AddNode(Event,false,false);Event->CreateNewGuid();
            Event->EventReference.SetExternalMember(TEXT("SetIsCutscene"),Interface->GeneratedClass);
            Event->bOverrideFunction=true;Event->AllocateDefaultPins();Event->NodePosX=-1500;Event->NodePosY=2400;
        }
        auto* Exec=Event->FindPinChecked(TEXT("then"));
        auto* Existing=Exec->LinkedTo.Num()==1?Cast<UK2Node_VariableSet>(Exec->LinkedTo[0]->GetOwningNode()):nullptr;
        if(!Existing || Existing->VariableReference.GetMemberName()!=TEXT("IsCutscene"))
        {
            auto Old=Exec->LinkedTo;Exec->BreakAllPinLinks();
            auto* Set=NewObject<UK2Node_VariableSet>(G);G->AddNode(Set,false,false);Set->CreateNewGuid();
            Set->VariableReference.SetSelfMember(TEXT("IsCutscene"));Set->AllocateDefaultPins();
            Set->NodePosX=Event->NodePosX+330;Set->NodePosY=Event->NodePosY;
            const auto* Schema=GetDefault<UEdGraphSchema_K2>();
            check(Schema->TryCreateConnection(Exec,Set->FindPinChecked(TEXT("execute"))));
            check(Schema->TryCreateConnection(Event->FindPinChecked(TEXT("IsCutscene")),Set->FindPinChecked(TEXT("IsCutscene"))));
            for(auto* Link:Old)check(Schema->TryCreateConnection(Set->FindPinChecked(TEXT("then")),Link));
        }
        else
        {
            const auto* Schema=GetDefault<UEdGraphSchema_K2>();
            if(!Existing->FindPinChecked(TEXT("IsCutscene"))->LinkedTo.Contains(Event->FindPinChecked(TEXT("IsCutscene"))))
            {
                Existing->FindPinChecked(TEXT("IsCutscene"))->BreakAllPinLinks();
                check(Schema->TryCreateConnection(Event->FindPinChecked(TEXT("IsCutscene")),Existing->FindPinChecked(TEXT("IsCutscene"))));
            }
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    }
    FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);if(Log.NumErrors)return 3;
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);
    auto* Mesh=NewObject<USkeletalMeshComponent>();
    auto* Instance=NewObject<UAnimInstance>(Mesh,BP->GeneratedClass);
    auto* Flag=FindFProperty<FBoolProperty>(Instance->GetClass(),TEXT("IsCutscene"));
    auto* Fn=Instance->FindFunction(TEXT("SetIsCutscene"));if(!Flag || !Fn)return 4;
    for(bool Value:{true,false,true,false})
    {
        struct {bool IsCutscene;} Input{Value};Instance->ProcessEvent(Fn,&Input);
        if(Flag->GetPropertyValue_InContainer(Instance)!=Value)return 5;
        UE_LOG(LogTemp,Display,TEXT("CUTSCENE INTERFACE PASS: input %d updates animation state"),Value);
    }
    if(!Verify)
    {
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
        const auto File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        if(!UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args))return 6;
    }
    return 0;
}
