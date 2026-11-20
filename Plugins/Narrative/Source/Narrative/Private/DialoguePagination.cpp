#include "Dialogue.h"
#include "DialogueSM.h"
#include "NarrativeComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

bool UDialogue::CanPaginateCurrentLine(bool ForSideText) const
{
 return OwningComp && OwningComp->GetNetMode()==NM_Standalone && (!bFreeMovement || ForSideText || bSidePresentationPagination) && !bWaitingForPlayerResponse &&
  CurrentNode && CurrentNode->IsA<UDialogueNode_NPC>() && CurrentNode->bIsSkippable && !CurrentNode->IsRoutingNode() &&
  CurrentLine.Duration==ELineDuration::LD_AfterReadingTime && !CurrentLine.DialogueSound && !CurrentLine.Shot;
}
void UDialogue::SetPresentationPages(const TArray<FString>& Pages,bool ForSideText)
{
 if(!Pages.IsEmpty() && !CanPaginateCurrentLine(ForSideText))return;
 bSidePresentationPagination=ForSideText && Pages.Num()>1;
 PresentationPages=Pages;PresentationPageIndex=0;bWaitingForManualAdvance=false;
 if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(TimerHandle_NPCReplyFinished);
 ArmPresentationPageTimer();
}
FText UDialogue::GetPresentationPage() const
{
 return PresentationPages.IsValidIndex(PresentationPageIndex)?FText::AsCultureInvariant(PresentationPages[PresentationPageIndex]):CurrentLine.Text;
}
void UDialogue::ArmPresentationPageTimer()
{
 if(!GetWorld())return;
 FDialogueLine Page=CurrentLine;Page.Text=GetPresentationPage();
 const float Duration=FMath::Max(.05f,GetLineDuration(CurrentNode,Page));
 GetWorld()->GetTimerManager().SetTimer(TimerHandle_NPCReplyFinished,this,&UDialogue::FinishNPCDialogue,Duration,false);
}
bool UDialogue::AdvancePresentationPage()
{
 if(!CanPaginateCurrentLine() || !PresentationPages.IsValidIndex(PresentationPageIndex+1))return false;
 ++PresentationPageIndex;bWaitingForManualAdvance=false;bExplicitLineAdvance=false;
 if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(TimerHandle_NPCReplyFinished);
 ArmPresentationPageTimer();
 return true;
}
