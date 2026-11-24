#include "VTGSaveKeyWatcher.h"
#include "VTGLevelManagerBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CoreGlobals.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

void UVTGSaveKeyWatcher::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 if (GEngine)
 {
  DeletedHandle = GEngine->OnLevelActorDeleted().AddUObject(this, &UVTGSaveKeyWatcher::HandleActorDeleted);
 }
}

void UVTGSaveKeyWatcher::Deinitialize()
{
 if (GEngine && DeletedHandle.IsValid())
 {
  GEngine->OnLevelActorDeleted().Remove(DeletedHandle);
 }
 DeletedHandle.Reset();
 Super::Deinitialize();
}

bool UVTGSaveKeyWatcher::IsReinstancing(const AActor* Actor)
{
 // Compiling a Blueprint destroys every placed instance of it and spawns replacements. That arrives
 // on OnLevelActorDeleted exactly like a real deletion, so without this the warning fired on every
 // single compile of a BPLM - which is the one Blueprint an author edits all day. Worse than noise:
 // a warning that cries wolf on every compile is a warning nobody reads when it is real.
 //
 // Three tests because any one of them alone has a hole: the globals are only set while the compiler
 // is on the stack (some teardown happens after), and the flags are only on objects that actually got
 // a replacement. The name check is the backstop, and it is what the reported case looked like:
 // "REINST_BPLM_Clinic_C_3".
 if (GCompilingBlueprint || GIsReinstancing) return true;
 if (!Actor) return false;
 if (Actor->HasAnyFlags(RF_NewerVersionExists)) return true;
 if (const UClass* Class = Actor->GetClass(); Class && Class->HasAnyClassFlags(CLASS_NewerVersionExists)) return true;
 const FString Name = Actor->GetName();
 return Name.StartsWith(TEXT("REINST_")) || Name.StartsWith(TEXT("TRASHCLASS_")) || Name.StartsWith(TEXT("LIVECODING_"));
}

FString UVTGSaveKeyWatcher::DescribeSaveKeyUse(AActor* Actor)
{
 if (!Actor || !Actor->GetWorld()) return FString();

 // This warns the author about deleting a placed actor. A play session destroys actors constantly and
 // tears every one of them down on exit, none of which touches what is saved on disk, so only the
 // editor world can produce a real deletion here.
 if (Actor->GetWorld()->WorldType != EWorldType::Editor) return FString();

 // Nor did the author delete anything when the Blueprint compiler swapped the instance out.
 if (IsReinstancing(Actor)) return FString();

 // A level manager's progress is keyed by LevelManagerSaveId, or by its actor name when that is blank.
 if (auto* Manager = Cast<AVTGLevelManagerBase>(Actor))
 {
  return Manager->LevelManagerSaveId.IsNone()
   ? FString::Printf(TEXT("its progress snapshot is keyed by its name \"%s\" (LevelManagerSaveId is empty)"), *Actor->GetName())
   : FString();
 }

 // Everything a level manager holds a reference to is a candidate for being destroyed by it, and the
 // destroyed-actor record is keyed by actor name. Finding the reference is the cheap way to tell.
 for (TActorIterator<AVTGLevelManagerBase> It(Actor->GetWorld()); It; ++It)
 {
  AVTGLevelManagerBase* Manager = *It;
  for (TFieldIterator<FObjectPropertyBase> Property(Manager->GetClass()); Property; ++Property)
  {
   if (Property->GetOwnerClass() && !Property->GetOwnerClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint)) continue;
   if (Property->GetObjectPropertyValue_InContainer(Manager) == Actor)
   {
    return FString::Printf(TEXT("%s refers to it as \"%s\", and anything a level manager destroys is remembered by actor name"),
     *Manager->GetName(), *Property->GetName());
   }
  }
 }
 return FString();
}

void UVTGSaveKeyWatcher::HandleActorDeleted(AActor* Actor)
{
 const FString Reason = DescribeSaveKeyUse(Actor);
 if (Reason.IsEmpty()) return;

 const FText Message = FText::FromString(FString::Printf(
  TEXT("Deleted \"%s\": %s.\nExisting saves refer to the old name, so a replacement placed here gets a new one and those saves will not match it. Undo, or accept that old saves lose this."),
  *Actor->GetName(), *Reason));
 UE_LOG(LogTemp, Warning, TEXT("VTG save key deleted - %s"), *Message.ToString().Replace(TEXT("\n"), TEXT(" ")));

 FNotificationInfo Info(Message);
 Info.ExpireDuration = 12.f;
 Info.bFireAndForget = true;
 Info.bUseLargeFont = false;
 FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
}
