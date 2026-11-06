#include "VTGNoteInputTestFixture.h"
#include "Audio/VTGPlayerAudioComponent.h"
#include "NarrativeComponent.h"
#include "NarrativeEvent.h"
#include "DialogueBlueprint.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "DialogueSM.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSync.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "UObject/UnrealType.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "EngineUtils.h"

static int32 PoliceMapFootProbe()
{
 auto* W=UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/ThirdPerson/Maps/PoliceStation/L3_CentralPoliceStation_FrontDoor_DownStream"));check(W);
 UE_LOG(LogTemp,Display,TEXT("POLICE_MAP_PHYSICS scene=%d"),W->GetPhysicsScene()!=nullptr);
 if(!W->GetPhysicsScene())W->CreatePhysicsScene();
 W->UpdateWorldComponents(false,false);
 int Registered=0,Physics=0;
 for(TActorIterator<AActor> It(W);It;++It){TInlineComponentArray<UPrimitiveComponent*> Cs(*It);for(auto* C:Cs)if(C->IsRegistered()){++Registered;C->RecreatePhysicsState();Physics+=C->IsPhysicsStateCreated();}}
 UE_LOG(LogTemp,Display,TEXT("POLICE_MAP_PHYSICS registered=%d physicsState=%d"),Registered,Physics);
 auto* A=W->SpawnActor<AActor>();A->SetFlags(RF_Transient);auto* Control=NewObject<UStaticMeshComponent>(A);A->SetRootComponent(Control);A->AddInstanceComponent(Control);Control->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Control->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Control->SetCollisionResponseToAllChannels(ECR_Block);Control->RegisterComponent();Control->SetWorldLocation(FVector(1000,1600,5000));
 FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(PoliceMapFoot),true);
 bool ControlHit=W->LineTraceSingleByChannel(H,FVector(1000,1600,5100),FVector(1000,1600,4900),ECC_Visibility,Q);
 UE_LOG(LogTemp,Display,TEXT("POLICE_MAP_CONTROL hit=%d"),ControlHit);check(ControlHit);A->Destroy();
 for(const FVector Center:{FVector(983.444397,1806.473877,165),FVector(1018.154541,1596.237183,165)})
  for(const FVector Offset:{FVector::ZeroVector,FVector(-30,0,0),FVector(30,0,0),FVector(0,-30,0),FVector(0,30,0)})
  {
   const FVector Foot=Center+Offset;
   const bool Hit=W->LineTraceSingleByChannel(H,Foot+FVector(0,0,35),Foot-FVector(0,0,120),ECC_Visibility,Q);
   UE_LOG(LogTemp,Display,TEXT("POLICE_MAP_FOOT at=%s hit=%d actor=%s component=%s point=%s"),*Foot.ToString(),Hit,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*H.ImpactPoint.ToString());
   FHitResult Long;const bool LongHit=W->LineTraceSingleByChannel(Long,FVector(Foot.X,Foot.Y,1000),FVector(Foot.X,Foot.Y,-1000),ECC_Visibility,Q);
   UE_LOG(LogTemp,Display,TEXT("POLICE_MAP_LONG hit=%d actor=%s point=%s"),LongHit,*GetNameSafe(Long.GetActor()),*Long.ImpactPoint.ToString());
  }
 return 0;
}

