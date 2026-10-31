#include "VTGClinicFix.h"
#include "VTGNarrativeInsert.h"
#include "NarrativeComponent.h"
#include "Dialogue.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

void UVTGNarrativeMontageCleanup::Track(UObject* Context,UAnimInstance* Anim,UAnimMontage* Montage)
{
 auto* D=Context?Context->GetTypedOuter<UDialogue>():nullptr;
 if(!D || !D->OwningComp || !Anim || !Montage)return;
 if(Narrative.IsValid())Narrative->OnDialogueFinished.RemoveDynamic(this,&UVTGNarrativeMontageCleanup::DialogueFinished);
 // A new dialogue taking ownership must not be stopped by the previous dialogue's timer.
 GetWorld()->GetTimerManager().ClearTimer(CleanupTimer);
 SourceDialogue=D;Narrative=D->OwningComp;
 Narrative->OnDialogueFinished.AddUniqueDynamic(this,&UVTGNarrativeMontageCleanup::DialogueFinished);
 if(auto* I=Anim->GetActiveInstanceForMontage(Montage))Clips.Add({Anim,Montage,I->GetInstanceID()});
}
void UVTGNarrativeMontageCleanup::TrackRestored(UAnimInstance* Anim,UAnimMontage* Montage)
{
 if(Clips.ContainsByPredicate([&](const FClip& C){return C.Anim==Anim && C.Montage==Montage;}))
  if(auto* I=Anim->GetActiveInstanceForMontage(Montage))Clips.Add({Anim,Montage,I->GetInstanceID()});
}
void UVTGNarrativeMontageCleanup::DialogueFinished(UDialogue* D,bool)
{
 if(D!=SourceDialogue.Get())return;
 GetWorld()->GetTimerManager().SetTimer(CleanupTimer,this,&UVTGNarrativeMontageCleanup::Cleanup,0.75f,false);
}
void UVTGNarrativeMontageCleanup::Cleanup()
{
 // Detach insert restoration before stopping its montage, so an old dialogue base cannot return.
 const bool OwnsActive=Clips.ContainsByPredicate([](const FClip& C){
  if(!C.Anim.IsValid() || !C.Montage.IsValid())return false;
  auto* I=C.Anim->GetActiveInstanceForMontage(C.Montage.Get());return I && I->GetInstanceID()==C.InstanceID;
 });
 if(OwnsActive)if(auto* S=GetOwner()->FindComponentByClass<UVTGNarrativeInsertState>())S->CancelRestoration();
 for(const FClip& C:Clips)if(C.Anim.IsValid() && C.Montage.IsValid())
  if(auto* I=C.Anim->GetActiveInstanceForMontage(C.Montage.Get());I && I->GetInstanceID()==C.InstanceID)
   I->Stop(FAlphaBlend(0.25f),true);
 Clips.Reset();
}
void UVTGNarrativeMontageCleanup::EndPlay(const EEndPlayReason::Type Reason)
{
 if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(CleanupTimer);
 if(Narrative.IsValid())Narrative->OnDialogueFinished.RemoveDynamic(this,&UVTGNarrativeMontageCleanup::DialogueFinished);
 Super::EndPlay(Reason);
}
void UVTGClinicLobbyCameraEvent::ExecuteEvent_Implementation(APawn*,APlayerController*,UNarrativeComponent* C)
{
 // Intentionally inert: retained for loading backups and the explicit asset migration.
}
void UVTGClinicButtonEvent::ExecuteEvent_Implementation(APawn*,APlayerController*,UNarrativeComponent*)
{
 if(!GetWorld())return;
 auto* ManagerClass=LoadClass<AActor>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L5Clinic/BPLM_Clinic.BPLM_Clinic_C"));if(!ManagerClass)return;
 for(TActorIterator<AActor> It(GetWorld(),ManagerClass);It;++It)
 {
   // Reaching the reminder is the authoritative point at which Ulises has asked for the button.
   if(auto* Asked=FindFProperty<FBoolProperty>(It->GetClass(),TEXT("L5_AfterExplore_UlisesHadTalkedCaseNAskToPressButton")))Asked->SetPropertyValue_InContainer(*It,true);
   if(auto* Pressed=FindFProperty<FBoolProperty>(It->GetClass(),TEXT("L5_HadPressedButton"));Pressed && Pressed->GetPropertyValue_InContainer(*It))continue;
   if(auto* P=FindFProperty<FObjectPropertyBase>(It->GetClass(),TEXT("ISXColi_L5Button")))
    if(auto* Button=P->GetObjectPropertyValue_InContainer(*It))
     if(IsValid(Button))if(auto* Enable=Button->FindFunction(TEXT("EnableInteract")))Button->ProcessEvent(Enable,nullptr);
 }
}
