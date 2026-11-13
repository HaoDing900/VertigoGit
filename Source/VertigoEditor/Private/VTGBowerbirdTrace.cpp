#include "VTGBowerbirdTrace.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace VTGBowerbirdTrace
{
namespace
{
TAutoConsoleVariable<int32> Enabled(TEXT("vtg.BowerbirdTrace"),1,TEXT("Capture Bowerbird state in L2 street PIE/Simulation. Editor only; 0 disables."));
FDelegateHandle Handle;
struct FState {double LastSample=-100;bool Started=false;};
TMap<TWeakObjectPtr<UWorld>,FState> States;

void Append(const FString& Line)
{
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("Diagnostics");IFileManager::Get().MakeDirectory(*Dir,true);
 FFileHelper::SaveStringToFile(Line+TEXT("\n"),*(Dir/TEXT("L2BowerbirdTrace.log")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
}

void Sample(UWorld* W,ULevelSequencePlayer* Player,double SequenceTime)
{
 FString Line=FString::Printf(TEXT("world=%s worldTime=%.3f sequence=%s seqTime=%.5f"),*W->GetName(),W->GetTimeSeconds(),Player?*Player->GetName():TEXT("none"),SequenceTime);
 int32 Found=0;
 for(TActorIterator<ACharacter> It(W);It;++It)if(It->GetClass()->GetName()==TEXT("BP_Bowerbird_Still_C"))
 {
  ++Found;auto* A=*It;auto* M=A->GetMesh();auto* Move=A->GetCharacterMovement();
  auto* Flag=FindFProperty<FBoolProperty>(A->GetClass(),TEXT("IsCutscene?(NeedsManualDetect)"));
  Line+=FString::Printf(TEXT("\n actor=%s destroying=%d hidden=%d cut=%d loc=(%s) velocity=(%s) movement=%d gravityScale=%.3f"),*A->GetName(),A->IsActorBeingDestroyed(),A->IsHidden(),Flag?Flag->GetPropertyValue_InContainer(A):-1,*A->GetActorLocation().ToString(),*A->GetVelocity().ToString(),Move?int32(Move->MovementMode):-1,Move?Move->GravityScale:0.f);
  if(M)
  {
   int32 Invalid=0;float MaxBoneDistance=0;
   for(const auto& Bone:M->GetComponentSpaceTransforms()){Invalid+=Bone.ContainsNaN()?1:0;MaxBoneDistance=FMath::Max(MaxBoneDistance,float(Bone.GetLocation().Size()));}
   Line+=FString::Printf(TEXT("\n mesh=%s visible=%d hidden=%d registered=%d renderState=%d recentRender=%d mainPass=%d ownerNoSee=%d onlyOwnerSee=%d mode=%d anim=%s poseBones=%d invalidBones=%d maxBoneDistance=%.3f meshLoc=(%s) boundsOrigin=(%s) boundsExtent=(%s) boundsScale=%.3f"),*GetNameSafe(M->GetSkeletalMeshAsset()),M->IsVisible(),M->bHiddenInGame,M->IsRegistered(),M->IsRenderStateCreated(),M->WasRecentlyRendered(.15f),M->bRenderInMainPass,M->bOwnerNoSee,M->bOnlyOwnerSee,int32(M->GetAnimationMode()),*GetNameSafe(M->GetAnimInstance()),M->GetComponentSpaceTransforms().Num(),Invalid,MaxBoneDistance,*M->GetComponentLocation().ToString(),*M->Bounds.Origin.ToString(),*M->Bounds.BoxExtent.ToString(),M->BoundsScale);
   for(FConstPlayerControllerIterator PCIt=W->GetPlayerControllerIterator();PCIt;++PCIt)if(auto* PC=PCIt->Get())Line+=FString::Printf(TEXT(" controller=%s hiddenActor=%d hiddenPrimitive=%d"),*PC->GetName(),PC->HiddenActors.Contains(A),PC->HiddenPrimitiveComponents.Contains(M));
  }
 }
 Line+=FString::Printf(TEXT("\n bowerbirdCount=%d"),Found);
 if(Player)if(auto* Camera=Player->GetActiveCameraComponent())Line+=FString::Printf(TEXT(" camera=%s cameraLoc=(%s) cameraRot=(%s) FOV=%.3f"),*Camera->GetPathName(),*Camera->GetComponentLocation().ToString(),*Camera->GetComponentRotation().ToString(),Camera->FieldOfView);
 Append(Line);
}

void Tick(UWorld* W,ELevelTick,float)
{
 if(!Enabled.GetValueOnGameThread() || !W || W->WorldType!=EWorldType::PIE || !W->GetMapName().Contains(TEXT("L2_StreetOutsideBar")))return;
 auto& State=States.FindOrAdd(W);
 if(!State.Started){Append(TEXT("=== L2 Bowerbird PIE/Simulation trace: ")+FDateTime::UtcNow().ToIso8601()+TEXT(" ==="));State.Started=true;}
 ULevelSequencePlayer* Target=nullptr;double Time=-1;
 for(TObjectIterator<ULevelSequencePlayer> It;It;++It)if(It->GetWorld()==W && (It->IsPlaying() || It->IsPaused()))if(auto* Seq=It->GetSequence())if(Seq->GetPathName()==TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/LS_L2BarAfterFight.LS_L2BarAfterFight")){Target=*It;Time=It->GetCurrentTime().AsSeconds();break;}
 const double Now=W->GetTimeSeconds();const bool NearCut=Time>=17.3 && Time<=18.9;
 if(Now>=State.LastSample && Now-State.LastSample<(NearCut?.016:2.0))return;
 State.LastSample=Now;Sample(W,Target,Time);
}
}
void Startup(){if(!IsRunningCommandlet() && !Handle.IsValid())Handle=FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}
void Shutdown(){FWorldDelegates::OnWorldPostActorTick.Remove(Handle);Handle.Reset();States.Reset();}
}
