#include "VTGSaCinePoseCommandlet.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_CallFunction.h"
#include "EdGraphSchema_K2.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "UObject/SavePackage.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "Editor.h"
#include "VTGL2AfterFightPoseProbe.inl"
#include "VTGL2EyeTremor.inl"
#include "VTGL2BowerbirdPhysics.inl"

namespace SaCinePose
{
const TCHAR* FallbackPath=TEXT("/Game/Characters/Sa/Anm/Loco/Sa_am_Stand_Idle_02.Sa_am_Stand_Idle_02");
void Test(UAnimBlueprint* BP,bool RequireFixed)
{
 FEditorScriptExecutionGuard Guard;
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("SaCinePoseTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* ActorBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Sa/BP_Cine_Sa"));check(ActorBP);
 auto* A=W->SpawnActor<ACharacter>();auto* M=A->GetMesh();M->SetSkeletalMesh(CastChecked<ACharacter>(ActorBP->GeneratedClass->GetDefaultObject())->GetMesh()->GetSkeletalMeshAsset());M->SetAnimInstanceClass(BP->GeneratedClass);M->InitAnim(true);
 auto* Anim=M->GetAnimInstance();check(Anim);
 auto* Idle=FindFProperty<FObjectPropertyBase>(Anim->GetClass(),TEXT("BaseIdleAnm"));check(Idle);
 auto* Cut=FindFProperty<FBoolProperty>(Anim->GetClass(),TEXT("IsCutscene"));check(Cut);
 auto SetIdle=[&](UObject* Clip){auto* F=Anim->FindFunctionChecked(TEXT("SetBaseIdle"));FStructOnScope Args(F);FindFProperty<FObjectPropertyBase>(F,TEXT("NewIdle"))->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),Clip);Anim->ProcessEvent(F,Args.GetStructMemory());};
 auto Tick=[&](float Dt){M->TickAnimation(Dt,false);M->RefreshBoneTransforms();};
 auto PoseDelta=[&](){float Delta=0.f;int Count=0;auto& Ref=M->GetSkeletalMeshAsset()->GetRefSkeleton();const auto& Pose=M->GetBoneSpaceTransforms();for(int I=0;I<Ref.GetNum();++I){auto N=Ref.GetBoneName(I).ToString();if(N.Contains(TEXT("arm")) || N.Contains(TEXT("hand")) || N.Contains(TEXT("leg")) || N.Contains(TEXT("thigh")) || N.Contains(TEXT("spine")) || N.Contains(TEXT("hips"))){Delta=FMath::Max(Delta,Pose[I].GetRotation().AngularDistance(Ref.GetRefBonePose()[I].GetRotation()));++Count;}}check(Count>5);return Delta;};
 auto AssertPose=[&](const TCHAR* Label){float D=PoseDelta();UE_LOG(LogTemp,Display,TEXT("SACINE POSE %s body-reference max angle=%.5f rad"),Label,D);if(RequireFixed)check(D>.15f);};
 Tick(1.f/60.f);AssertPose(TEXT("initial default"));
 SetIdle(nullptr);Tick(1.f/60.f);AssertPose(TEXT("null idle request"));
 auto* Stair=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L5Clinic/ShoppingCenterYard/ANM/ANM_L5_Sa_OnStairsLookingCool"));check(Stair);SetIdle(Stair);check(Idle->GetObjectPropertyValue_InContainer(Anim)==Stair);
 Cut->SetPropertyValue_InContainer(Anim,true);for(int I=0;I<15;++I)Tick(1.f/60.f);AssertPose(TEXT("cinematic slot without sequence"));
 Cut->SetPropertyValue_InContainer(Anim,false);Tick(0.f);AssertPose(TEXT("exit cinematic same frame"));for(int I=0;I<12;++I){Tick(1.f/60.f);AssertPose(TEXT("exit blend frame"));}
 SetIdle(nullptr);if(RequireFixed)check(Idle->GetObjectPropertyValue_InContainer(Anim)==Stair);
 if(RequireFixed)UE_LOG(LogTemp,Display,TEXT("SACINE TEST PASS: fallback idle, null protection, authored stair idle, cinematic gap and 13 exit frames"));
 W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
}
}
int32 UVTGSaCinePoseCommandlet::Main(const FString& Params)
{
 if(Params.Contains(TEXT("StagedBirdBounds")))
 {
  FPackageName::RegisterMountPoint(TEXT("/VTGBirdBoundsStaged/"),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("Scripts/L2AfterFightPose/Bowerbird/Staged/")));
  StagedBirdMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/VTGBirdBoundsStaged/Bowerbird_RIG_ue_renamed_.Bowerbird_RIG_ue_renamed_"));check(StagedBirdMesh.IsValid());StagedBirdMesh->AddToRoot();
 }
 if(Params.Contains(TEXT("L2EyeTremor")))return AdjustL2EyeTremor(Params);
 if(Params.Contains(TEXT("BowerbirdPhysics")))return ProbeL2BowerbirdPhysics(Params.Contains(TEXT("Cinematic")));
 if(Params.Contains(TEXT("AfterFightRepair")))return RepairL2Return(Params.Contains(TEXT("VerifyOnly")));
 if(Params.Contains(TEXT("AfterFightProbe")))return ProbeL2AfterFightPose(Params.Contains(TEXT("MeshFirst")),Params.Contains(TEXT("RequireFixed")),Params.Contains(TEXT("BirdGameplay")),Params.Contains(TEXT("BirdCinematic")),Params.Contains(TEXT("FixBirdBounds")),Params.Contains(TEXT("VerifyBirdBounds")));
 using namespace SaCinePose;auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/Characters/Sa/ABP_SaCine"));check(BP);
 if(Params.Contains(TEXT("Probe")) || Params.Contains(TEXT("Test"))){Test(BP,!Params.Contains(TEXT("Probe")));return 0;}
 const bool Verify=Params.Contains(TEXT("VerifyOnly"));auto* Fallback=LoadObject<UAnimSequence>(nullptr,FallbackPath);check(Fallback);
 TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);UAnimGraphNode_Slot* Body=nullptr;UAnimGraphNode_SaveCachedPose* Cache=nullptr;
 for(auto* G:Graphs)for(UEdGraphNode* N:G->Nodes){if(auto* S=Cast<UAnimGraphNode_Slot>(N);S && S->Node.SlotName==TEXT("FullBodySlot"))Body=S;if(auto* C=Cast<UAnimGraphNode_SaveCachedPose>(N);C && C->CacheName==TEXT("Cache_BasePosture"))Cache=C;}
 check(Body && Cache);auto* Source=Body->FindPinChecked(TEXT("Source"));
 if(!Verify && Source->LinkedTo.IsEmpty())
 {
  auto* G=Body->GetGraph();auto* Use=NewObject<UAnimGraphNode_UseCachedPose>(G);G->AddNode(Use,false,false);Use->CreateNewGuid();Use->SaveCachedPoseNode=Cache;Use->AllocateDefaultPins();Use->NodePosX=Body->NodePosX-280;Use->NodePosY=Body->NodePosY;Use->NodeComment=TEXT("Keep authored idle alive when Sequencer releases FullBodySlot");Use->bCommentBubbleVisible=true;
  check(G->GetSchema()->TryCreateConnection(Use->FindPinChecked(TEXT("Pose")),Source));Body->Node.bAlwaysUpdateSourcePose=true;
 }
 check(Source->LinkedTo.Num()==1);auto* Use=CastChecked<UAnimGraphNode_UseCachedPose>(Source->LinkedTo[0]->GetOwningNode());check(Use->SaveCachedPoseNode.Get()==Cache && Body->Node.bAlwaysUpdateSourcePose);
 int32 Guards=0;
 for(auto* G:Graphs)
 {
  auto Nodes=G->Nodes;for(UEdGraphNode* N:Nodes)if(auto* Set=Cast<UK2Node_VariableSet>(N);Set && Set->VariableReference.GetMemberName()==TEXT("BaseIdleAnm"))
  {
   auto* Exec=Set->FindPinChecked(TEXT("execute"));bool Guarded=Exec->LinkedTo.Num()==1 && Exec->LinkedTo[0]->GetOwningNode()->NodeComment==TEXT("Preserve previous idle when caller supplies None");
   if(!Guarded && !Verify)
   {
    auto* B=NewObject<UK2Node_IfThenElse>(G);G->AddNode(B,false,false);B->CreateNewGuid();B->AllocateDefaultPins();B->NodePosX=Set->NodePosX-250;B->NodePosY=Set->NodePosY;B->NodeComment=TEXT("Preserve previous idle when caller supplies None");B->bCommentBubbleVisible=true;
    auto* Valid=NewObject<UK2Node_CallFunction>(G);G->AddNode(Valid,false,false);Valid->CreateNewGuid();Valid->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("IsValid")));Valid->AllocateDefaultPins();Valid->NodePosX=B->NodePosX-240;Valid->NodePosY=B->NodePosY+120;
    auto* Schema=GetDefault<UEdGraphSchema_K2>();auto Links=Exec->LinkedTo;Exec->BreakAllPinLinks();for(auto* L:Links)check(Schema->TryCreateConnection(L,B->GetExecPin()));check(Schema->TryCreateConnection(B->GetThenPin(),Exec));
    auto* Value=Set->FindPinChecked(TEXT("BaseIdleAnm"));check(Value->LinkedTo.Num()==1);check(Schema->TryCreateConnection(Value->LinkedTo[0],Valid->FindPinChecked(TEXT("Object"))));check(Schema->TryCreateConnection(Valid->FindPinChecked(TEXT("ReturnValue")),B->GetConditionPin()));Guarded=true;
   }
   check(Guarded);++Guards;
  }
 }
 check(Guards==2);
 if(!Verify)
 {
  auto* P=FindFProperty<FObjectPropertyBase>(BP->GeneratedClass,TEXT("BaseIdleAnm"));check(P);if(!P->GetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject()))P->SetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject(),Fallback);
  const int I=FBlueprintEditorUtils::FindNewVariableIndex(BP,TEXT("BaseIdleAnm"));check(I!=INDEX_NONE);BP->NewVariables[I].DefaultValue=Fallback->GetPathName();
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);check(!Log.NumErrors && BP->Status!=BS_Error);
  P=FindFProperty<FObjectPropertyBase>(BP->GeneratedClass,TEXT("BaseIdleAnm"));P->SetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject(),Fallback);
  const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/SaCinePose/Backup/ABP_SaCine.uasset");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
  FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,A));
 }
 check(FindFProperty<FObjectPropertyBase>(BP->GeneratedClass,TEXT("BaseIdleAnm"))->GetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject()));
 UE_LOG(LogTemp,Display,TEXT("SACINE VERIFY PASS: FullBodySlot has warm idle source, non-null default and two guarded idle writes"));return 0;
}

