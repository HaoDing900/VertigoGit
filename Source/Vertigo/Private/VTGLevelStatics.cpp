#include "VTGLevelStatics.h"
#include "VTGLevelManagerBase.h"
#include "VTGGameInstanceBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

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
