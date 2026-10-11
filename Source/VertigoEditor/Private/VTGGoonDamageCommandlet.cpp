#include "VTGGoonDamageCommandlet.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

AVTGGoonDamageTarget::AVTGGoonDamageTarget()
{
    auto* Sphere=CreateDefaultSubobject<USphereComponent>(TEXT("DamageSphere")); RootComponent=Sphere;
    Sphere->InitSphereRadius(12.f); Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Sphere->SetCollisionObjectType(ECC_Pawn); Sphere->SetCollisionResponseToAllChannels(ECR_Overlap);
    Tags.Add(TEXT("Player"));
}
float AVTGGoonDamageTarget::TakeDamage(float Amount,const FDamageEvent& Event,AController* EventInstigator,AActor* Causer)
{
    Health-=Amount; return Amount;
}
UVTGGoonDamageCommandlet::UVTGGoonDamageCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}

namespace
{
void Backup(UObject* Asset)
{
    const FString File=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    const FString Copy=FPaths::ProjectDir()/TEXT("Scripts/GoonDamageDiagnosis/Backup")/(Asset->GetName()+TEXT(".before_damage_fix.uasset"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Copy),true);
    if(IFileManager::Get().FileExists(*Copy)){check(FMD5Hash::HashFile(*Copy)==FMD5Hash::HashFile(*File));}
    else {check(IFileManager::Get().Copy(*Copy,*File,false)==COPY_OK);}
}
void Save(UObject* Asset)
{
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    const FString File=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    check(UPackage::SavePackage(Asset->GetOutermost(),Asset,*File,Args));
}
UObject* Component(AActor* Actor,const TCHAR* Name)
{
    auto* P=FindFProperty<FObjectProperty>(Actor->GetClass(),Name);check(P);return P->GetObjectPropertyValue_InContainer(Actor);
}
void Enable(UObject* Collision,int32 Part)
{
    auto* F=Collision->FindFunction(TEXT("EnableCollision"));check(F);FStructOnScope Params(F);
    auto* P=FindFProperty<FByteProperty>(F,TEXT("CollisionPart"));check(P);
    const int64 Value=P->Enum->GetValueByNameString(FString::Printf(TEXT("NewEnumerator%d"),Part));check(Value!=INDEX_NONE);
    P->SetPropertyValue_InContainer(Params.GetStructMemory(),(uint8)Value);Collision->ProcessEvent(F,Params.GetStructMemory());
}
void TickTrace(UObject* Collision)
{
    auto* F=Collision->FindFunction(TEXT("ReceiveTick"));check(F);FStructOnScope Params(F);
    auto* P=FindFProperty<FFloatProperty>(F,TEXT("DeltaSeconds"));check(P);P->SetPropertyValue_InContainer(Params.GetStructMemory(),1.f/60.f);
    Collision->ProcessEvent(F,Params.GetStructMemory());
}
}

