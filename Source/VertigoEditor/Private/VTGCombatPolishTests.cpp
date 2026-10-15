#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Combat/VTGCombatPresentation.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/StructOnScope.h"
#include "VTGSewerCombatCommandlet.h"

bool VTGTestCombatPolish(UBlueprint *BP, UBlueprint *BossBP)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    auto Init = UWorld::InitializationValues()
                    .AllowAudioPlayback(false)
                    .RequiresHitProxies(false)
                    .CreatePhysicsScene(true)
                    .CreateNavigation(false)
                    .CreateAISystem(false)
                    .ShouldSimulatePhysics(false)
                    .SetTransactional(false);
    auto *World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CombatPolishTest"), nullptr, true,
                                      ERHIFeatureLevel::Num, &Init);
    auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(World);
    auto* GI=NewObject<UGameInstance>();World->SetGameInstance(GI);Context.OwningGameInstance=GI;
    // APawn::ShouldTakeDamage rejects damage without an authority GameMode, even in a game world.
    check(World->SetGameMode(FURL(nullptr,TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"),TRAVEL_Absolute)));
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto *Player = World->SpawnActor<ACharacter>(BP->GeneratedClass, FTransform::Identity, Spawn);
    check(Player);
    Player->GetMesh()->SetAnimInstanceClass(UAnimInstance::StaticClass());
    Player->GetMesh()->InitAnim(true);
    auto *Anim = Player->GetMesh()->GetAnimInstance();
    auto Set = [&](FName Name, bool Value)
    {
        auto *P = FindFProperty<FBoolProperty>(Player->GetClass(), Name);
        check(P);
        P->SetPropertyValue_InContainer(Player, Value);
    };
    auto Get = [&](FName Name)
    { return FindFProperty<FBoolProperty>(Player->GetClass(), Name)->GetPropertyValue_InContainer(Player); };
    auto Run = [](UObject *Object, FName Name)
    {
        auto *F = Object->FindFunction(Name);
        check(F);
        FStructOnScope Args(F);
        Object->ProcessEvent(F, Args.GetStructMemory());
    };
    int Fail = 0;
    auto Expect = [&](bool OK, const TCHAR *Description)
    {
        Fail += !OK;
        UE_LOG(LogTemp, Display, TEXT("POLISH TEST %s: %s"), OK ? TEXT("PASS") : TEXT("FAIL"), Description);
    };
    Set(TEXT("InCombat"), true);
    Set(TEXT("StoryMode?"), false);
    Set(TEXT("CanAttack?"), true);
    Set(TEXT("CanPunch?"), true);
    Set(TEXT("HasWeaponInHand"), false);
    Run(Player, TEXT("Punch"));
    auto *First = Anim->GetCurrentActiveMontage();
    Expect(First != nullptr, TEXT("real first punch starts"));
    Run(Player, TEXT("Punch"));
    Expect(Get(TEXT("SaveAttack")), TEXT("early second press enters the existing buffer"));
    auto *Support = Player->FindComponentByClass<UVTGPlayerPresentationComponent>();
    check(Support);
    Support->ConsumeBufferedAttack();
    Expect(Anim->GetCurrentActiveMontage() == First, TEXT("buffer cannot skip the authored attack window"));
    // Advance the real animation and dispatch its authored ComboWindow notify.
    bool Advanced = false;
    for (int I = 0; I < 180 && !Advanced; ++I)
    {
        Player->GetMesh()->TickAnimation(1.f / 120.f, false);
        Player->GetMesh()->ConditionallyDispatchQueuedAnimEvents();
        Support->ConsumeBufferedAttack();
        Advanced = Anim->GetCurrentActiveMontage() != First && Anim->GetCurrentActiveMontage() != nullptr;
    }
    Expect(Advanced && !Get(TEXT("SaveAttack")),
           TEXT("authored montage window consumes an early press and starts the next punch"));

    auto *Boss = World->SpawnActor<ACharacter>(BossBP->GeneratedClass, FTransform(FVector(3000, 0, 0)), Spawn);
    check(Boss);
    auto *AI = World->SpawnActor<AAIController>(AAIController::StaticClass(), FTransform::Identity, Spawn);
    AI->Possess(Boss);
    auto *Target = World->SpawnActor<AVTGAttackRangeTestTarget>(AVTGAttackRangeTestTarget::StaticClass(),
                                                                FTransform(FVector(4000, 0, 0)), Spawn);
    check(Target);
    auto *AbilityBP = LoadObject<UBlueprint>(
        nullptr, TEXT("/Game/AI/BossAIToolkit/Blueprints/Abilities/Damage/BP_Ability_Projectile"));
    check(AbilityBP);
    auto *Ability = World->SpawnActor<AActor>(AbilityBP->GeneratedClass, FTransform::Identity, Spawn);
    Ability->SetOwner(Boss);
    auto *Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Characters/Monster/SeaMonsterBoss/DT_SeaMonster_Phases"));
    check(Table);
    FArrayProperty *Abilities = nullptr;
    for (TFieldIterator<FArrayProperty> I(Table->GetRowStruct()); I; ++I)
        if (I->GetName().StartsWith(TEXT("Abilities_")))
            Abilities = *I;
    check(Abilities);
    FScriptArrayHelper Rows(Abilities,
                            Abilities->ContainerPtrToValuePtr<void>(Table->FindRowUnchecked(TEXT("SeaMonster_P1"))));
    auto *Info = FindFProperty<FStructProperty>(Ability->GetClass(), TEXT("AbilityInfo"));
    check(Info && Rows.Num() == 4);
    Info->CopyCompleteValue(Info->ContainerPtrToValuePtr<void>(Ability), Rows.GetRawPtr(3));
    UActorComponent *Behavior = nullptr;
    for (auto *C : Boss->GetComponents())
        if (FindFProperty<FNumericProperty>(C->GetClass(), TEXT("AttackPower")))
            Behavior = C;
    check(Behavior);
    FindFProperty<FObjectPropertyBase>(Behavior->GetClass(), TEXT("TargetActor"))
        ->SetObjectPropertyValue_InContainer(Behavior, Target);
    Run(Ability, TEXT("ReceiveBeginPlay"));
    auto *Start = Ability->FindFunction(TEXT("OnBeginDamage"));
    check(Start);
    FStructOnScope StartArgs(Start);
    auto *Duration = FindFProperty<FNumericProperty>(Start, TEXT("Duration"));
    Duration->SetFloatingPointPropertyValue(Duration->ContainerPtrToValuePtr<void>(StartArgs.GetStructMemory()), .65);
    Ability->ProcessEvent(Start, StartArgs.GetStructMemory());
    auto *ProjectileBP =
        LoadObject<UBlueprint>(nullptr, TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_AcidProjectile"));
    check(ProjectileBP);
    AActor *Projectile = nullptr;
    int Count = 0;
    for (TActorIterator<AActor> I(World, ProjectileBP->GeneratedClass.Get()); I; ++I)
    {
        Projectile = *I;
        ++Count;
    }
    Expect(Count == 1, TEXT("boss ability damage notify emits exactly one acid projectile"));
    if (Projectile)
    {
        Run(Projectile, TEXT("ReceiveBeginPlay"));
        auto *Move = Projectile->FindComponentByClass<UProjectileMovementComponent>();
        Expect(Move && Move->Velocity.Size() > 800, TEXT("acid projectile has real forward velocity"));
        auto *Collision = Projectile->FindComponentByClass<USphereComponent>();
        check(Collision);
        FHitResult Hit(Target, Target->GetCapsuleComponent(), Target->GetActorLocation(), FVector(-1, 0, 0));
        Collision->OnComponentBeginOverlap.Broadcast(Collision, Target, Target->GetCapsuleComponent(), 0, true, Hit);
        Expect(Target->TotalDamage > 0, TEXT("acid collision applies actual toolkit damage"));
        Expect(Projectile->IsActorBeingDestroyed(), TEXT("acid projectile ends after the impact"));

        // The generic target proves the toolkit callback; also verify the game's actual player receiver.
        auto* Health=FindFProperty<FNumericProperty>(Player->GetClass(),TEXT("Health"));check(Health);
        auto ReadHealth=[&](){auto* V=Health->ContainerPtrToValuePtr<void>(Player);return Health->IsInteger()?double(Health->GetSignedIntPropertyValue(V)):Health->GetFloatingPointPropertyValue(V);};
        const double Before=ReadHealth();
        FActorSpawnParameters AcidSpawn;AcidSpawn.Owner=Ability;AcidSpawn.Instigator=Boss;AcidSpawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Acid=World->SpawnActor<AActor>(ProjectileBP->GeneratedClass,FTransform(FVector(200,0,150)),AcidSpawn);check(Acid);
        Run(Acid,TEXT("ReceiveBeginPlay"));auto* Sphere=Acid->FindComponentByClass<USphereComponent>();check(Sphere);
        auto* AcidType=FindFProperty<FObjectPropertyBase>(Acid->GetClass(),TEXT("DamageType"));
        auto* AcidDamage=FindFProperty<FNumericProperty>(Acid->GetClass(),TEXT("Damage"));
        UE_LOG(LogTemp,Display,TEXT("ACID PLAYER CHECK: health=%f damage=%f damageType=%s story=%d dodge=%d canDamage=%d"),Before,AcidDamage?AcidDamage->GetFloatingPointPropertyValue(AcidDamage->ContainerPtrToValuePtr<void>(Acid)):0,*GetPathNameSafe(AcidType?AcidType->GetObjectPropertyValue_InContainer(Acid):nullptr),Get(TEXT("StoryMode?")),Get(TEXT("IsDodging?")),Player->CanBeDamaged());
        FHitResult PlayerHit(Player,Player->GetCapsuleComponent(),Player->GetActorLocation(),FVector(-1,0,0));
        Sphere->OnComponentBeginOverlap.Broadcast(Sphere,Player,Player->GetCapsuleComponent(),0,true,PlayerHit);
        Expect(ReadHealth()<Before,TEXT("acid contact reduces real BP_Player_Sa Health"));
        const double After=ReadHealth();
        Sphere->OnComponentBeginOverlap.Broadcast(Sphere,Player,Player->GetCapsuleComponent(),0,true,PlayerHit);
        Expect(ReadHealth()==After,TEXT("a second overlap cannot apply acid damage twice to the same player"));
    }
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return Fail == 0;
}
