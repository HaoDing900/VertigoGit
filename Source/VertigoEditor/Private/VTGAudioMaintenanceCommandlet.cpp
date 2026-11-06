#include "VTGAudioMaintenanceCommandlet.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Audio/VTGPlayerAudioComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/SaveGame.h"
#include "Engine/World.h"
#include "Engine/Level.h"

namespace AudioMaintenance
{
const FString Root=TEXT("/Game/Characters/Monster/SeaMonsterBoss/");
void BackupPackage(const FString& Package)
{
    if(!Package.StartsWith(TEXT("/Game/")))return;
    FString Source;
    if(!FPackageName::DoesPackageExist(Package,&Source))return;
    const FString Target=FPaths::ProjectDir()/TEXT("Scripts/AudioMaintenance/Backup")/(Package.RightChop(1)+TEXT(".")+FPaths::GetExtension(Source));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target),true);
    if(!IFileManager::Get().FileExists(*Target))check(IFileManager::Get().Copy(*Target,*Source)==COPY_OK);
}
void Save(UObject* Object)
{
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    check(UPackage::SavePackage(Object->GetOutermost(),Object,*FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));
}
FProperty* Field(const UStruct* S,const FString& Name)
{
    for(TFieldIterator<FProperty> It(S);It;++It)if(It->GetName()==Name || It->GetName().StartsWith(Name+TEXT("_")))return *It;
    checkNoEntry();return nullptr;
}
void ToxicGas(bool Apply)
{
    auto* Table=LoadObject<UDataTable>(nullptr,*(Root+TEXT("DT_SeaMonster_Phases")));check(Table);
    auto* Array=CastFieldChecked<FArrayProperty>(Field(Table->GetRowStruct(),TEXT("Abilities")));
    auto* Ability=CastFieldChecked<FStructProperty>(Array->Inner);int32 Found=0;
    if(Apply)BackupPackage(Table->GetOutermost()->GetName());
    for(const auto& Row:Table->GetRowMap())
    {
        FScriptArrayHelper Entries(Array,Array->ContainerPtrToValuePtr<void>(Row.Value));
        for(int32 I=0;I<Entries.Num();++I)
        {
            auto* Data=Entries.GetRawPtr(I);
            if(CastFieldChecked<FNameProperty>(Field(Ability->Struct,TEXT("AbilityName")))->GetPropertyValue_InContainer(Data)!=TEXT("Toxic Gas"))continue;
            auto* Damage=CastFieldChecked<FStructProperty>(Field(Ability->Struct,TEXT("Damage")));auto* D=Damage->ContainerPtrToValuePtr<void>(Data);
            auto* Mult=CastFieldChecked<FNumericProperty>(Field(Damage->Struct,TEXT("DamageMultiplier")));auto* Value=Mult->ContainerPtrToValuePtr<void>(D);
            const double Before=Mult->GetFloatingPointPropertyValue(Value);check(FMath::IsNearlyEqual(Before,.4,.0001)||FMath::IsNearlyEqual(Before,.8,.0001));
            if(Apply)Mult->SetFloatingPointPropertyValue(Value,.8);
            UE_LOG(LogTemp,Display,TEXT("TOXIC GAS %s multiplier %.3f -> %.3f"),*Row.Key.ToString(),Before,Mult->GetFloatingPointPropertyValue(Value));++Found;
        }
    }
    check(Found);if(Apply)Save(Table);
}
void Assets(bool Move,bool Verify)
{
    auto* Player=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(Player);
    UVTGPlayerAudioComponent* Audio=nullptr;
    for(auto* Node:Player->SimpleConstructionScript->GetAllNodes())if(Node->GetVariableName()==TEXT("PlayerAudio"))Audio=Cast<UVTGPlayerAudioComponent>(Node->ComponentTemplate);
    check(Audio);TMap<UObject*,FString> Targets;
    auto Add=[&](UObject* Object,const FString& Folder){check(Object);const FString To=Folder+Object->GetName();check(!Targets.Contains(Object)||Targets[Object]==To);Targets.Add(Object,To);};
    auto FootCue=[&](USoundBase* Sound)
    {
        Add(Sound,TEXT("/Game/SFX/SaFootstep/Cues/"));
        if(auto* Cue=Cast<USoundCue>(Sound))for(USoundNode* Node:Cue->AllNodes)if(auto* Wave=Cast<USoundNodeWavePlayer>(Node))if(Wave->GetSoundWave())Add(Wave->GetSoundWave(),TEXT("/Game/SFX/SaFootstep/Waves/"));
    };
    for(auto* Pool:{&Audio->DefaultSteps,&Audio->BarefootSteps})for(USoundBase* Sound:*Pool)FootCue(Sound);
    for(auto* Pool:{&Audio->ConcreteSteps,&Audio->MetalSteps,&Audio->WaterSteps,&Audio->WoodSteps})for(USoundBase* Sound:*Pool)Add(Sound,TEXT("/Game/SFX/SaFootstep/Surfaces/"));
    Add(Audio->StepAttenuation,TEXT("/Game/SFX/SaFootstep/Settings/"));
    for(const TCHAR* Name:{TEXT("SC_EggMother_Roar"),TEXT("SC_EggMother_Swing"),TEXT("SC_EggMother_Slam"),TEXT("SC_EggMother_Gas"),TEXT("SC_EggMother_AcidSpit"),TEXT("SC_EggMother_Hurt"),TEXT("SC_EggMother_Death"),TEXT("SC_EggMother_EggDrop"),TEXT("SC_SeaEgg_Gas"),TEXT("SC_SeaEgg_Hurt"),TEXT("SC_SeaEgg_Death")})
    {
        auto* Cue=LoadObject<USoundCue>(nullptr,*(Root+TEXT("Audio/used/")+Name),nullptr,LOAD_NoWarn|LOAD_Quiet);
        if(!Cue)Cue=LoadObject<USoundCue>(nullptr,*(Root+TEXT("Audio/")+Name));check(Cue);Add(Cue,Root+TEXT("Audio/used/"));
        for(USoundNode* Node:Cue->AllNodes)if(auto* Wave=Cast<USoundNodeWavePlayer>(Node))if(auto* Sound=Wave->GetSoundWave())
        {
            FString Folder=FPackageName::GetLongPackagePath(Sound->GetOutermost()->GetName());
            if(!Folder.EndsWith(TEXT("/used")))Folder+=TEXT("/used");Add(Sound,Folder+TEXT("/"));
        }
    }
    auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();Registry.SearchAllAssets(true);
    TArray<FAssetRenameData> Renames;FString Manifest=TEXT("OldPackage\tNewPackage\n");TSet<FName> References;
    for(const auto& Pair:Targets)
    {
        const FString From=Pair.Key->GetOutermost()->GetName();const FString To=Pair.Value;
        UE_LOG(LogTemp,Display,TEXT("AUDIO MOVE %s -> %s"),*From,*To);
        if(Verify){check(From==To);continue;}
        if(From==To)continue;
        check(!FPackageName::DoesPackageExist(To));Manifest+=From+TEXT("\t")+To+TEXT("\n");
        if(Move)
        {
            BackupPackage(From);TArray<FName> Refs;Registry.GetReferencers(*From,Refs,UE::AssetRegistry::EDependencyCategory::Package);
            for(FName Ref:Refs)if(Ref.ToString().StartsWith(TEXT("/Game/"))){References.Add(Ref);BackupPackage(Ref.ToString());}
            Renames.Emplace(Pair.Key,FPackageName::GetLongPackagePath(To),FPackageName::GetShortName(To));
        }
    }
    if(Move && Renames.Num())
    {
        const FString File=FPaths::ProjectDir()/TEXT("Scripts/AudioMaintenance/AssetMoves.tsv");IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
        check(!IFileManager::Get().FileExists(*File));check(FFileHelper::SaveStringToFile(Manifest,*File));
        // Load referencers so the editor rename service can update and save their object references.
        for(FName Ref:References)LoadPackage(nullptr,*Ref.ToString(),LOAD_None);
        check(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().RenameAssets(Renames));
        for(const auto& Pair:Targets){check(Pair.Key->GetOutermost()->GetName()==Pair.Value);Save(Pair.Key);}
        for(FName Ref:References)if(auto* Package=FindPackage(nullptr,*Ref.ToString());Package && Package->IsDirty())
        {FString Filename;check(FPackageName::DoesPackageExist(Ref.ToString(),&Filename));FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(Package,nullptr,*Filename,Args));}
    }
    UE_LOG(LogTemp,Display,TEXT("AUDIO ASSETS PASS: %d assets, %d renames"),Targets.Num(),Renames.Num());
}
}
int32 UVTGAudioMaintenanceCommandlet::Main(const FString& Params)
{
    if(FParse::Param(*Params,TEXT("InspectSave")))
    {
        auto* Save=UGameplayStatics::LoadGameFromSlot(TEXT("VTG_Slot_0"),0);
        if(Save)for(TFieldIterator<FProperty> It(Save->GetClass());It;++It){FString V;It->ExportText_InContainer(0,V,Save,Save,Save,PPF_None);if(It->GetName().Contains(TEXT("Persistent")) || It->GetName().Contains(TEXT("Level")) || It->GetName().Contains(TEXT("Player")) || It->GetName().Contains(TEXT("Stage")))UE_LOG(LogTemp,Display,TEXT("SAVE INSPECT %s=%s"),*It->GetName(),*V);}
        auto* Map=LoadObject<UWorld>(nullptr,TEXT("/Game/ThirdPerson/Maps/L_SewerUnderApartment"));check(Map);
        for(AActor* Actor:Map->PersistentLevel->Actors)if(Actor && (Actor->GetClass()->GetName().Contains(TEXT("Hammer")) || Actor->GetClass()->GetName().Contains(TEXT("Player_Sa")) || Actor->GetClass()->GetName().Contains(TEXT("SewerUnderApt"))))
        {UE_LOG(LogTemp,Display,TEXT("MAP INSPECT %s class=%s"),*Actor->GetName(),*Actor->GetClass()->GetPathName());for(TFieldIterator<FProperty> It(Actor->GetClass());It;++It)if(It->GetName().Contains(TEXT("Combat")) || It->GetName().Contains(TEXT("Story")) || It->GetName().Contains(TEXT("CanPunch")) || It->GetName().Contains(TEXT("LockInput")) || It->GetName().Contains(TEXT("Defeated"))){FString V;It->ExportText_InContainer(0,V,Actor,Actor,Actor,PPF_None);UE_LOG(LogTemp,Display,TEXT("MAP STATE %s=%s"),*It->GetName(),*V);}}
        return 0;
    }
    if(FParse::Param(*Params,TEXT("ToxicGas")))AudioMaintenance::ToxicGas(FParse::Param(*Params,TEXT("Apply")));
    else AudioMaintenance::Assets(FParse::Param(*Params,TEXT("Move")),FParse::Param(*Params,TEXT("VerifyOnly")));
    return 0;
}
