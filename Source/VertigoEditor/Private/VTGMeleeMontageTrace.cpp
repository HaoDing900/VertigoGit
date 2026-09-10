#include "VTGMeleeMontageTrace.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "UObject/Stack.h"
#include "UObject/UnrealType.h"
UVTGMeleeMontageTrace::UVTGMeleeMontageTrace(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickInterval=.05f;}
void UVTGMeleeMontageTrace::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* TickFunction)
{
 Super::TickComponent(Delta,Type,TickFunction);
 auto Character=Cast<ACharacter>(GetOwner());auto Anim=Character?Character->GetMesh()->GetAnimInstance():nullptr;
 if(!Anim||Bound.Get()==Anim)return;
 if(Bound.IsValid()){Bound->OnMontageStarted.RemoveDynamic(this,&UVTGMeleeMontageTrace::Started);Bound->OnMontageBlendingOut.RemoveDynamic(this,&UVTGMeleeMontageTrace::BlendingOut);}
 Bound=Anim;Anim->OnMontageStarted.AddDynamic(this,&UVTGMeleeMontageTrace::Started);Anim->OnMontageBlendingOut.AddDynamic(this,&UVTGMeleeMontageTrace::BlendingOut);
 UE_LOG(LogTemp,Display,TEXT("MELEE TRACE attached to %s / %s"),*GetNameSafe(GetOwner()),*GetNameSafe(Anim));
}
void UVTGMeleeMontageTrace::Started(UAnimMontage* Montage){Report(TEXT("START"),Montage,false);}
void UVTGMeleeMontageTrace::BlendingOut(UAnimMontage* Montage,bool Interrupted){Report(TEXT("BLENDOUT"),Montage,Interrupted);}
void UVTGMeleeMontageTrace::Report(const TCHAR* Event,UAnimMontage* Montage,bool Interrupted)
{
 auto Flag=[&](const TCHAR* Name){auto P=FindFProperty<FBoolProperty>(GetOwner()->GetClass(),Name);return P&&P->GetPropertyValue_InContainer(GetOwner());};
 UE_LOG(LogTemp,Display,TEXT("MELEE TRACE t=%.4f %s montage=%s group=%s interrupted=%d attacking=%d inCombat=%d dodging=%d active=%s\n%s"),GetWorld()->GetTimeSeconds(),Event,*GetNameSafe(Montage),Montage?*Montage->GetGroupName().ToString():TEXT("None"),Interrupted,Flag(TEXT("IsAttacking")),Flag(TEXT("InCombat")),Flag(TEXT("IsDodging?")),*GetNameSafe(Bound.IsValid()?Bound->GetCurrentActiveMontage():nullptr),*FFrame::GetScriptCallstack());
}
