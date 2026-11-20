#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "NarrativeComponent.h"
#include "Dialogue.h"
#include "Blueprint/UserWidget.h"
#include "Components/RichTextBlock.h"
#include "UObject/UObjectIterator.h"
#include "UI/VTGLocalizationSubsystem.h"
#include "UI/VTGDialogueHistorySubsystem.h"
#include "Internationalization/TextLocalizationManager.h"

static FAutoConsoleCommandWithWorldAndArgs GBarLocProbe(TEXT("VTG.BarLocalizationProbe"),TEXT("Explicit PIE-only real bar dialogue localization diagnosis: start / inspect."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* W){
 if(!W || W->WorldType!=EWorldType::PIE || Args.IsEmpty())return;
 auto* GI=W->GetGameInstance();auto* Languages=GI->GetSubsystem<UVTGLocalizationSubsystem>();
 if(Args[0]==TEXT("start"))
 {
  Languages->SetLanguage(TEXT("en-US"));
  auto* History=GI->GetSubsystem<UVTGDialogueHistorySubsystem>();if(History->IsAutoAdvanceEnabled())History->ToggleAutoAdvance();
  auto* Class=LoadClass<UDialogue>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L2BarNStreetOutside/L2_Bar_NPC.L2_Bar_NPC_C"));
  for(TObjectIterator<UNarrativeComponent> It;It;++It)if(!It->IsTemplate() && It->GetWorld()==W && It->GetOwningController()==W->GetFirstPlayerController())
  {
   It->ExitDialogue();FDialoguePlayParams P;P.StartFromID=TEXT("L2_Bar_NPC_DialogueNode_NPC_16");
   const bool Began=It->BeginDialogue(Class,P);UE_LOG(LogTemp,Display,TEXT("BAR_LIVE_START %s began=%d"),*It->GetPathName(),Began);return;
  }
  UE_LOG(LogTemp,Error,TEXT("BAR_LIVE no player narrative component"));return;
 }
 UE_LOG(LogTemp,Display,TEXT("BAR_LIVE_STATE requested=%s preview=%d"),*Languages->GetLanguage(),FTextLocalizationManager::Get().IsGameLocalizationPreviewEnabled());
 for(TObjectIterator<UNarrativeComponent> It;It;++It)if(!It->IsTemplate() && It->GetWorld()==W && It->IsInDialogue())
 {auto* D=It->GetCurrentDialogue();const FText T=D->GetCompletePresentationText();UE_LOG(LogTemp,Display,TEXT("BAR_LIVE_NODE %s ns=%s key=%s display=[%s]"),*D->GetPathName(),*FTextInspector::GetNamespace(T).Get(FString()),*FTextInspector::GetKey(T).Get(FString()),*T.ToString());}
 for(TObjectIterator<UUserWidget> It;It;++It)if(!It->IsTemplate() && It->GetWorld()==W && It->GetClass()->GetName()==TEXT("BP_Narrative3Overlay_C"))
  for(const TCHAR* Name:{TEXT("Line_Main"),TEXT("Line_Side"),TEXT("Line_Thought"),TEXT("Line_Monologue")})if(auto* R=Cast<URichTextBlock>(It->GetWidgetFromName(Name)))UE_LOG(LogTemp,Display,TEXT("BAR_LIVE_UI %s %s visible=%d text=[%s]"),*It->GetPathName(),Name,R->IsVisible(),*R->GetText().ToString());
}));
