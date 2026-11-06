#include "VTGSaBlinkCommandlet.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimNodes/AnimNode_LayeredBoneBlend.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/FileManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace SaBlink
{
const FName AllowName(TEXT("Seq allow blink?"));
const FString GateComment(TEXT("VTG: auto blink requires curve permission and (!IsCutscene || Seq allow blink?)"));

template<class T> T* Add(UEdGraph* Graph, int32 X, int32 Y)
{
    auto* Node=NewObject<T>(Graph);
    Graph->AddNode(Node,false,false);
    Node->CreateNewGuid();
    Node->NodePosX=X; Node->NodePosY=Y;
    return Node;
}
UK2Node_CallFunction* Call(UEdGraph* Graph, FName Function, int32 X, int32 Y)
{
    auto* Node=Add<UK2Node_CallFunction>(Graph,X,Y);
    Node->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(Function));
    Node->AllocateDefaultPins();
    return Node;
}
UK2Node_VariableGet* Variable(UEdGraph* Graph, FName Name, int32 X, int32 Y)
{
    auto* Node=Add<UK2Node_VariableGet>(Graph,X,Y);
    Node->VariableReference.SetSelfMember(Name);
    Node->AllocateDefaultPins();
    return Node;
}
void Link(UEdGraphNode* A, FName APin, UEdGraphNode* B, FName BPin)
{
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A->FindPinChecked(APin),B->FindPinChecked(BPin)));
}
void CheckRuntime(UAnimBlueprint* BP)
{
    FEditorScriptExecutionGuard Guard;
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("SaBlinkCheck"),nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* ActorBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Sa/BP_Cine_Sa"));check(ActorBP);
    auto* Actor=World->SpawnActor<ACharacter>();
    auto* Mesh=Actor->GetMesh();
    Mesh->SetSkeletalMesh(CastChecked<ACharacter>(ActorBP->GeneratedClass->GetDefaultObject())->GetMesh()->GetSkeletalMeshAsset());
    Mesh->SetAnimInstanceClass(BP->GeneratedClass); Mesh->InitAnim(true);
    auto* Anim=Mesh->GetAnimInstance();check(Anim);
    auto* Cut=FindFProperty<FBoolProperty>(Anim->GetClass(),TEXT("IsCutscene"));
    auto* Allow=FindFProperty<FBoolProperty>(Anim->GetClass(),AllowName);check(Cut && Allow);
    check(!Allow->GetPropertyValue_InContainer(Anim));
    auto* VisualNode=FindObject<UAnimGraphNode_LayeredBoneBlend>(BP,TEXT("AnimGraph.AnimGraphNode_LayeredBoneBlend_6"));
    check(VisualNode && VisualNode->Node.LayerSetup[0].BranchFilters[0].BoneName==TEXT("head_x"));
    auto* AnimClass=CastChecked<UAnimBlueprintGeneratedClass>(Anim->GetClass());
    auto* BlinkLayer=AnimClass->GetPropertyInstance<FAnimNode_LayeredBoneBlend>(Anim,VisualNode);
    check(BlinkLayer);
    auto Check=[&](bool IsCutscene,bool Allowed,float Expected)
    {
        Cut->SetPropertyValue_InContainer(Anim,IsCutscene);
        Allow->SetPropertyValue_InContainer(Anim,Allowed);
        for(int32 Frame=0;Frame<24;++Frame){Mesh->TickAnimation(1.f/60.f,false);Mesh->RefreshBoneTransforms();}
        check(BlinkLayer->BlendWeights.Num()==1);
        const float Weight=BlinkLayer->BlendWeights[0];
        UE_LOG(LogTemp,Display,TEXT("SA BLINK RUNTIME cutscene=%d allow=%d weight=%.3f expected=%.3f"),IsCutscene,Allowed,Weight,Expected);
        check(FMath::IsNearlyEqual(Weight,Expected));
    };
    Check(false,false,1.f);
    Check(true,false,0.f);
    Check(true,true,1.f);
    Check(true,false,0.f);
    Check(false,false,1.f);
    Check(false,true,1.f);
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
}
}

UVTGSaBlinkCommandlet::UVTGSaBlinkCommandlet()
{IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}

