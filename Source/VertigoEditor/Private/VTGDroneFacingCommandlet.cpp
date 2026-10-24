#include "VTGDroneFacingCommandlet.h"
#include "Characters/VTGMovementFacing.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Self.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace DroneFacing
{
const FName FunctionName(TEXT("GetMovementFacingRotation"));
template<class T> T* Add(UEdGraph* G, int X, int Y)
{
    auto* N=NewObject<T>(G); G->AddNode(N,false,false); N->CreateNewGuid();
    N->NodePosX=X; N->NodePosY=Y; return N;
}
void Link(UEdGraphNode* A, const TCHAR* AP, UEdGraphNode* B, const TCHAR* BP)
{
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A->FindPinChecked(AP),B->FindPinChecked(BP)));
}
UEdGraphNode* Named(UEdGraph* G,const TCHAR* Name)
{
    for(UEdGraphNode* N:G->Nodes)if(N->GetName()==Name)return N;
    return nullptr;
}
void AddParentFunction(UBlueprint* Parent)
{
    for(UEdGraph* G:Parent->FunctionGraphs)if(G->GetFName()==FunctionName)return;
    auto* G=FBlueprintEditorUtils::CreateNewGraph(Parent,FunctionName,UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph(Parent,G,false,static_cast<UClass*>(nullptr));
    UK2Node_FunctionEntry* Entry=nullptr;
    for(UEdGraphNode* N:G->Nodes)if(auto* E=Cast<UK2Node_FunctionEntry>(N))Entry=E;
    check(Entry);
    Entry->AddExtraFlags(FUNC_Public | FUNC_BlueprintCallable | FUNC_BlueprintPure);
    Entry->MetaData.Category=FText::FromString(TEXT("Movement|Facing"));
    FEdGraphPinType Rot; Rot.PinCategory=UEdGraphSchema_K2::PC_Struct; Rot.PinSubCategoryObject=TBaseStructure<FRotator>::Get();
    Entry->CreateUserDefinedPin(TEXT("DesiredWorldRotation"),Rot,EGPD_Output);
    auto* Result=Add<UK2Node_FunctionResult>(G,720,0);
    Result->FunctionReference.SetSelfMember(FunctionName); Result->AllocateDefaultPins();
    Result->CreateUserDefinedPin(TEXT("TargetRelativeRotation"),Rot,EGPD_Input);
    auto* Call=Add<UK2Node_CallFunction>(G,360,80);
    Call->SetFromFunction(UVTGMovementFacing::StaticClass()->FindFunctionByName(TEXT("MovementFacingRotation"))); Call->AllocateDefaultPins();
    auto* Self=Add<UK2Node_Self>(G,0,160); Self->AllocateDefaultPins();
    auto* Offset=Add<UK2Node_VariableGet>(G,0,240);
    Offset->VariableReference.SetSelfMember(TEXT("FacingYawOffsetForFollow")); Offset->AllocateDefaultPins();
    Call->NodeComment=TEXT("Shared with Follow. Input is visual WORLD heading; output is Capsule RELATIVE rotation. Mesh transform is unchanged.");
    Link(Self,TEXT("self"),Call,TEXT("Character"));
    Link(Offset,TEXT("FacingYawOffsetForFollow"),Call,TEXT("FacingYawOffset"));
    Link(Entry,TEXT("DesiredWorldRotation"),Call,TEXT("DesiredWorldRotation"));
    Link(Call,TEXT("ReturnValue"),Result,TEXT("TargetRelativeRotation"));
    Link(Entry,TEXT("then"),Result,TEXT("execute"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Parent);
}
bool Compile(UBlueprint* BP)
{
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);
    UE_LOG(LogTemp,Display,TEXT("DRONE compile %s errors=%d warnings=%d"),*BP->GetName(),Log.NumErrors,Log.NumWarnings);
    return !Log.NumErrors && BP->Status!=BS_Error;
}
bool Save(UBlueprint* BP)
{
    const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/DroneFacing/Backup")/(BP->GetName()+TEXT(".uasset"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
    if(!IFileManager::Get().FileExists(*Backup) && IFileManager::Get().Copy(*Backup,*File)!=COPY_OK)return false;
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args);
}
}
UVTGDroneFacingCommandlet::UVTGDroneFacingCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGDroneFacingCommandlet::Main(const FString& Params)
{
    using namespace DroneFacing;
    auto* Parent=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/BP_StillNPC.BP_StillNPC"));
    auto* Drone=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Drone_IPHA/BP_Drone_IPHA_NPC.BP_Drone_IPHA_NPC"));
    auto* Market=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket.BPLM_BlackMarket"));
    if(!Parent || !Drone || !Market)return 1;
    const FTransform OriginalMesh=CastChecked<ACharacter>(Drone->GeneratedClass->GetDefaultObject())->GetMesh()->GetRelativeTransform();
    auto OffsetValue=[&](){auto* P=FindFProperty<FDoubleProperty>(Drone->GeneratedClass,TEXT("FacingYawOffsetForFollow"));check(P);return P->GetPropertyValue_InContainer(Drone->GeneratedClass->GetDefaultObject());};
    const double OriginalOffset=OffsetValue();
    UE_LOG(LogTemp,Display,TEXT("DRONE existing Follow offset=%f mesh=%s"),OriginalOffset,*OriginalMesh.ToString());
    if(!Params.Contains(TEXT("VerifyOnly")))AddParentFunction(Parent);
    if(!Compile(Parent) || !Compile(Drone))return 2;
    UEdGraph* Graph=nullptr;for(UEdGraph* G:Market->UbergraphPages)if(G->GetName()==TEXT("EventGraph"))Graph=G;
    if(!Graph)return 3;
    auto* Move=Named(Graph,TEXT("K2Node_CallFunction_154"));
    auto* Look=Named(Graph,TEXT("K2Node_CallFunction_167"));
    auto* DroneGet=Named(Graph,TEXT("K2Node_VariableGet_94"));
    if(!Move || !Look || !DroneGet)return 4;
    auto* Target=Move->FindPinChecked(TEXT("TargetRelativeRotation"));
    bool Installed=false;
    for(auto* P:Target->LinkedTo)if(auto* C=Cast<UK2Node_CallFunction>(P->GetOwningNode()))Installed|=C->FunctionReference.GetMemberName()==FunctionName;
    if(!Installed)
    {
        if(Params.Contains(TEXT("VerifyOnly")))return 5;
        if(Target->LinkedTo.Num()!=1 || Target->LinkedTo[0]->GetOwningNode()->GetName()!=TEXT("K2Node_CallFunction_RotationOffset"))return 6;
        auto* Call=Add<UK2Node_CallFunction>(Graph,9472,9008);
        Call->SetFromFunction(Parent->GeneratedClass->FindFunctionByName(FunctionName));Call->AllocateDefaultPins();
        Call->NodeComment=TEXT("Use parent movement facing, shared with Follow; preserve mesh orientation.");
        Target->BreakAllPinLinks();
        Link(DroneGet,TEXT("IPHADrone"),Call,TEXT("self"));
        Link(Look,TEXT("ReturnValue"),Call,TEXT("DesiredWorldRotation"));
        Link(Call,TEXT("TargetRelativeRotation"),Move,TEXT("TargetRelativeRotation"));
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Market);
    }
    if(!Compile(Market))return 7;
    check(OriginalMesh.Equals(CastChecked<ACharacter>(Drone->GeneratedClass->GetDefaultObject())->GetMesh()->GetRelativeTransform()));
    check(FMath::IsNearlyEqual(OriginalOffset,OffsetValue()));
    // Exercise the actual parent Blueprint function, including its variable getter.
    auto* CDO=CastChecked<ACharacter>(Drone->GeneratedClass->GetDefaultObject());
    auto* Function=CDO->FindFunction(FunctionName);if(!Function)return 8;
    TGuardValue<bool> AllowScript(GAllowActorScriptExecutionInEditor,true);
    for(double Yaw:{-170.,0.,90.,179.})
    {
        struct FArgs{FRotator DesiredWorldRotation;FRotator TargetRelativeRotation;};
        FArgs Args{FRotator(0,Yaw,0),FRotator::ZeroRotator};CDO->ProcessEvent(Function,&Args);
        const double Expected=FRotator::NormalizeAxis(Yaw+OriginalOffset);
        if(FMath::Abs(FMath::FindDeltaAngleDegrees(Args.TargetRelativeRotation.Yaw,Expected))>.001)return 9;
        UE_LOG(LogTemp,Display,TEXT("DRONE TEST PASS heading=%f result=%f"),Yaw,Args.TargetRelativeRotation.Yaw);
    }
    UE_LOG(LogTemp,Display,TEXT("DRONE TEST PASS mesh transform and Follow offset unchanged"));
    if(!Params.Contains(TEXT("VerifyOnly")) && (!Save(Parent)||!Save(Market)))return 10;
    UE_LOG(LogTemp,Display,TEXT("DRONE_FACING_SUCCESS"));return 0;
}

