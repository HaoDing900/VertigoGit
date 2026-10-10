#include "VTGLevelManagerBase.h"
#include "VTGGameInstanceBase.h"
#include "Save/VTGSaveCoordinator.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

AVTGLevelManagerBase::AVTGLevelManagerBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

FName AVTGLevelManagerBase::GetActiveStage() const
{
	//Test override wins so designers can force a stage while editing.
	if (bOverrideStageForTesting)
	{
		return TestStage;
	}

	//Otherwise read the value the previous map stashed on the (persistent) game instance.
	if (const UWorld* World = GetWorld())
	{
		if (const UVTGGameInstanceBase* GI = World->GetGameInstance<UVTGGameInstanceBase>())
		{
			return GI->CurrentStage;
		}
	}

	return NAME_None;
}

FName AVTGLevelManagerBase::GetResumeCheckpoint() const
{
	const UGameInstance* GI = GetGameInstance();
	const UVTGSaveCoordinator* Save = GI ? GI->GetSubsystem<UVTGSaveCoordinator>() : nullptr;
	return Save ? Save->GetResumeCheckpoint() : NAME_None;
}

void AVTGLevelManagerBase::BeginPlay()
{
 // Restore before Blueprint ReceiveBeginPlay and OnStageBegin can start narrative.
 if (UGameInstance* GI = GetGameInstance())
  if (auto* Save = GI->GetSubsystem<UVTGSaveCoordinator>()) Save->RestoreLevelManager(this);
 Super::BeginPlay();

	//Hand the resolved stage to the Blueprint child so it can run its Chain-A / Chain-B logic.
	OnStageBegin(GetActiveStage());
}

namespace
{
 bool IsProgressProperty(const FProperty* P)
 {
  if (P->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient)) return false;
  const UClass* Owner = P->GetOwnerClass();
  if (!Owner || !Owner->HasAnyClassFlags(CLASS_CompiledFromBlueprint)) return false;
  // Common BPLM camera/input/sequence bookkeeping must start clean after loading.
  if (Owner->GetName() == TEXT("BPLM_C") && P->GetFName() != TEXT("GeneralLevelPhase")) return false;
  return P->IsA<FBoolProperty>() || P->IsA<FNumericProperty>() || P->IsA<FEnumProperty>()
   || P->IsA<FNameProperty>() || P->IsA<FStrProperty>();
 }
}

bool AVTGLevelManagerBase::EnterSavedStoryEvent(FName EventId)
{
 if (EventId.IsNone() || RestoredStoryEvents.Contains(EventId)) return false;
 StartedStoryEvents.Add(EventId);
 return true;
}

void AVTGLevelManagerBase::CaptureLevelProgress(FVTGLevelProgressRecord& Record) const
{
 Record.ClassPath = GetClass()->GetPathName();
 Record.StartedEvents = StartedStoryEvents;
 for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
 {
  const FProperty* P = *It;
  if (!IsProgressProperty(P)) continue;
  FString Value;
  P->ExportText_InContainer(0, Value, this, this, const_cast<AVTGLevelManagerBase*>(this), PPF_None);
  Record.Values.Add(P->GetFName(), Value);
  Record.Types.Add(P->GetFName(), P->GetCPPType());
 }
}

void AVTGLevelManagerBase::RestoreLevelProgress(const FVTGLevelProgressRecord& Record)
{
 if (Record.ClassPath != GetClass()->GetPathName()) return;
 for (const auto& Pair : Record.Values)
 {
  FProperty* P = FindFProperty<FProperty>(GetClass(), Pair.Key);
  const FString* Type = Record.Types.Find(Pair.Key);
  if (P && IsProgressProperty(P) && Type && *Type == P->GetCPPType())
   P->ImportText_Direct(*Pair.Value, P->ContainerPtrToValuePtr<void>(this), this, PPF_None);
 }
 bProgressRestored = true;
 StartedStoryEvents = Record.StartedEvents;
 RestoredStoryEvents = Record.StartedEvents;
}
