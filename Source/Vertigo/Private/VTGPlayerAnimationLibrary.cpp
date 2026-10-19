#include "VTGPlayerAnimationLibrary.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

bool UVTGPlayerAnimationLibrary::SetPlayerBaseIdle(ACharacter* Player, UAnimSequence* NewIdle, bool Instant)
{
    if (!IsValid(Player) || !IsValid(NewIdle)) return false;
    auto* Function = Player->FindFunction(TEXT("SetBaseIdle"));
    auto* Idle = Function ? FindFProperty<FObjectProperty>(Function, TEXT("NewIdle")) : nullptr;
    if (!Idle || Function->NumParms != 1) return false;
    FStructOnScope Params(Function);
    Idle->SetObjectPropertyValue_InContainer(Params.GetStructMemory(), NewIdle);
    Player->ProcessEvent(Function, Params.GetStructMemory());
    if (Instant)
    {
        auto* Mesh = Player->GetMesh();
        auto* Animation = Mesh ? Mesh->GetAnimInstance() : nullptr;
        if (Animation)
        {
            Animation->Montage_Stop(0.f);
            Mesh->TickAnimation(0.f, false);
            Mesh->RefreshBoneTransforms();
        }
    }
    return true;
}
