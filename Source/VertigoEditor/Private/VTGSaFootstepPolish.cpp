#include "Audio/VTGPlayerAudioComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Sound/AmbientSound.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Slate/WidgetRenderer.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Text/STextBlock.h"
#include "ImageUtils.h"
#include "RenderingThread.h"

namespace VTGCutsceneAudioDetails {void RegisterCustomizations();}
namespace
{
float FootHeight(UAnimSequence* A,FName Bone,float Time)
{
 const auto& Ref=A->GetSkeleton()->GetReferenceSkeleton();int32 Index=Ref.FindBoneIndex(Bone);check(Index!=INDEX_NONE);FTransform T=FTransform::Identity;
 while(Index!=INDEX_NONE){FTransform Local;A->GetBoneTransform(Local,FSkeletonPoseBoneIndex(Index),double(Time),true);T=T*Local;Index=Ref.GetParentIndex(Index);}return T.GetLocation().Z;
}
void PatchAnimation(bool Verify)
{
 auto* A=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Sa/Anm/Loco/Sa_am_Loco_Walk_Fwd_NoRM"));check(A);
 bool Changed=false;
 for(float Time:{.35f,.95f})
 {
  const FName Bone=FootHeight(A,TEXT("foot_l"),Time)<FootHeight(A,TEXT("foot_r"),Time)?TEXT("foot_l"):TEXT("foot_r");
  int Count=0;for(const auto& E:A->Notifies)if(auto* N=Cast<UVTGFootstepNotify>(E.Notify))if(FMath::IsNearlyEqual(E.GetTime(),Time,.001f)){check(N->FootBone==Bone && !N->bLanding);++Count;}
  check(Count<=1);
  if(!Count)
  {
   check(!Verify);
   if(!Changed)
   {
    const auto File=FPackageName::LongPackageNameToFilename(A->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());const auto Backup=FPaths::ProjectDir()/TEXT("Scripts/SaFootstepPolish/Backup/Sa_am_Loco_Walk_Fwd_NoRM.uasset");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
    if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File,false)==COPY_OK);
   }
   int Track=A->AnimNotifyTracks.IndexOfByPredicate([](const FAnimNotifyTrack& T){return T.TrackName==TEXT("Footsteps");});
   if(Track==INDEX_NONE){Track=A->AnimNotifyTracks.AddDefaulted();A->AnimNotifyTracks[Track].TrackName=TEXT("Footsteps");}
   auto* N=NewObject<UVTGFootstepNotify>(A,NAME_None,RF_Transactional);N->FootBone=Bone;
   auto& E=A->Notifies.AddDefaulted_GetRef();E.Notify=N;E.NotifyName=TEXT("VTG_Footstep");E.Link(A,Time);E.TriggerTimeOffset=GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetAfter);E.TriggerWeightThreshold=.25f;E.Guid=FGuid::NewGuid();E.TrackIndex=Track;Changed=true;
  }
  UE_LOG(LogTemp,Display,TEXT("SA_FOOTSTEP_NOTIFY_PASS time=%.2f bone=%s"),Time,*Bone.ToString());
 }
 if(Changed){A->SortNotifies();A->PostEditChange();const auto File=FPackageName::LongPackageNameToFilename(A->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());FSavePackageArgs Save;Save.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(A->GetOutermost(),A,*File,Save));}
}
void VolumeRegression()
{
 TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* Class=LoadClass<ACharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C"));check(Class);FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Sa=W->SpawnActor<ACharacter>(Class,FTransform::Identity,Spawn);check(Sa);auto* Audio=Sa->FindComponentByClass<UVTGPlayerAudioComponent>();check(Audio);
 const float OriginalVolume=Audio->FootstepVolume;auto* Flag=FindFProperty<FBoolProperty>(Class,TEXT("NoRunning!SlowDownWalk"));check(Flag);
 auto* Move=Sa->GetCharacterMovement();auto* Accel=FindFProperty<FStructProperty>(Move->GetClass(),TEXT("Acceleration"));check(Accel);
 auto Walk=[&](bool Restricted){Flag->SetPropertyValue_InContainer(Sa,Restricted);for(int I=0;I<90;++I){Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(200,0,0);*Accel->ContainerPtrToValuePtr<FVector>(Move)=FVector(2048,0,0);Sa->GetMesh()->TickAnimation(1.f/30.f,false);Sa->GetMesh()->RefreshBoneTransforms();}return Audio->GetFootstepContextVolumeMultiplier();};
 const float Normal=Walk(false);check(Normal==1.f);
 Flag->SetPropertyValue_InContainer(Sa,true);check(Audio->GetFootstepContextVolumeMultiplier()==1.f); // Flag alone does not reduce another BlendSpace.
 const float Restricted=Walk(true);UE_LOG(LogTemp,Display,TEXT("SA_FOOTSTEP_VOLUME normal=%.3f restricted=%.3f"),Normal,Restricted);check(FMath::IsNearlyEqual(Restricted,.8f));
 Flag->SetPropertyValue_InContainer(Sa,false);check(Audio->GetFootstepContextVolumeMultiplier()==1.f); // Restricted asset alone is insufficient.
 check(Walk(false)==1.f && Audio->FootstepVolume==OriginalVolume);
 UE_LOG(LogTemp,Display,TEXT("SA_FOOTSTEP_VOLUME_PASS actual normal/restricted BlendSpaces, both condition guards, normal return and unchanged base volume"));
 W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
}
int CountLabels(const TSharedRef<SWidget>& Widget,TSet<const SWidget*>& Seen)
{
 if(Seen.Contains(&Widget.Get()))return 0;Seen.Add(&Widget.Get());int Count=0;
 if(Widget->GetTypeAsString()==TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString()==TEXT("跟随过场环境声"))++Count;
 auto* Children=Widget->GetChildren();for(int I=0;I<Children->Num();++I)Count+=CountLabels(Children->GetChildAt(I),Seen);return Count;
}
void DetailsRegression(const FString& Output)
{
 if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
 VTGCutsceneAudioDetails::RegisterCustomizations();
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
 auto* W=UWorld::CreateWorld(EWorldType::Editor,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);auto* Actor=W->SpawnActor<AAmbientSound>();check(Actor);
 FDetailsViewArgs Args;Args.bUpdatesFromSelection=false;Args.bLockable=false;Args.bAllowSearch=false;Args.NameAreaSettings=FDetailsViewArgs::HideNameArea;
 auto View=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(Args);
 FWidgetRenderer Renderer(true);
 for(UObject* Target:{static_cast<UObject*>(Actor),static_cast<UObject*>(Actor->GetAudioComponent())})
 {
  View->SetObject(Target,true);auto* RT=Renderer.DrawWidget(View,FVector2D(900,1400));FlushRenderingCommands();
  for(int Frame=0;Frame<3;++Frame){Renderer.DrawWidget(RT,View,FVector2D(900,1400),.016f);FlushRenderingCommands();}
  TSet<const SWidget*> Seen;const int Count=CountLabels(View,Seen);UE_LOG(LogTemp,Display,TEXT("AUDIO_DETAILS_ROW_COUNT target=%s count=%d"),*Target->GetClass()->GetName(),Count);check(Count==1);
  TArray<FColor> Pixels;check(RT->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(900,1400,Pixels,PNG);const FString File=Output+(Target==Actor?TEXT("-actor.png"):TEXT("-component.png"));check(FFileHelper::SaveArrayToFile(PNG,*File));
 }
 View->SetObject(nullptr);W->DestroyWorld(false);
}
}
int32 VTGSaFootstepPolish(const FString& Params)
{
 FString Output;if(FParse::Value(*Params,TEXT("DetailsPreview="),Output)){DetailsRegression(Output);return 0;}
 PatchAnimation(FParse::Param(*Params,TEXT("Verify")));VolumeRegression();return 0;
}
