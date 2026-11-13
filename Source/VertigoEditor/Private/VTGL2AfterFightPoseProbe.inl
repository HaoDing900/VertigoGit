#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Tracks/MovieSceneBoolTrack.h"
#include "Tracks/MovieSceneVisibilityTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Sections/MovieSceneEventTriggerSection.h"
#include "MovieSceneEventUtils.h"
#include "K2Node_CustomEvent.h"
#include "UI/VTGSequencePoseDirector.h"
#include "Tracks/MovieSceneMaterialParameterCollectionTrack.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"

static const FName L2PoseTrackName(TEXT("VTG_SaReturnPoseRefresh"));
static TWeakObjectPtr<USkeletalMesh> StagedBirdMesh;
static bool IsL2PoseTrack(const UMovieSceneTrack* Track){return Track->IsA<UMovieSceneEventTrack>() && Track->GetDisplayName().ToString()==TEXT("Sa: refresh pose after shot return");}
static int32 RepairL2Return(bool Verify)
{
 auto* Child=LoadObject<ULevelSequence>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/FLS_L2BarStreet_AfterFight_Animation"));check(Child);
 auto* Scene=Child->GetMovieScene();FGuid Sa;
 for(int32 I=0;I<Scene->GetPossessableCount();++I)if(Scene->GetPossessable(I).GetName()==TEXT("BP_Player_Sa"))Sa=Scene->GetPossessable(I).GetGuid();check(Sa.IsValid());
 UMovieSceneEventTrack* Track=nullptr;
 for(auto* T:Scene->FindBinding(Sa)->GetTracks())if(IsL2PoseTrack(T))Track=CastChecked<UMovieSceneEventTrack>(T);
 auto* BP=CastChecked<UBlueprint>(FindFProperty<FObjectPropertyBase>(ULevelSequence::StaticClass(),TEXT("DirectorBlueprint"))->GetObjectPropertyValue_InContainer(Child));
 if(!Verify && !Track)
 {
  const FString File=FPackageName::LongPackageNameToFilename(Child->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
  const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/L2AfterFightPose/Backup")/(Child->GetName()+TEXT(".uasset"));
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
  Track=NewObject<UMovieSceneEventTrack>(Scene,L2PoseTrackName,RF_Transactional);Scene->AddGivenTrack(Track,Sa);
  Track->SetDisplayName(FText::FromString(TEXT("Sa: refresh pose after shot return")));Track->EventPosition=EFireEventsAtPosition::AtEndOfEvaluation;
  auto* Section=NewObject<UMovieSceneEventTriggerSection>(Track,NAME_None,RF_Transactional);Section->SetRange(TRange<FFrameNumber>::All());Track->AddSection(*Section);
  FMovieSceneEvent Event;auto* Endpoint=FMovieSceneEventUtils::BindNewUserFacingEvent(&Event,Section,BP);check(Endpoint);
  auto* Call=NewObject<UK2Node_CallFunction>(Endpoint->GetGraph());Endpoint->GetGraph()->AddNode(Call,false,false);Call->CreateNewGuid();Call->SetFromFunction(UVTGSequencePoseDirector::StaticClass()->FindFunctionByName(TEXT("RefreshSequenceCharacterPose")));Call->AllocateDefaultPins();Call->NodePosX=Endpoint->NodePosX+350;
  const auto* Schema=GetDefault<UEdGraphSchema_K2>();check(Schema->TryCreateConnection(Endpoint->FindPinChecked(TEXT("then")),Call->FindPinChecked(TEXT("execute"))));check(Schema->TryCreateConnection(Endpoint->FindPinChecked(Event.BoundObjectPinName),Call->FindPinChecked(TEXT("Character"))));
  Section->EventChannel.GetData().AddKey(Scene->GetTickResolution().AsFrameNumber(523.0/30.0),Event);
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);check(!Log.NumErrors && BP->Status!=BS_Error);
  Child->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(Child->GetOutermost(),Child,*File,Args));
 }
 check(Track && Track->EventPosition==EFireEventsAtPosition::AtEndOfEvaluation && Track->GetAllSections().Num()==1);
 auto Data=CastChecked<UMovieSceneEventTriggerSection>(Track->GetAllSections()[0])->EventChannel.GetData();check(Data.GetValues().Num()==1 && Data.GetValues()[0].Ptrs.Function && Data.GetValues()[0].Ptrs.BoundObjectProperty.Get());
 UE_LOG(LogTemp,Display,TEXT("L2 RETURN POSE %s: bound Sa endpoint at child frame 523, end of evaluation"),Verify?TEXT("VERIFIED"):TEXT("SAVED"));return 0;
}

