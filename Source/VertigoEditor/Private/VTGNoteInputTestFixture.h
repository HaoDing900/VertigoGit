#pragma once
#include "Dialogue.h"
#include "UI/VTGNoteInputWidget.h"
#include "VTGPlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "VTGNoteInputTestFixture.generated.h"

UCLASS()
class UVTGNoteInputTestDialogue : public UDialogue
{
    GENERATED_BODY()
public:
    void Start(APlayerController* Controller)
    {
        OwningController = Controller;
        bFreeMovement = false;
        bAdjustPlayerTransform = false;
        DialogueBlendOutTime = 0.f;
        OnBeginDialogue();
    }
    void Finish() { OnEndDialogue(); }
    void DispatchNodeForTest(class UNarrativeComponent* Component, class UDialogueNode* Node, bool bStarted)
    {
        OwningComp = Component;
        ProcessNodeEvents(Node, bStarted);
    }
    int32 NodeStartCount = 0;
    int32 NodeFinishCount = 0;
    UFUNCTION()
    void RecordNodeDispatchForTest(class UDialogueNode* Node, bool bStarted)
    {
        if (bStarted) ++NodeStartCount;
        else ++NodeFinishCount;
    }
protected:
    virtual void InitSpeakerAvatars() override {}
    virtual void CleanUpSpeakerAvatars() override {}
};

UCLASS()
class UVTGNoteInputTestWidget : public UVTGNoteInputWidget
{
    GENERATED_BODY()
public:
    void CloseForTest() { NativeDestruct(); }
};

UCLASS()
class AVTGNoteInputTestGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AVTGNoteInputTestGameMode()
    {
        PlayerControllerClass = AVTGPlayerController::StaticClass();
        DefaultPawnClass = nullptr;
        HUDClass = nullptr;
    }
};

UCLASS()
class AVTGMailReadingTestActor : public AActor
{
    GENERATED_BODY()
public:
    int32 CleanupCount = 0;
    UFUNCTION()
    void FinishMailForTest() { ++CleanupCount; }
};
