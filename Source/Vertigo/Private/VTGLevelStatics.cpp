#include "VTGLevelStatics.h"
#include "VTGLevelManagerBase.h"
#include "VTGGameInstanceBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

FName UVTGLevelStatics::GetActiveStage(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return NAME_None;
	}

	//If a level manager is present we defer to it - that lets its in-editor test override take effect.
	if (AActor* Found = UGameplayStatics::GetActorOfClass(World, AVTGLevelManagerBase::StaticClass()))
	{
		if (const AVTGLevelManagerBase* Manager = Cast<AVTGLevelManagerBase>(Found))
		{
			return Manager->GetActiveStage();
		}
	}

	//No manager in this level - just read the persistent game instance value.
	if (const UVTGGameInstanceBase* GI = World->GetGameInstance<UVTGGameInstanceBase>())
	{
		return GI->CurrentStage;
	}

	return NAME_None;
}

bool UVTGLevelStatics::IsActiveStage(const UObject* WorldContextObject, FName StageToCheck)
{
	//Empty means "belongs to every stage" - never destroyed by this check.
	if (StageToCheck.IsNone())
	{
		return true;
	}

	return GetActiveStage(WorldContextObject) == StageToCheck;
}

void UVTGLevelStatics::OpenLevelWithStage(const UObject* WorldContextObject, FName LevelName, FName Stage, bool bAbsolute, const FString& Options)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;

	//Only stash the stage if one was actually given. Empty = leave the current stage untouched.
	if (World && !Stage.IsNone())
	{
		if (UVTGGameInstanceBase* GI = World->GetGameInstance<UVTGGameInstanceBase>())
		{
			GI->CurrentStage = Stage;
		}
	}

	//Same behaviour as the built-in "Open Level (by Name)".
	UGameplayStatics::OpenLevel(WorldContextObject, LevelName, bAbsolute, Options);
}

AActor* UVTGLevelStatics::FindValidActorOfClass(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass)
{
	return FindValidActorOfClassWithTag(WorldContextObject, ActorClass, NAME_None);
}

AActor* UVTGLevelStatics::FindValidActorOfClassWithTag(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, FName Tag)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || !*ActorClass)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World, ActorClass); It; ++It)
	{
		AActor* Actor = *It;

		//Skip anything that's invalid or mid-destruction (e.g. a stage-gated actor removing itself).
		if (!IsValid(Actor) || Actor->IsActorBeingDestroyed())
		{
			continue;
		}

		//If a tag was requested, the actor must carry it.
		if (!Tag.IsNone() && !Actor->ActorHasTag(Tag))
		{
			continue;
		}

		return Actor;
	}

	return nullptr;
}

AActor* UVTGLevelStatics::FindActorOfClassInWorldOf(const UObject* WorldSource, TSubclassOf<AActor> ActorClass)
{
	UWorld* World = (GEngine && WorldSource) ? GEngine->GetWorldFromContextObject(WorldSource, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || !*ActorClass)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World, ActorClass); It; ++It)
	{
		AActor* Actor = *It;
		if (IsValid(Actor) && !Actor->IsActorBeingDestroyed())
		{
			return Actor;
		}
	}

	return nullptr;
}
