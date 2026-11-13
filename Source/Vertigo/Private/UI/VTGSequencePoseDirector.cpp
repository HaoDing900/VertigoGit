#include "UI/VTGSequencePoseDirector.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

void UVTGSequencePoseDirector::RefreshSequenceCharacterPose(ACharacter* Character)
{
    if (!IsValid(Character)) return;
    auto* Mesh = Character->GetMesh();
    if (!IsValid(Mesh) || !Mesh->GetAnimInstance()) return;
    UE_LOG(LogTemp,Verbose,TEXT("Sequence pose refresh: %s"),*Character->GetName());
    // Evaluate the position already authored by Sequencer without advancing animation time.
    Mesh->TickAnimation(0.f, false);
    Mesh->RefreshBoneTransforms();
}
