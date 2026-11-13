#include "FileHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"

static int32 ProbeL2BowerbirdPhysics(bool Cinematic)
{
 FEditorScriptExecutionGuard Guard;
 auto* Map=UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar"));check(Map);
 ACharacter* Template=nullptr;for(TActorIterator<ACharacter> It(Map);It;++It)if(It->GetName()==TEXT("BP_Bowerbird_Still_C_1"))Template=*It;check(Template);
 FHitResult Hit;FCollisionQueryParams Query;Query.AddIgnoredActor(Template);
 const FVector Start=Template->GetActorLocation();const bool Ground=Map->LineTraceSingleByChannel(Hit,Start+FVector(0,0,200),Start-FVector(0,0,100000),ECC_Visibility,Query);
 FHitResult PawnHit;const bool PawnGround=Map->LineTraceSingleByChannel(PawnHit,Start+FVector(0,0,200),Start-FVector(0,0,100000),ECC_Pawn,Query);
 UE_LOG(LogTemp,Display,TEXT("BIRDPHYS pawn-floor-hit=%d floor=%s"),PawnGround,*PawnHit.Location.ToString());
 UE_LOG(LogTemp,Display,TEXT("BIRDPHYS MAP KillZ=%.3f bounds-check=%d floor-hit=%d floor=%s start=%s"),Map->GetWorldSettings()->KillZ,Map->GetWorldSettings()->bEnableWorldBoundsChecks,Ground,*Hit.Location.ToString(),*Start.ToString());
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("BowerbirdPhysicsProbe"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* GI=NewObject<UGameInstance>();W->SetGameInstance(GI);GEngine->GetWorldContextFromWorldChecked(W).OwningGameInstance=GI;
 W->GetWorldSettings()->KillZ=Map->GetWorldSettings()->KillZ;W->GetWorldSettings()->bEnableWorldBoundsChecks=Map->GetWorldSettings()->bEnableWorldBoundsChecks;
 W->GetWorldSettings()->WorldGravityZ=Map->GetGravityZ();W->GetWorldSettings()->bWorldGravitySet=true;
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Bird=W->SpawnActor<ACharacter>(Template->GetClass(),Template->GetActorTransform(),Spawn);check(Bird);
 auto* Cut=FindFProperty<FBoolProperty>(Bird->GetClass(),TEXT("IsCutscene?(NeedsManualDetect)"));check(Cut);
 Cut->SetPropertyValue_InContainer(Bird,Cinematic?true:Cut->GetPropertyValue_InContainer(Template));
 auto* Move=Bird->GetCharacterMovement();Move->bRunPhysicsWithNoController=true;
 W->InitializeActorsForPlay(FURL());W->BeginPlay();
 const int32 SavedMode=Template->GetCharacterMovement()->MovementMode;
 Bird->ProcessEvent(Bird->FindFunctionChecked(TEXT("Detect If in Cutscene")),nullptr);
 UE_LOG(LogTemp,Display,TEXT("BIRDPHYS BEGIN cut=%d saved-mode=%d detected-mode=%d"),Cut->GetPropertyValue_InContainer(Bird),SavedMode,int32(Move->MovementMode));
 UE_LOG(LogTemp,Display,TEXT("BIRDPHYS MOVE active=%d updated=%s registered=%d gravity=%.3f map-gravity=%.3f gravity-scale=%.3f template-gravity-scale=%.3f role=%d controller=%s"),Move->IsActive(),*GetNameSafe(Move->UpdatedComponent),Move->IsRegistered(),Move->GetGravityZ(),Map->GetGravityZ(),Move->GravityScale,Template->GetCharacterMovement()->GravityScale,int32(Bird->GetLocalRole()),*GetNameSafe(Bird->GetController()));
 for(int32 Frame=0;Frame<18000;++Frame)
 {
  ++GFrameCounter;Move->TickComponent(1.f/60.f,LEVELTICK_All,&Move->PrimaryComponentTick);Bird->CheckStillInWorld();
  if(Frame%1800==0 || !IsValid(Bird) || Bird->IsActorBeingDestroyed())UE_LOG(LogTemp,Display,TEXT("BIRDPHYS time=%.3f valid=%d destroying=%d mode=%d location=%s"),(Frame+1)/60.f,IsValid(Bird),Bird->IsActorBeingDestroyed(),int32(Move->MovementMode),*Bird->GetActorLocation().ToString());
  if(!IsValid(Bird) || Bird->IsActorBeingDestroyed())break;
 }
 W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return 0;
}