int32 UVTGGoonDamageCommandlet::Main(const FString& Params)
{
    const bool Verify=Params.Contains(TEXT("VerifyOnly"));
    FEditorScriptExecutionGuard Guard;
    auto* World=GEditor->GetEditorWorldContext().World();check(World);
    for(const TCHAR* Name:{TEXT("BP_Enm_StreetGoon1Misto"),TEXT("BP_Enm_StreetGoon2Nara")})
    {
        const FString Path=FString(TEXT("/Game/Characters/NPC/StreetGoons/"))+Name;
        auto* BP=LoadObject<UBlueprint>(nullptr,*(Path+TEXT(".")+Name));check(BP);
        auto* CDO=CastChecked<ACharacter>(BP->GeneratedClass->GetDefaultObject());
        auto* Mesh=CDO->GetMesh()->GetSkeletalMeshAsset();check(Mesh);
        auto* Skeleton=Mesh->GetSkeleton();
        const FString SkeletonFile=FPackageName::LongPackageNameToFilename(Skeleton->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        const FMD5Hash SkeletonHash=FMD5Hash::HashFile(*SkeletonFile);
        const int32 BoneCount=Mesh->GetRefSkeleton().GetNum();
        UEdGraph* Graph=BP->UbergraphPages[0];
        UEdGraphNode* Foot=nullptr;UK2Node_VariableGet* MeshGet=nullptr;
        for(UEdGraphNode* N:Graph->Nodes)
        {
            if(N->GetName()==TEXT("K2Node_MakeStruct_0")) Foot=N;
            if(auto* V=Cast<UK2Node_VariableGet>(N);V && V->GetName()==TEXT("K2Node_VariableGet_0")) MeshGet=V;
        }
        check(Foot && MeshGet && MeshGet->VariableReference.GetMemberName()==TEXT("Mesh"));
        auto* FootComponent=Foot->FindPin(TEXT("Component_2_3941386A40A6BCBCE45808A34CB04791"));check(FootComponent);
        if(!Verify)
        {
            Backup(Mesh);Backup(BP);Mesh->Modify();
            for(const TCHAR* Bone:{TEXT("hand_r"),TEXT("hand_l"),TEXT("foot_r"),TEXT("foot_l")})
            {
                const FName SocketName(FString(Bone)+TEXT("Socket"));
                check(Mesh->GetRefSkeleton().FindBoneIndex(Bone)!=INDEX_NONE && !Mesh->FindSocket(SocketName));
                auto* Socket=NewObject<USkeletalMeshSocket>(Mesh,NAME_None,RF_Transactional);
                Socket->SocketName=SocketName;Socket->BoneName=Bone;
                Mesh->GetMeshOnlySocketList().Add(Socket);
            }
            Mesh->PostEditChange();
            check(FootComponent->LinkedTo.IsEmpty());
            check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(MeshGet->FindPin(TEXT("Mesh")),FootComponent));
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
            FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Results);
            check(Results.NumErrors==0);
        }
        check(FootComponent->LinkedTo.Num()==1);
        check(Mesh->GetRefSkeleton().GetNum()==BoneCount);
        for(const TCHAR* Bone:{TEXT("hand_r"),TEXT("hand_l"),TEXT("foot_r"),TEXT("foot_l")})
        {
            auto* Socket=Mesh->FindSocket(FName(FString(Bone)+TEXT("Socket")));check(Socket && Socket->BoneName==Bone && Socket->GetOuter()==Mesh);
        }
        auto* Actor=World->SpawnActor<ACharacter>(BP->GeneratedClass);check(Actor);
        auto* Collision=Component(Actor,TEXT("MeleeCollisionComponent"));check(Collision);
        // Exercise the animation-facing event and trace tick for both fists and the kick.
        for(int32 Part:{2,3,4})
        {
            Enable(Collision,Part);
            auto* Array=FindFProperty<FArrayProperty>(Collision->GetClass(),TEXT("CollisionComponents"));check(Array);
            FScriptArrayHelper Comps(Array,Array->ContainerPtrToValuePtr<void>(Collision));check(Comps.Num()==1);
            auto* Struct=CastFieldChecked<FStructProperty>(Array->Inner);
            auto* Comp=FindFProperty<FObjectProperty>(Struct->Struct,TEXT("Component_2_3941386A40A6BCBCE45808A34CB04791"));check(Comp);
            check(Comp->GetObjectPropertyValue_InContainer(Comps.GetRawPtr(0))==Actor->GetMesh());
            auto* Sockets=FindFProperty<FArrayProperty>(Struct->Struct,TEXT("Sockets_6_12180EF74F4BA7D53FD5AC84FDE6E1BF"));check(Sockets);
            FScriptArrayHelper Names(Sockets,Sockets->ContainerPtrToValuePtr<void>(Comps.GetRawPtr(0)));check(Names.Num()==1);
            const FName Bone=CastFieldChecked<FNameProperty>(Sockets->Inner)->GetPropertyValue(Names.GetRawPtr(0));
            check(Bone!=NAME_None && Actor->GetMesh()->GetBoneIndex(Bone)!=INDEX_NONE);
            auto* Target=World->SpawnActor<AVTGGoonDamageTarget>();check(Target);
            Target->SetActorLocation(Actor->GetMesh()->GetSocketLocation(Bone));
            // First tick initializes tracing; the next tick performs the sweep.
            TickTrace(Collision);
            TickTrace(Collision);
            UE_LOG(LogTemp,Display,TEXT("GOON DAMAGE %s part=%d bone=%s health=%.1f"),Name,Part,*Bone.ToString(),Target->Health);
            check(FMath::IsNearlyEqual(Target->Health,93.f));
            World->DestroyActor(Target);
        }
        World->DestroyActor(Actor);
        if(!Verify){Save(Mesh);Save(BP);}
        check(SkeletonHash==FMD5Hash::HashFile(*SkeletonFile));
        UE_LOG(LogTemp,Display,TEXT("GOON DAMAGE %s %s PASS skeleton unchanged"),Name,Verify?TEXT("VERIFY"):TEXT("SAVE"));
    }
    return 0;
}
