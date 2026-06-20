#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VTGLevelManagerBase.generated.h"

/**
 * Base class for level managers (reparent BPLM to this).
 *
 * Holds an in-editor test override so you can force a stage while editing, and resolves the
 * "active stage" purely from already-available data (test override, else the GameInstance value).
 * Because GetActiveStage is a pure lookup - not something computed in BeginPlay - any other actor
 * can ask for the stage in its own BeginPlay, in any order, and always get the right answer. The
 * manager does NOT need to run before everyone else.
 *
 * Stage is a plain Name, matching your existing structure.
 */
UCLASS()
class VERTIGO_API AVTGLevelManagerBase : public AActor
{
	GENERATED_BODY()

public:

	AVTGLevelManagerBase();

	/** TEST ONLY: when true this level ignores the GameInstance and uses TestStage. Turn OFF before shipping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	bool bOverrideStageForTesting = false;

	/** The stage to force while bOverrideStageForTesting is on (e.g. "Stage_Bar_Intro"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage", meta = (EditCondition = "bOverrideStageForTesting"))
	FName TestStage = NAME_None;

	/**
	 * The resolved stage for this level. Pure and order-independent - safe to call from any actor's
	 * BeginPlay (or anywhere) without worrying about who initialised first.
	 */
	UFUNCTION(BlueprintPure, Category = "Stage")
	FName GetActiveStage() const;

	/** Implement in your BPLM child to run the Chain-A / Chain-B logic for the resolved stage. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Stage")
	void OnStageBegin(FName Stage);

protected:

	virtual void BeginPlay() override;
};
