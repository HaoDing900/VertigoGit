#pragma once
#include "EditorSubsystem.h"
#include "VTGSaveKeyWatcher.generated.h"

/**
 * Warns, at the moment it happens, when you delete a level actor whose NAME is a save key.
 *
 * Why deletion and not renaming: AActor::SetActorLabel only writes ActorLabel - it never renames the
 * object - so renaming in the Outliner does NOT change GetFName() and does NOT break a save. What does
 * break one is deleting an actor and placing a new one, because the replacement gets a fresh
 * auto-generated name, and the save still refers to the old one. That is silent, and it is what this
 * catches: delete a gated BPLM or something a BPLM destroys, and you hear about it straight away.
 */
UCLASS()
class UVTGSaveKeyWatcher : public UEditorSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;

 /** Empty when the actor's name is not a save key; otherwise why it is one. */
 static FString DescribeSaveKeyUse(AActor* Actor);

 /**
  * True when this "deletion" is the Blueprint compiler swapping an instance out, not an author
  * removing an actor. Compiling a BPLM destroys and respawns every placed instance of it, and that
  * reaches OnLevelActorDeleted indistinguishably from the real thing.
  */
 static bool IsReinstancing(const AActor* Actor);
private:
 void HandleActorDeleted(AActor* Actor);
 FDelegateHandle DeletedHandle;
};