int32 UVTGSaBlinkCommandlet::Main(const FString& Params)
{
    using namespace SaBlink;
    const bool Verify=FParse::Param(*Params,TEXT("VerifyOnly"));
    auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/Characters/Sa/ABP_Sa"));check(BP);
    const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    if(!Verify)
    {
        const FString Backup=FPaths::ProjectSavedDir()/TEXT("QA/BlinkCutscene/ABP_Sa.before-cutscene-blink.uasset");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
        if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
        if(FBlueprintEditorUtils::FindNewVariableIndex(BP,AllowName)==INDEX_NONE)
        {
            FEdGraphPinType Type;Type.PinCategory=UEdGraphSchema_K2::PC_Boolean;
            check(FBlueprintEditorUtils::AddMemberVariable(BP,AllowName,Type,TEXT("false")));
        }
        FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP,AllowName,false);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(BP,AllowName,nullptr,FText::FromString(TEXT("Cutscene")));
        FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,AllowName,nullptr,TEXT("ToolTip"),
            TEXT("Allow the existing automatic blink layer during cutscenes. Off by default. Disable Auto Blink curves still take priority; authored facial animation is unaffected."));
        auto& Description=BP->NewVariables[FBlueprintEditorUtils::FindNewVariableIndex(BP,AllowName)];
        Description.PropertyFlags|=CPF_Interp;
    }
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
    UK2Node_CallFunction* Convert=nullptr;
    for(auto* Graph:Graphs)if(Graph->GetFName()==TEXT("AnimGraph"))
        for(UEdGraphNode* Node:Graph->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(Node))
            if(C->FunctionReference.GetMemberName()==TEXT("Conv_BoolToDouble"))
            {
                auto* Input=C->FindPinChecked(TEXT("InBool"));
                if(Input->LinkedTo.Num()==1)
                {
                    auto* Source=Input->LinkedTo[0]->GetOwningNode();
                    if(Source->NodeComment==GateComment || Source->GetFName()==TEXT("K2Node_PromotableOperator_0"))
                    {check(!Convert);Convert=C;}
                }
            }
    check(Convert);
    auto* Input=Convert->FindPinChecked(TEXT("InBool"));
    if(Input->LinkedTo[0]->GetOwningNode()->NodeComment!=GateComment)
    {
        check(!Verify);
        auto* CurveAllowed=Input->LinkedTo[0];
        auto* Graph=Convert->GetGraph();
        const int32 X=Convert->NodePosX,Y=Convert->NodePosY;
        auto* Cut=Variable(Graph,TEXT("IsCutscene"),X-850,Y+180);
        auto* Allow=Variable(Graph,AllowName,X-850,Y+300);
        auto* NotCut=Call(Graph,TEXT("Not_PreBool"),X-600,Y+180);
        auto* Or=Call(Graph,TEXT("BooleanOR"),X-400,Y+180);
        auto* Gate=Call(Graph,TEXT("BooleanAND"),X-200,Y);
        Gate->NodeComment=GateComment;Gate->bCommentBubbleVisible=true;
        Input->BreakAllPinLinks();
        check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(CurveAllowed,Gate->FindPinChecked(TEXT("A"))));
        Link(Cut,TEXT("IsCutscene"),NotCut,TEXT("A"));
        Link(NotCut,TEXT("ReturnValue"),Or,TEXT("A"));
        Link(Allow,AllowName,Or,TEXT("B"));
        Link(Or,TEXT("ReturnValue"),Gate,TEXT("B"));
        Link(Gate,TEXT("ReturnValue"),Convert,TEXT("InBool"));
    }
    if(!Verify)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);
        check(!Log.NumErrors && BP->Status!=BS_Error);
    }
    auto* Allow=FindFProperty<FBoolProperty>(BP->GeneratedClass,AllowName);
    check(Allow && Allow->HasAnyPropertyFlags(CPF_Interp));
    check(!Allow->GetPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject()));
    CheckRuntime(BP);
    if(!Verify)
    {
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
        check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args));
    }
    UE_LOG(LogTemp,Display,TEXT("SA BLINK PASS: cutscene blocks auto blink by default; Seq allow blink? opts in; gameplay, curve gate and exit behavior preserved"));
    return 0;
}
