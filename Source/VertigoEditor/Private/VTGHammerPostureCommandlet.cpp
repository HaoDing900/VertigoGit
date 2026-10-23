#include "VTGHammerPostureCommandlet.h"
#include "Animation/VTGCameraLookAnimInstance.h"
#include "AnimGraphNode_ModifyBone.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace HammerPosture
{
const FString Marker(TEXT("VTG Hammer spine X correction"));
void Connect(UEdGraph* G,UEdGraphPin* A,UEdGraphPin* B){check(G->GetSchema()->TryCreateConnection(A,B));}
template<class T>T* Add(UEdGraph* G,int X,int Y){auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->PostPlacedNewNode();N->NodeComment=Marker;N->NodePosX=X;N->NodePosY=Y;return N;}
void Apply(UAnimBlueprint* BP,UEdGraph* G)
{
    for(UEdGraphNode* N:G->Nodes)if(N->NodeComment==Marker)return;
    const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/HammerPosture/ABP_Sa.before.uasset");if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
    // Insert before existing camera neck/head controls, after all authored locomotion and montage slots.
    UAnimGraphNode_ModifyBone* Neck=nullptr;for(UEdGraphNode* N:G->Nodes)if(auto* B=Cast<UAnimGraphNode_ModifyBone>(N);B && B->NodeComment==TEXT("VTG Camera Look") && B->Node.BoneToModify.BoneName==TEXT("neck_x"))Neck=B;check(Neck);
    auto* Input=Neck->FindPinChecked(TEXT("ComponentPose"));check(Input->LinkedTo.Num()==1);auto* Previous=Input->LinkedTo[0];
    auto* Bone=Add<UAnimGraphNode_ModifyBone>(G,Neck->NodePosX-300,Neck->NodePosY);Bone->Node.BoneToModify.BoneName=TEXT("spine_02_x");Bone->Node.RotationMode=BMM_Additive;Bone->Node.RotationSpace=BCS_BoneSpace;Bone->Node.TranslationMode=BMM_Ignore;Bone->Node.ScaleMode=BMM_Ignore;Bone->AllocateDefaultPins();
    Input->BreakAllPinLinks();Connect(G,Previous,Bone->FindPinChecked(TEXT("ComponentPose")));Connect(G,Bone->FindPinChecked(TEXT("Pose")),Input);
    for(const auto& Pair:{TPair<FName,FName>(TEXT("HammerSpineRotation"),TEXT("Rotation")),TPair<FName,FName>(TEXT("HammerSpineAlpha"),TEXT("Alpha"))}){auto* Get=Add<UK2Node_VariableGet>(G,Bone->NodePosX-100,Bone->NodePosY+(Pair.Key==TEXT("HammerSpineRotation")?250:400));Get->VariableReference.SetSelfMember(Pair.Key);Get->AllocateDefaultPins();Connect(G,Get->GetValuePin(),Bone->FindPinChecked(Pair.Value));}
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);check(!Log.NumErrors);
    BP->GetOutermost()->FullyLoad();ResetLoaders(BP->GetOutermost());BP->MarkPackageDirty();FSavePackageArgs Save;Save.TopLevelFlags=RF_Public|RF_Standalone;Save.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,Save));
}
bool Test(UAnimBlueprint* BP)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("HammerPostureRegression"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;auto* PlayerBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));auto* Player=World->SpawnActor<ACharacter>(PlayerBP->GeneratedClass,FTransform::Identity,Spawn);check(Player);auto* Mesh=Player->GetMesh();Mesh->InitAnim(true);auto* Anim=CastChecked<UVTGCameraLookAnimInstance>(Mesh->GetAnimInstance());Anim->bCameraLookEnabled=false;
    auto Bool=[&](UObject* O,const TCHAR* N,bool V){auto* P=FindFProperty<FBoolProperty>(O->GetClass(),N);check(P);P->SetPropertyValue_InContainer(O,V);};
    for(const TCHAR* Name:{TEXT("UsingStunGun"),TEXT("UsingSMG1"),TEXT("IsDead?"),TEXT("HidePlayerNCam"),TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),TEXT("IsAttacking"),TEXT("IsReadingMail"),TEXT("InCombat"),TEXT("StoryMode?")})Bool(Player,Name,false);Bool(Anim,TEXT("IsCutscene"),false);
    auto* Weapon=FindFProperty<FByteProperty>(Player->GetClass(),TEXT("CurrentMeleeWeapon"));check(Weapon && Weapon->Enum);const uint8 Hammer=Weapon->Enum->GetValueByNameString(TEXT("NewEnumerator1"));const uint8 None=Weapon->Enum->GetValueByNameString(TEXT("NewEnumerator0"));check(Weapon->Enum->GetDisplayNameTextByValue(Hammer).ToString()==TEXT("SmolHammer"));
    auto Frame=[&](){Anim->NativeUpdateAnimation(1.f/60);Mesh->TickAnimation(0,false);Mesh->RefreshBoneTransforms();};
    auto Pose=[&](bool Enabled){Anim->bHammerSpineCorrectionEnabled=Enabled;for(int I=0;I<5;++I)Frame();const auto Bone=Mesh->GetSocketTransform(TEXT("spine_02_x"),RTS_Component);const auto Parent=Mesh->GetSocketTransform(Mesh->GetParentBone(TEXT("spine_02_x")),RTS_Component);return Bone.GetRelativeTransform(Parent);};
    int Fail=0;auto Expect=[&](bool Pass,const TCHAR* Label){UE_LOG(LogTemp,Display,TEXT("HAMMER POSTURE %s: %s"),Pass?TEXT("PASS"):TEXT("FAIL"),Label);Fail+=!Pass;};
    Weapon->SetPropertyValue_InContainer(Player,None);const auto Neutral=Pose(false);const auto NeutralEnabled=Pose(true);Expect(Neutral.Equals(NeutralEnabled,.001),TEXT("out-of-combat empty-handed spine remains unchanged"));Expect(Anim->HammerSpineAlpha==0,TEXT("out-of-combat correction is disabled"));
    auto CheckX=[&](const TCHAR* Label){const auto Off=Pose(false);const auto On=Pose(true);const auto D=(Off.GetRotation().Inverse()*On.GetRotation()).Rotator();Expect(Anim->HammerSpineAlpha==1 && FMath::IsNearlyEqual(D.Roll,-15.,.2) && FMath::Abs(D.Pitch)<.2 && FMath::Abs(D.Yaw)<.2,Label);};
    Bool(Player,TEXT("InCombat"),true);CheckX(TEXT("fists evaluated spine has one -15 degree local X correction"));
    Weapon->SetPropertyValue_InContainer(Player,Hammer);const auto Base=Pose(false);const auto Corrected=Pose(true);const auto Delta=(Base.GetRotation().Inverse()*Corrected.GetRotation()).Rotator();UE_LOG(LogTemp,Display,TEXT("HAMMER MEASURED bone-local delta=%s"),*Delta.ToString());
    Expect(FMath::IsNearlyEqual(Delta.Roll,-15.,.2) && FMath::Abs(Delta.Pitch)<.2 && FMath::Abs(Delta.Yaw)<.2,TEXT("actual evaluated spine rotates -15 degrees on local X only"));Expect(Base.GetLocation().Equals(Corrected.GetLocation(),.01) && Base.GetScale3D().Equals(Corrected.GetScale3D(),.001),TEXT("bone translation and scale remain unchanged"));
    for(const TCHAR* Name:{TEXT("UsingStunGun"),TEXT("UsingSMG1")}){Bool(Player,Name,true);CheckX(*FString::Printf(TEXT("%s uses the same combat correction"),Name));Bool(Player,Name,false);}
    for(const TCHAR* Name:{TEXT("IsDead?"),TEXT("HidePlayerNCam"),TEXT("StoryMode?"),TEXT("IsInCineCutscene_LockInput(NotWorkForMont)")}){Bool(Player,Name,true);const auto Off=Pose(false);const auto On=Pose(true);Expect(Anim->HammerSpineAlpha==0 && Off.Equals(On,.001),*FString::Printf(TEXT("%s excludes combat correction"),Name));Bool(Player,Name,false);}
    Bool(Anim,TEXT("IsCutscene"),true);Anim->NativeUpdateAnimation(0);Expect(Anim->HammerSpineAlpha==0,TEXT("animation graph cinematic override disables correction"));Bool(Anim,TEXT("IsCutscene"),false);
    Bool(Player,TEXT("IsAttacking"),true);Anim->NativeUpdateAnimation(0);Expect(Anim->HammerSpineAlpha==1,TEXT("hammer attacks keep their posture correction"));Bool(Player,TEXT("IsAttacking"),false);
    Weapon->SetPropertyValue_InContainer(Player,None);Frame();Expect(Anim->HammerSpineAlpha==1,TEXT("switching from hammer to fists in combat keeps the correction"));
    Bool(Player,TEXT("InCombat"),false);Weapon->SetPropertyValue_InContainer(Player,Hammer);const auto OutsideOff=Pose(false);const auto OutsideOn=Pose(true);Expect(Anim->HammerSpineAlpha==0 && OutsideOff.Equals(OutsideOn,.001),TEXT("leaving combat disables correction even while holding hammer"));
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return Fail==0;
}
}
UVTGHammerPostureCommandlet::UVTGHammerPostureCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UVTGHammerPostureCommandlet::Main(const FString& Params)
{
    auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/Characters/Sa/ABP_Sa"));check(BP && BP->GeneratedClass->IsChildOf(UVTGCameraLookAnimInstance::StaticClass()));check(BP->TargetSkeleton->GetReferenceSkeleton().FindBoneIndex(TEXT("spine_02_x"))!=INDEX_NONE);UEdGraph* Graph=nullptr;for(UEdGraph* G:BP->FunctionGraphs)if(G->GetFName()==TEXT("AnimGraph"))Graph=G;check(Graph);
    if(!Params.Contains(TEXT("VerifyOnly")))HammerPosture::Apply(BP,Graph);int Count=0;for(UEdGraphNode* N:Graph->Nodes)if(auto* B=Cast<UAnimGraphNode_ModifyBone>(N);B && B->NodeComment==HammerPosture::Marker){++Count;check(B->Node.BoneToModify.BoneName==TEXT("spine_02_x") && B->Node.RotationSpace==BCS_BoneSpace && B->Node.RotationMode==BMM_Additive);}check(Count==1);
    return HammerPosture::Test(BP)?0:1;
}
