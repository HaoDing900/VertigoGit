#pragma once
#include "CoreMinimal.h"
#include "NarrativeEvent.h"
#include "VTGEnsureQuestStateEvent.generated.h"
class UQuest;

/** Start a missing quest or advance its existing instance, without restarting its saved history. */
UCLASS(DisplayName="VTG Ensure Quest State")
class VERTIGO_API UVTGEnsureQuestStateEvent : public UNarrativeEvent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Quest") TSubclassOf<UQuest> Quest;
    UPROPERTY(EditAnywhere, Category="Quest", meta=(ToolTip="目标 State 的实际 ID。在 Quest 图中选择目标 State，在 Details 的‘对话推进任务：目标 State ID’中点击‘复制 State ID’，粘贴到这里。若选中 Branch，应复制其 Destination State ID；不要填写 QuestBranch_*、节点标题或描述。例如 QuestState_1。")) FName StateID;
    virtual void ExecuteEvent_Implementation(APawn* Pawn,APlayerController* Controller,UNarrativeComponent* Component) override;
    virtual FString GetGraphDisplayText_Implementation() override;
};
