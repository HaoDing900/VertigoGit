#include "VTGBroskySocketCommandlet.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

UVTGBroskySocketCommandlet::UVTGBroskySocketCommandlet()
{
    IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true;
}

int32 UVTGBroskySocketCommandlet::Main(const FString& Params)
{
    const bool VerifyOnly=Params.Contains(TEXT("VerifyOnly"));
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/NPC/StreetGoons/Meshes/StreetGoon3Brosky_NPC_DefaultRig.StreetGoon3Brosky_NPC_DefaultRig"));
    auto* Source=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/AI/AIBehaviorSystem/Demo/Characters/Mannequins/Meshes/UE5RTG_SKM_Manny.UE5RTG_SKM_Manny"));
    check(Mesh && Source && Mesh->GetSkeleton()==Source->GetSkeleton());
    const auto& Ref=Mesh->GetRefSkeleton();
    const auto& SourceRef=Source->GetRefSkeleton();
    check(Ref.FindBoneIndex(TEXT("weapon_r"))==INDEX_NONE && Ref.FindBoneIndex(TEXT("hand_r"))!=INDEX_NONE);
    const int32 WeaponIndex=SourceRef.FindBoneIndex(TEXT("weapon_r"));
    check(WeaponIndex!=INDEX_NONE && SourceRef.GetBoneName(SourceRef.GetParentIndex(WeaponIndex))==TEXT("hand_r"));
    auto* Original=Source->FindSocket(TEXT("baseballbat_hand"));
    check(Original && Original->BoneName==TEXT("weapon_r"));
    // Preserve the original bat grip, including the weapon bone's reference-pose offset.
    const FTransform Grip=Original->GetSocketLocalTransform()*SourceRef.GetRefBonePose()[WeaponIndex];
    const FString Filename=FPackageName::LongPackageNameToFilename(Mesh->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    const FString SkeletonFilename=FPackageName::LongPackageNameToFilename(Mesh->GetSkeleton()->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    const FMD5Hash SkeletonHash=FMD5Hash::HashFile(*SkeletonFilename);
    USkeletalMeshSocket* Socket=nullptr;
    for(auto Candidate:Mesh->GetMeshOnlySocketList()) if(Candidate->SocketName==TEXT("baseballbat_hand")) Socket=Candidate;
    if(!VerifyOnly)
    {
        checkf(!Socket,TEXT("Mesh socket already exists; use -VerifyOnly"));
        const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/WeaponDiagnosis/Backup/StreetGoon3Brosky_NPC_DefaultRig.before_socket.uasset");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
        if(IFileManager::Get().FileExists(*Backup)) { check(FMD5Hash::HashFile(*Backup)==FMD5Hash::HashFile(*Filename)); }
        else { check(IFileManager::Get().Copy(*Backup,*Filename,false)==COPY_OK); }
        Mesh->Modify();
        Socket=NewObject<USkeletalMeshSocket>(Mesh,TEXT("Brosky_BaseballBat_Hand"),RF_Transactional);
        Socket->SocketName=TEXT("baseballbat_hand"); Socket->BoneName=TEXT("hand_r");
        Socket->RelativeLocation=Grip.GetTranslation(); Socket->RelativeRotation=Grip.Rotator(); Socket->RelativeScale=Grip.GetScale3D();
        Mesh->GetMeshOnlySocketList().Add(Socket);
        Mesh->PostEditChange();
    }
    check(Socket && Mesh->FindSocket(TEXT("baseballbat_hand"))==Socket && Socket->BoneName==TEXT("hand_r"));
    check(Socket->GetSocketLocalTransform().Equals(Grip,0.001f));
    check(Mesh->GetSkeleton()->FindSocket(TEXT("baseballbat_hand"))->BoneName==TEXT("weapon_r"));
    auto* Class=LoadClass<ACharacter>(nullptr,TEXT("/Game/Characters/NPC/StreetGoons/BP_Enm_SteetGoon3_Brosky.BP_Enm_SteetGoon3_Brosky_C"));
    check(Class && GEditor);
    UWorld* World=GEditor->GetEditorWorldContext().World(); check(World);
    auto* Actor=World->SpawnActor<ACharacter>(Class); check(Actor);
    auto* Prop=FindFProperty<FObjectProperty>(Class,TEXT("Weapon")); check(Prop);
    auto* Weapon=Cast<UStaticMeshComponent>(Prop->GetObjectPropertyValue_InContainer(Actor)); check(Weapon);
    auto* Function=Class->FindFunctionByName(TEXT("Server_SwitchWeapon")); check(Function);
    FEditorScriptExecutionGuard ScriptGuard;
    // Exercise the actual Blueprint RPC/event chain, rather than attaching directly.
    for(bool Equip:{false,true,false})
    {
        uint8* Buffer=(uint8*)FMemory_Alloca(Function->ParmsSize); FMemory::Memzero(Buffer,Function->ParmsSize);
        auto* EquipProp=FindFProperty<FBoolProperty>(Function,TEXT("Equip")); check(EquipProp);
        EquipProp->SetPropertyValue_InContainer(Buffer,Equip);
        Actor->ProcessEvent(Function,Buffer);
        check(Weapon->GetAttachParent()==Actor->GetMesh());
        check(Weapon->GetAttachSocketName()==(Equip?FName(TEXT("baseballbat_hand")):FName(TEXT("baseballbat_back"))));
        check(Weapon->GetComponentTransform().Equals(Actor->GetMesh()->GetSocketTransform(Weapon->GetAttachSocketName()),0.01f));
        UE_LOG(LogTemp,Display,TEXT("BROSKY SOCKET Blueprint Equip=%d socket=%s bone=%s PASS"),Equip,*Weapon->GetAttachSocketName().ToString(),*Actor->GetMesh()->GetSocketBoneName(Weapon->GetAttachSocketName()).ToString());
    }
    World->DestroyActor(Actor);
    if(!VerifyOnly)
    {
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
        check(UPackage::SavePackage(Mesh->GetOutermost(),Mesh,*Filename,Args));
    }
    check(SkeletonHash==FMD5Hash::HashFile(*SkeletonFilename));
    UE_LOG(LogTemp,Display,TEXT("BROSKY SOCKET %s PASS grip=%s skeleton file unchanged"),VerifyOnly?TEXT("VERIFY"):TEXT("SAVE"),*Grip.ToHumanReadableString());
    return 0;
}
