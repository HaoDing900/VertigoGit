#include "Save/VTGCheckpoint.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Save/VTGSaveCoordinator.h"
#include "VTGLevelStatics.h"

AVTGCheckpoint::AVTGCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(150.f, 300.f, 150.f));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetCanEverAffectNavigation(false);
	Trigger->ShapeColor = FColor(80, 220, 120);
	RootComponent = Trigger;

	RespawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("RespawnPoint"));
	RespawnPoint->SetupAttachment(Trigger);
	RespawnPoint->ArrowColor = FColor(80, 220, 120);
	RespawnPoint->ArrowSize = 1.5f;
	RespawnPoint->bIsScreenSizeScaled = true;
}

void AVTGCheckpoint::BeginPlay()
{
	Super::BeginPlay();

	// Restored from a later (or this) checkpoint: everything up to it is already passed. Saving
	// again would just add a disk write the moment the player respawns inside the box.
	if (const UVTGSaveCoordinator* Save = GetGameInstance()->GetSubsystem<UVTGSaveCoordinator>())
	{
		bActivated = !Save->GetResumeCheckpoint().IsNone() && Order <= Save->GetResumeCheckpointOrder();
	}

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AVTGCheckpoint::HandleOverlap);

	// The player can already be inside (a checkpoint at the PlayerStart): that overlap began before
	// we were listening. Look next tick, once the pawn is possessed and the level has settled.
	GetWorldTimerManager().SetTimerForNextTick([WeakThis = TWeakObjectPtr<AVTGCheckpoint>(this)]()
	{
		AVTGCheckpoint* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		TArray<AActor*> Inside;
		Self->Trigger->GetOverlappingActors(Inside, APawn::StaticClass());
		for (AActor* Actor : Inside)
		{
			Self->HandleOverlap(Self->Trigger, Actor, nullptr, INDEX_NONE, false, FHitResult());
		}
	});
}

bool AVTGCheckpoint::CanActivate_Implementation(APawn* Player) const
{
	return IsValid(Player);
}

void AVTGCheckpoint::HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	APawn* Player = Cast<APawn>(Other);
	if (bActivated || !Player || !Player->IsPlayerControlled() || !UVTGLevelStatics::IsActiveStage(this, RequiredStage) || !CanActivate(Player))
	{
		return;
	}

	UVTGSaveCoordinator* Save = GetGameInstance()->GetSubsystem<UVTGSaveCoordinator>();
	bActivated = true;
	// Position + yaw only: a scaled box or a tilted arrow must not scale/tilt the player.
	const FTransform Respawn(FRotator(0.f, RespawnPoint->GetComponentRotation().Yaw, 0.f), RespawnPoint->GetComponentLocation());
	if (Save && Save->SaveCheckpoint(GetCheckpointId(), Respawn, Order))
	{
		OnCheckpointReached(Player);
	}
}