int32 VTGPoliceFootsteps(const FString& Params)
{
 if(FParse::Param(*Params,TEXT("MapFootProbe")))return PoliceMapFootProbe();
 TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("PoliceFootsteps"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* SaClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C"));check(SaClass);
 auto* Sa=W->SpawnActor<ACharacter>(SaClass,FTransform::Identity,Spawn);check(Sa);
 auto* PC=W->SpawnActor<AVTGPlayerController>();PC->Possess(Sa);W->AddController(PC);
 auto* N=Sa->FindComponentByClass<UNarrativeComponent>();auto* Audio=Sa->FindComponentByClass<UVTGPlayerAudioComponent>();check(N && Audio);
 auto* Current=FindFProperty<FObjectPropertyBase>(N->GetClass(),TEXT("CurrentDialogue"));check(Current);
 auto Begin=[&](){auto* D=NewObject<UVTGNoteInputTestDialogue>(N);D->OwningComp=N;D->bCanBeExited=false;Current->SetObjectPropertyValue_InContainer(N,D);D->Start(PC);return D;};
 auto* FloorActor=W->SpawnActor<AActor>();auto* Floor=NewObject<UStaticMeshComponent>(FloorActor);FloorActor->SetRootComponent(Floor);FloorActor->AddInstanceComponent(Floor);
 Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Floor->SetCollisionResponseToAllChannels(ECR_Block);Floor->RegisterComponent();Floor->SetWorldScale3D(FVector(10000,10000,1));Floor->SetWorldLocation(FVector(0,0,-150));
 // Verify both same-named roots and every registered descendant, including unrelated child names.
 TSet<FString> Paths={TEXT("/Game/EnvArt/Material/Hard/M_Wall_Concrete1.M_Wall_Concrete1"),TEXT("/Game/EnvArt/EnvMaterial/Hard/M_Wall_Concrete1.M_Wall_Concrete1"),TEXT("/Game/EnvArt/EnvMaterial/Hard/M_Wall_Concrete1_Inst.M_Wall_Concrete1_Inst")};
 auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
 TArray<FAssetData> Instances;Registry.GetAssetsByClass(UMaterialInstanceConstant::StaticClass()->GetClassPathName(),Instances,true);
 bool Changed=true;while(Changed){Changed=false;for(const auto& A:Instances){FString Parent;if(A.GetTagValue(TEXT("Parent"),Parent) && Paths.Contains(FPackageName::ExportTextPathToObjectPath(Parent)) && !Paths.Contains(A.GetSoftObjectPath().ToString())){Paths.Add(A.GetSoftObjectPath().ToString());Changed=true;}}}
 auto* Original=Sa->GetMesh()->GetSkeletalMeshAsset();Sa->GetMesh()->SetSkeletalMeshAsset(nullptr);
 FHitResult Hit;Hit.Component=Floor;
 for(const FString& Path:Paths){auto* M=LoadObject<UMaterialInterface>(nullptr,*Path);check(M);Floor->SetMaterial(0,M);check(&Audio->ResolveFootstepPool(Hit)==&Audio->ConcreteSteps);Floor->SetMaterial(0,UMaterialInstanceDynamic::Create(M,Floor));check(&Audio->ResolveFootstepPool(Hit)==&Audio->ConcreteSteps);UE_LOG(LogTemp,Display,TEXT("POLICE_CONCRETE_PASS %s"),*Path);}
 Sa->GetMesh()->SetSkeletalMeshAsset(Original);
 auto Snapshot=[&](const TCHAR* Label){auto* Mesh=Sa->GetMesh();Sa->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Sa->GetCharacterMovement()->Velocity=FVector(200,0,0);Mesh->TickAnimation(.033f,false);Mesh->RefreshBoneTransforms();FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(PoliceFoot),true,Sa);auto Foot=Mesh->GetSocketLocation(TEXT("foot_l"));bool Trace=W->LineTraceSingleByChannel(H,Foot+FVector(0,0,35),Foot-FVector(0,0,120),ECC_Visibility,Q);UE_LOG(LogTemp,Display,TEXT("POLICE_FOOT_STATE %s can=%d trace=%d anim=%s mode=%d pause=%d mesh=%s foot=%s pools=%d/%d volume=%f"),Label,Audio->CanPlayFootstep(),Trace,*GetNameSafe(Mesh->GetAnimInstance()),int32(Mesh->GetAnimationMode()),Mesh->bPauseAnims,*GetNameSafe(Mesh->GetSkeletalMeshAsset()),*Foot.ToString(),Audio->DefaultSteps.Num(),Audio->ConcreteSteps.Num(),Audio->FootstepVolume);};
 const bool Observe=FParse::Param(*Params,TEXT("ObserveNotifies"));
 // Read queues on a transient character. Never replace notifies or save animation/Sequence assets.
 auto ObserveWalking=[&](const FString& Label)
 {
  if(!Observe)return;
  auto* Mesh=Sa->GetMesh();auto* Move=Sa->GetCharacterMovement();
  auto Bool=[](const UObject* O,const TCHAR* Name){auto* P=O?FindFProperty<FBoolProperty>(O->GetClass(),Name):nullptr;return P && P->GetPropertyValue_InContainer(O);};
  UE_LOG(LogTemp,Display,TEXT("FOOT_NOTIFY_RAW %s anim=%s mode=%d cutscene=%d pause=%d hidden=%d hideNCam=%d attack=%d dodge=%d dead=%d movement=%d"),*Label,*GetNameSafe(Mesh->GetAnimInstance()),int32(Mesh->GetAnimationMode()),Bool(Mesh->GetAnimInstance(),TEXT("IsCutscene")),Mesh->bPauseAnims,Sa->IsHidden(),Bool(Sa,TEXT("HidePlayerNCam")),Bool(Sa,TEXT("IsAttacking")),Bool(Sa,TEXT("IsDodging?")),Bool(Sa,TEXT("IsDead?")),int32(Move->MovementMode));
  auto* Acceleration=FindFProperty<FStructProperty>(Move->GetClass(),TEXT("Acceleration"));check(Acceleration);
  int Queued=0,Allowed=0,Ground=0,Pool=0;TSet<FString> Assets;
  for(int Frame=0;Frame<180;++Frame)
  {
   // Supply deterministic walking input; do not reset dialogue/animation flags under investigation.
   Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(200,0,0);
   *Acceleration->ContainerPtrToValuePtr<FVector>(Move)=FVector(2048,0,0);
   Mesh->TickAnimation(1.f/30.f,false);Mesh->RefreshBoneTransforms();Mesh->ConditionallyDispatchQueuedAnimEvents();
   auto* Anim=Mesh->GetAnimInstance();if(!Anim)continue;
   for(const auto& Record:Anim->GetUngroupedActivePlayersRead())if(Record.SourceAsset)Assets.Add(Record.SourceAsset->GetName());
   for(const auto& Group:Anim->GetSyncGroupMapRead())for(const auto& Record:Group.Value.ActivePlayers)if(Record.SourceAsset)Assets.Add(Record.SourceAsset->GetName());
   for(const auto& Ref:Anim->NotifyQueue.AnimNotifies)if(const auto* Event=Ref.GetNotify())if(auto* Notify=Cast<UVTGFootstepNotify>(Event->Notify))
   {
    ++Queued;if(Audio->CanPlayFootstep())++Allowed;
    const FVector Foot=Mesh->GetSocketLocation(Notify->FootBone);FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(PoliceFootNotify),true,Sa);Q.bReturnPhysicalMaterial=true;Q.bReturnFaceIndex=true;
    if(W->LineTraceSingleByChannel(H,Foot+FVector(0,0,35),Foot-FVector(0,0,120),ECC_Visibility,Q)){++Ground;if(!Audio->ResolveFootstepPool(H).IsEmpty())++Pool;}
   }
  }
  FString Names;for(const auto& Name:Assets)Names+=Name+TEXT(",");
  UE_LOG(LogTemp,Display,TEXT("FOOT_NOTIFY_OBSERVATION %s duration=6 queued=%d gateAllowed=%d groundHit=%d nonEmptyPool=%d assets=%s"),*Label,Queued,Allowed,Ground,Pool,*Names);
 };
 Snapshot(TEXT("Before"));
 ObserveWalking(TEXT("Baseline"));
 auto* BP=LoadObject<UDialogueBlueprint>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L3PoliceStation/L3PoliceStation_Still"));check(BP);auto* Template=CastChecked<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass)->GetDialogueTemplate();
 for(auto* Node:Template->GetNodes())for(UNarrativeEvent* E:Node->Events)if(E && E->GetClass()->GetName()==TEXT("1PlayLevelSequence_C"))
 {
  auto* D=Begin();auto* Event=DuplicateObject<UNarrativeEvent>(E,D);auto* P=FindFProperty<FObjectPropertyBase>(Event->GetClass(),TEXT("Level Sequence"));auto* Seq=CastChecked<ULevelSequence>(P->GetObjectPropertyValue_InContainer(Event));
  for(int I=0;I<Seq->GetMovieScene()->GetPossessableCount();++I){const auto& B=Seq->GetMovieScene()->GetPossessable(I);if(!B.GetParent().IsValid() && B.GetPossessedObjectClass()==SaClass){Seq->UnbindPossessableObjects(B.GetGuid());Seq->BindPossessableObject(B.GetGuid(),*Sa,W);}}
  Event->ExecuteEvent(Sa,PC,N);auto* Player=N->GetDialogueLevelSequencePlayer();check(Player);Player->Update(float(Player->GetDuration().AsSeconds()+.1));N->ForceExitDialogue();
  ObserveWalking(Node->GetID().ToString()+TEXT(" natural finish"));Snapshot(*Node->GetID().ToString());
  if(Observe)
  {
   D=Begin();Event=DuplicateObject<UNarrativeEvent>(E,D);Event->ExecuteEvent(Sa,PC,N);Player=N->GetDialogueLevelSequencePlayer();check(Player);
   Player->Update(float(Player->GetDuration().AsSeconds()*.45));N->ForceExitDialogue();ObserveWalking(Node->GetID().ToString()+TEXT(" interrupted"));
  }
 }
 W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return 0;
}
