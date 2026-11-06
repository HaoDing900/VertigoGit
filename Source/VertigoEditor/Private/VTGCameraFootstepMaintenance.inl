#include "Camera/CameraComponent.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimSequence.h"
#include "Audio/VTGPlayerAudioComponent.h"
#include "UObject/UObjectIterator.h"

namespace VTGUIFeedback
{
void BackupMaintenance(UObject* Object)
{
    FString File;check(FPackageName::DoesPackageExist(Object->GetPackage()->GetName(),&File));
    const FString Dest=FPaths::ProjectDir()/TEXT("Scripts/CameraFootstepMaintenance/Backup")/Object->GetPackage()->GetName().RightChop(1)+FPaths::GetExtension(File,true);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest),true);
    if(!IFileManager::Get().FileExists(*Dest))check(IFileManager::Get().Copy(*Dest,*File)==COPY_OK);
}
void SaveMaintenance(UObject* Object)
{
    FString File;check(FPackageName::DoesPackageExist(Object->GetPackage()->GetName(),&File));FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    check(UPackage::SavePackage(Object->GetPackage(),Object,*File,Args));
}
void InstallLanding(bool Verify)
{
    auto* Anim=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Sa/Anm/Loco/MM_Sa_Land.MM_Sa_Land"));check(Anim);
    int32 Found=0;for(auto& Event:Anim->Notifies)if(auto* Notify=Cast<UVTGFootstepNotify>(Event.Notify))if(FMath::IsNearlyEqual(Event.GetTime(),.06f,.001f)){++Found;check(Notify->bLanding);}
    check(Found<=1);
    if(!Found)
    {
        check(!Verify);BackupMaintenance(Anim);auto* Notify=NewObject<UVTGFootstepNotify>(Anim,NAME_None,RF_Transactional);Notify->FootBone=TEXT("foot_l");Notify->bLanding=true;
        auto& Event=Anim->Notifies.AddDefaulted_GetRef();Event.Notify=Notify;Event.NotifyName=TEXT("VTG_Footstep_Landing");Event.Link(Anim,.06f);Event.TriggerTimeOffset=GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetAfter);Event.Guid=FGuid::NewGuid();Event.TriggerWeightThreshold=.25f;Anim->SortNotifies();Anim->PostEditChange();SaveMaintenance(Anim);
    }
    UE_LOG(LogTemp,Display,TEXT("LANDING FOOTSTEP PASS: 0.06s; existing surface/barefoot pools; stationary landing supported"));
}
void CameraAspectBatch(bool Verify,bool Resume=false)
{
    auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();Registry.SearchAllAssets(true);
    TArray<UClass*> Classes;TArray<FName> Queue;TSet<FName> Seen;
    for(const TCHAR* Path:{TEXT("/Game/Blueprints/BP_0CameraLockPoint"),TEXT("/Game/Blueprints/BP_1CameraLockAutoTrigger")})
    {
        auto* BP=LoadObject<UBlueprint>(nullptr,Path);check(BP);Classes.Add(BP->GeneratedClass);BP->AddToRoot();Classes.Last()->AddToRoot();Queue.Add(BP->GetOutermost()->GetFName());
        bool Changed=false;
        for(TObjectIterator<UCameraComponent> It;It;++It)if(It->GetOutermost()==BP->GetOutermost() && It->IsTemplate() && It->bConstrainAspectRatio)
        {check(!Verify);if(!Changed)BackupMaintenance(BP);Changed=true;It->bConstrainAspectRatio=false;}
        if(Changed){Compile(BP);SaveMaintenance(BP);Classes.Last()->RemoveFromRoot();Classes.Last()=BP->GeneratedClass;Classes.Last()->AddToRoot();}
        UE_LOG(LogTemp,Display,TEXT("CAMERA DEFAULT %s changed=%d"),Path,Changed);
    }
    // Include descendants, map referencers and external actor packages through the asset registry.
    for(int32 I=0;I<Queue.Num();++I)
    {
        const FName Name=Queue[I];if(Seen.Contains(Name))continue;Seen.Add(Name);
        TArray<FName> Refs;Registry.GetReferencers(Name,Refs);
        for(FName Ref:Refs)if(Ref.ToString().StartsWith(TEXT("/Game/")) && !Seen.Contains(Ref))Queue.AddUnique(Ref);
    }
    FString Previous;if(Resume)FFileHelper::LoadFileToString(Previous,*(FPaths::ProjectSavedDir()/TEXT("Logs/CameraAspectBatchApply.log")));
    int32 Worlds=0,Actors=0,ChangedCameras=0;FString Report;
    for(FName Name:Seen)
    {
        if(!Name.ToString().StartsWith(TEXT("/Game/ThirdPerson/Maps/")) && !Name.ToString().StartsWith(TEXT("/Game/__ExternalActors__/ThirdPerson/Maps/")))continue;
        if(Resume && Previous.Contains(TEXT("CAMERA MAP ")+Name.ToString()+TEXT(" changed="))){UE_LOG(LogTemp,Display,TEXT("CAMERA PREVIOUSLY CHECKED %s"),*Name.ToString());continue;}
        TArray<FAssetData> Assets;Registry.GetAssetsByPackageName(Name,Assets);
        const bool Map=Assets.ContainsByPredicate([](const FAssetData& A){return A.AssetClassPath==UWorld::StaticClass()->GetClassPathName();});
        if(!Map && !Name.ToString().Contains(TEXT("/__ExternalActors__/")))continue;
        auto* Package=LoadPackage(nullptr,*Name.ToString(),LOAD_None);if(!Package){UE_LOG(LogTemp,Error,TEXT("CAMERA MAP LOAD FAILED %s"),*Name.ToString());check(Package);}
        ++Worlds;bool Changed=false;AActor* MainActor=nullptr;
        for(TObjectIterator<AActor> It;It;++It)if(It->GetPackage()==Package && !It->IsTemplate() && (It->IsA(Classes[0]) || It->IsA(Classes[1])))
        {
            ++Actors;TInlineComponentArray<UCameraComponent*> Cameras(*It);
            for(auto* Camera:Cameras)
            {
                Report+=Camera->GetPathName()+TEXT(" constrain=")+(Camera->bConstrainAspectRatio?TEXT("true"):TEXT("false"))+TEXT("\n");
                if(Camera->bConstrainAspectRatio){check(!Verify);if(!Changed)BackupMaintenance(Camera);Changed=true;MainActor=*It;Camera->bConstrainAspectRatio=false;++ChangedCameras;}
            }
        }
        if(Changed){UObject* Main=nullptr;for(const auto& Asset:Assets)if(Asset.AssetClassPath==UWorld::StaticClass()->GetClassPathName())Main=Asset.GetAsset();if(!Main)Main=MainActor;check(Main);SaveMaintenance(Main);}
        UE_LOG(LogTemp,Display,TEXT("CAMERA MAP %s changed=%d"),*Name.ToString(),Changed);
        CollectGarbage(RF_NoFlags);
    }
    FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/(Verify?TEXT("CameraAspectVerify.txt"):TEXT("CameraAspectChanges.txt"))));
    UE_LOG(LogTemp,Display,TEXT("CAMERA ASPECT PASS: packages=%d actors=%d explicitCameraChanges=%d"),Worlds,Actors,ChangedCameras);
    for(auto* Class:Classes)Class->RemoveFromRoot();
}
}