// Uses transient copies: never changes the authored assets.
static int32 ProbeL2AfterFightPose(bool MeshFirst,bool RequireFixed,bool BirdGameplay=false,bool BirdCinematic=false,bool FixBirdBounds=false,bool VerifyBirdBounds=false)
{
 FEditorScriptExecutionGuard Guard;
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("L2AfterFightProbe"),nullptr,true,ERHIFeatureLevel::Num,&Init);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(BP);
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* A=W->SpawnActor<ACharacter>(BP->GeneratedClass,FTransform::Identity,Spawn);check(A);
 auto* M=A->GetMesh();M->InitAnim(true);check(M->GetAnimInstance());
 if(auto* Cut=FindFProperty<FBoolProperty>(M->GetAnimInstance()->GetClass(),TEXT("IsCutscene")))Cut->SetPropertyValue_InContainer(M->GetAnimInstance(),true);
 auto* PastBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Sa/BP_Cine_Sa_Past"));check(PastBP);
 auto* Past=W->SpawnActor<ACharacter>(PastBP->GeneratedClass,FTransform::Identity,Spawn);check(Past);Past->GetMesh()->InitAnim(true);
 auto* BirdBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/BowerBird/BP_Bowerbird_Still"));check(BirdBP);
 auto* Bird=W->SpawnActor<ACharacter>(BirdBP->GeneratedClass,FTransform::Identity,Spawn);check(Bird);Bird->GetMesh()->InitAnim(true);
 if(StagedBirdMesh.IsValid()){Bird->GetMesh()->SetSkeletalMeshAsset(StagedBirdMesh.Get());Bird->GetMesh()->InitAnim(true);}
 if(BirdGameplay){auto* Flag=FindFProperty<FBoolProperty>(Bird->GetClass(),TEXT("IsCutscene?(NeedsManualDetect)"));check(Flag);Flag->SetPropertyValue_InContainer(Bird,BirdCinematic);Bird->ProcessEvent(Bird->FindFunctionChecked(TEXT("Detect If in Cutscene")),nullptr);}
 auto Clone=[](ULevelSequence* Source){check(Source);auto* Copy=NewObject<ULevelSequence>();Copy->Initialize();auto* Scene=DuplicateObject<UMovieScene>(Source->GetMovieScene(),Copy,TEXT("ProbeScene"));FindFProperty<FObjectPropertyBase>(ULevelSequence::StaticClass(),TEXT("MovieScene"))->SetObjectPropertyValue_InContainer(Copy,Scene);auto* Director=FindFProperty<FClassProperty>(ULevelSequence::StaticClass(),TEXT("DirectorClass"));Director->SetObjectPropertyValue_InContainer(Copy,Director->GetObjectPropertyValue_InContainer(Source));return Copy;};
 auto* Root=Clone(LoadObject<ULevelSequence>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L2BarStreet/LS_L2BarAfterFight")));check(Root);
 TMap<ULevelSequence*,ULevelSequence*> Copies;
 auto MasterTracks=Root->GetMovieScene()->GetTracks();
 for(auto* T:MasterTracks)
 {
  if(!T->IsA<UMovieSceneSubTrack>()){Root->GetMovieScene()->RemoveTrack(*T);continue;}
  for(auto* S:T->GetAllSections())if(auto* Sub=Cast<UMovieSceneSubSection>(S))
  {
   auto* Original=CastChecked<ULevelSequence>(Sub->GetSequence());auto*& Copy=Copies.FindOrAdd(Original);
   if(!Copy)
   {
    Copy=Clone(Original);auto* Scene=Copy->GetMovieScene();
    auto Tracks=Scene->GetTracks();for(auto* Track:Tracks)if(!Track->IsA<UMovieSceneMaterialParameterCollectionTrack>())Scene->RemoveTrack(*Track);
    if(auto* Camera=Scene->GetCameraCutTrack())Scene->RemoveCameraCutTrack();
    TSet<FGuid> Keep;
    for(int32 I=0;I<Scene->GetPossessableCount();++I)if(Scene->GetPossessable(I).GetName()==TEXT("BP_Player_Sa") || Scene->GetPossessable(I).GetName()==TEXT("BP_Cine_Sa_Past_LSBowerbird") || Scene->GetPossessable(I).GetName()==TEXT("BP_Bowerbird_Still"))Keep.Add(Scene->GetPossessable(I).GetGuid());
    for(int32 Pass=0;Pass<2;++Pass)for(int32 I=0;I<Scene->GetPossessableCount();++I)if(Keep.Contains(Scene->GetPossessable(I).GetParent()))Keep.Add(Scene->GetPossessable(I).GetGuid());
    auto Bindings=Scene->GetBindings();for(const auto& B:Bindings)
    {
     if(!Keep.Contains(B.GetObjectGuid())){for(auto* Track:B.GetTracks())Scene->RemoveTrack(*Track);continue;}
     auto* P=Scene->FindPossessable(B.GetObjectGuid());auto* Top=P;
     while(auto* Parent=Scene->FindPossessable(Top->GetParent()))Top=Parent;
     auto* BoundActor=Top->GetName()==TEXT("BP_Bowerbird_Still")?Bird:Top->GetName().StartsWith(TEXT("BP_Cine_Sa_Past"))?Past:A;auto* BoundMesh=BoundActor->GetMesh();UObject* Object=BoundActor;
     if(P->GetName()==TEXT("CharacterMesh0"))Object=BoundMesh;
     else if(P->GetName().StartsWith(TEXT("ABP_")))Object=BoundMesh->GetAnimInstance();
     UObject* Context=Object==BoundMesh?static_cast<UObject*>(BoundActor):Object==BoundMesh->GetAnimInstance()?static_cast<UObject*>(BoundMesh):static_cast<UObject*>(W);
     Copy->UnbindPossessableObjects(B.GetObjectGuid());Copy->BindPossessableObject(B.GetObjectGuid(),*Object,Context);
     for(auto* Track:B.GetTracks())if(!Track->IsA<UMovieSceneSkeletalAnimationTrack>() && !Track->IsA<UMovieSceneBoolTrack>() && !Track->IsA<UMovieScene3DTransformTrack>() && !Track->IsA<UMovieSceneVisibilityTrack>() && !IsL2PoseTrack(Track))Scene->RemoveTrack(*Track);
    }
   }
   Sub->SetSequence(Copy);
  }
 }
 FMovieSceneSequencePlaybackSettings Settings;Settings.bRestoreState=true;ALevelSequenceActor* Actor=nullptr;
 auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(W,Root,Settings,Actor);check(Player);Player->Play();
 FBox RequiredBirdBounds(ForceInit);int64 BirdVertices=0,OutsideVertices=0;
 for(int32 Frame=0;Frame<1150;++Frame)
 {
  ++GFrameCounter;
  if(!MeshFirst)Player->Update(1.f/60.f);
  M->TickAnimation(1.f/60.f,false);M->RefreshBoneTransforms();Past->GetMesh()->TickAnimation(1.f/60.f,false);Past->GetMesh()->RefreshBoneTransforms();
  Bird->GetMesh()->TickAnimation(1.f/60.f,false);Bird->GetMesh()->RefreshBoneTransforms();Bird->GetMesh()->UpdateBounds();
  if(MeshFirst)Player->Update(1.f/60.f);
  const double Time=Player->GetCurrentTime().AsSeconds();
  if((FixBirdBounds || VerifyBirdBounds) && Time>=17.633333 && Time<18.633333 && Frame%4==0)
  {
   auto* BM=Bird->GetMesh();auto* Data=BM->GetSkeletalMeshRenderData();check(Data && Data->LODRenderData.Num());auto* Weights=BM->GetSkinWeightBuffer(0);check(Weights);
   TArray<FMatrix44f> Matrices;BM->GetCurrentRefToLocalMatrices(Matrices,0);TArray<FVector3f> Vertices;USkinnedMeshComponent::ComputeSkinnedPositions(BM,Vertices,Matrices,Data->LODRenderData[0],*Weights);check(Vertices.Num());
   const FBox Existing=BM->GetSkeletalMeshAsset()->GetBounds().GetBox();
   for(const auto& V:Vertices){const FVector P(V);RequiredBirdBounds+=P;++BirdVertices;if(!Existing.IsInsideOrOn(P))++OutsideVertices;}
  }
  if(Time>17.5 && Time<17.9){auto* BM=Bird->GetMesh();UE_LOG(LogTemp,Display,TEXT("L2BIRD time=%.5f hidden=%d mesh-visible=%d mesh-hidden=%d location=%s bounds-origin=%s bounds-extent=%s"),Time,Bird->IsHidden(),BM->IsVisible(),BM->bHiddenInGame,*Bird->GetActorLocation().ToString(),*BM->Bounds.Origin.ToString(),*BM->Bounds.BoxExtent.ToString());}
  if(Time>17.5 && Time<18.4)
  {
   auto* BM=Bird->GetMesh();auto* BA=BM->GetAnimInstance();auto* Cut=FindFProperty<FBoolProperty>(BA->GetClass(),TEXT("Is Cutscene"));
   float MinScale=FLT_MAX,MaxScale=0;FBox BoneBox(ForceInit);int32 Inside=0;
   const FVector Cam(-916.088916,5293.968699,591.009033);const FRotationMatrix Axes(FRotator(-67.800001,69.199990,0));
   const float H=FMath::Tan(FMath::DegreesToRadians(35.409f*.5f)),V=H/(16.f/9.f);
   for(const auto& B:BM->GetComponentSpaceTransforms())
   {
    MinScale=FMath::Min(MinScale,float(B.GetScale3D().GetMin()));MaxScale=FMath::Max(MaxScale,float(B.GetScale3D().GetMax()));const FVector P=BM->GetComponentTransform().TransformPosition(B.GetTranslation());BoneBox+=P;
    const FVector D=P-Cam;const double Depth=FVector::DotProduct(D,Axes.GetScaledAxis(EAxis::X));if(Depth>0 && FMath::Abs(FVector::DotProduct(D,Axes.GetScaledAxis(EAxis::Y)))<Depth*H && FMath::Abs(FVector::DotProduct(D,Axes.GetScaledAxis(EAxis::Z)))<Depth*V)++Inside;
   }
   auto* MPC=LoadObject<UMaterialParameterCollection>(nullptr,TEXT("/Game/Characters/VTGCharacterShader/Player_LightColor"));
   UE_LOG(LogTemp,Display,TEXT("BIRDPOSE time=%.5f gameplay=%d anim-cut=%d scale=%.5f..%.5f in-camera=%d/%d bone-min=%s bone-max=%s fade-disabled=%.3f"),Time,BirdGameplay,Cut?Cut->GetPropertyValue_InContainer(BA):-1,MinScale,MaxScale,Inside,BM->GetComponentSpaceTransforms().Num(),*BoneBox.Min.ToString(),*BoneBox.Max.ToString(),UKismetMaterialLibrary::GetScalarParameterValue(W,MPC,TEXT("Enable Camera Fade?(1no0yes)")));
  }
  if(Time>18.45 && Time<18.9)
  {
   float D=0;const auto& Ref=M->GetSkeletalMeshAsset()->GetRefSkeleton();const auto& Pose=M->GetBoneSpaceTransforms();
   for(int32 I=0;I<Ref.GetNum();++I)if(Ref.GetBoneName(I).ToString().Contains(TEXT("arm")))D=FMath::Max(D,Pose[I].GetRotation().AngularDistance(Ref.GetRefBonePose()[I].GetRotation()));
   auto* Anim=M->GetAnimInstance();auto* Cut=FindFProperty<FBoolProperty>(Anim->GetClass(),TEXT("IsCutscene"));
   UE_LOG(LogTemp,Display,TEXT("L2POSE time=%.5f mode=%d anim=%s cut=%d delta=%.6f actor=%s mesh=%s"),Time,int32(M->GetAnimationMode()),*Anim->GetClass()->GetName(),Cut?Cut->GetPropertyValue_InContainer(Anim):-1,D,*A->GetActorLocation().ToString(),*M->GetRelativeLocation().ToString());
   if(RequireFixed && Time>=559.0/30.0 && Time<18.8){check(D>.6f);check(A->GetActorLocation().Equals(FVector(-399.275,2109.115,240.663),.01f));}
   auto* PM=Past->GetMesh();float PD=0;const auto& PR=PM->GetSkeletalMeshAsset()->GetRefSkeleton();const auto& PP=PM->GetBoneSpaceTransforms();for(int32 I=0;I<PR.GetNum();++I)if(PR.GetBoneName(I).ToString().Contains(TEXT("arm")))PD=FMath::Max(PD,PP[I].GetRotation().AngularDistance(PR.GetRefBonePose()[I].GetRotation()));
   UE_LOG(LogTemp,Display,TEXT("L2PAST time=%.5f delta=%.6f actor=%s hidden=%d"),Time,PD,*Past->GetActorLocation().ToString(),Past->IsHidden());
  }
 }
 if(RequireFixed)UE_LOG(LogTemp,Display,TEXT("L2 RETURN POSE TEST PASS: return frame and following frames retain cinematic pose and position; mesh-first=%d"),MeshFirst);
 if(FixBirdBounds || VerifyBirdBounds)
 {
  auto* Mesh=Bird->GetMesh()->GetSkeletalMeshAsset();check(RequiredBirdBounds.IsValid && BirdVertices>0);
  UE_LOG(LogTemp,Display,TEXT("BIRD VERTEX BOUNDS: vertices=%lld outside=%lld imported=%s animated=%s"),BirdVertices,OutsideVertices,*Mesh->GetImportedBounds().GetBox().ToString(),*RequiredBirdBounds.ToString());
  if(FixBirdBounds)
  {
   const FString File=FPackageName::LongPackageNameToFilename(Mesh->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/L2AfterFightPose/Bowerbird/Backup/Bowerbird_RIG_ue_renamed_.uasset");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
   const FBox Imported=Mesh->GetImportedBounds().GetBox();const FBox Needed=RequiredBirdBounds.ExpandBy(10.0);
   Mesh->Modify();Mesh->SetPositiveBoundsExtension(Mesh->GetPositiveBoundsExtension().ComponentMax((Needed.Max-Imported.Max).ComponentMax(FVector::ZeroVector)));Mesh->SetNegativeBoundsExtension(Mesh->GetNegativeBoundsExtension().ComponentMax((Imported.Min-Needed.Min).ComponentMax(FVector::ZeroVector)));
   const FBox Expanded=Mesh->GetBounds().GetBox();check(Expanded.IsInsideOrOn(RequiredBirdBounds.Min) && Expanded.IsInsideOrOn(RequiredBirdBounds.Max));Mesh->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
   const FString Staged=FPaths::ProjectDir()/TEXT("Scripts/L2AfterFightPose/Bowerbird/Staged/Bowerbird_RIG_ue_renamed_.uasset");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Staged),true);check(UPackage::SavePackage(Mesh->GetOutermost(),Mesh,*Staged,Args));
   UE_LOG(LogTemp,Display,TEXT("BIRD BOUNDS STAGED: positive=%s negative=%s bounds=%s file=%s"),*Mesh->GetPositiveBoundsExtension().ToString(),*Mesh->GetNegativeBoundsExtension().ToString(),*Expanded.ToString(),*Staged);
  }
  if(VerifyBirdBounds){check(OutsideVertices==0);UE_LOG(LogTemp,Display,TEXT("BIRD BOUNDS VERIFY PASS: all sampled animated vertices are enclosed"));}
 }
 Player->Stop();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return 0;
}

