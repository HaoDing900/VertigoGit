#include "VTGFistWindCommandlet.h"
#include "Combat/VTGFistWindComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceCurve.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Animation/AnimMontage.h"
#include "AssetToolsModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "K2Node_Event.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Self.h"
#include "EdGraphSchema_K2.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace FistWindEditor
{
const TCHAR* SourcePath=TEXT("/Game/VFX/Wind_VFX/VFX/Particles/NS_Wind_Flows_02_Fast");
const TCHAR* TargetPath=TEXT("/Game/VFX/Wind_VFX/VFX/Particles/NS_fist_Wind");
void Save(UObject* Asset)
{
    auto* P=Asset->GetOutermost();P->MarkPackageDirty();
    FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;
    check(UPackage::SavePackage(P,Asset,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A));
}
void Inspect(UNiagaraSystem* S,UBlueprint* BP)
{
    TArray<FNiagaraVariable> Vars;S->GetExposedParameters().GetParameters(Vars);
    for(auto V:Vars)
    {
        auto& P=S->GetExposedParameters();
        if(V.GetType()==FNiagaraTypeDefinition::GetFloatDef())UE_LOG(LogTemp,Display,TEXT("FIST_PARAM %s=%f"),*V.GetName().ToString(),P.GetParameterValue<float>(V));
        if(V.GetType()==FNiagaraTypeDefinition::GetIntDef())UE_LOG(LogTemp,Display,TEXT("FIST_PARAM %s=%d"),*V.GetName().ToString(),P.GetParameterValue<int32>(V));
        if(V.GetType()==FNiagaraTypeDefinition::GetVec3Def())UE_LOG(LogTemp,Display,TEXT("FIST_PARAM %s=%s"),*V.GetName().ToString(),*FVector(P.GetParameterValue<FVector3f>(V)).ToString());
    }
    for(auto& H:S->GetEmitterHandles())if(auto* D=H.GetEmitterData())
    {
        UE_LOG(LogTemp,Display,TEXT("FIST_EMITTER %s target=%d local=%d"),*H.GetName().ToString(),int32(D->SimTarget),D->bLocalSpace);
        for(auto* Script:{D->EmitterSpawnScriptProps.Script.Get(),D->EmitterUpdateScriptProps.Script.Get()})if(Script)
        {
            Vars.Empty();Script->RapidIterationParameters.GetParameters(Vars);
            for(auto V:Vars)if(V.GetType()==FNiagaraTypeDefinition::GetFloatDef())UE_LOG(LogTemp,Display,TEXT("FIST_RAPID %s=%f"),*V.GetName().ToString(),Script->RapidIterationParameters.GetParameterValue<float>(V));
        }
    }
    auto* Map=FindFProperty<FMapProperty>(BP->GeneratedClass,TEXT("WeaponDataMap"));check(Map);
    FScriptMapHelper Helper(Map,Map->ContainerPtrToValuePtr<void>(BP->GeneratedClass->GetDefaultObject()));
    for(int32 I=0;I<Helper.GetMaxIndex();++I)if(Helper.IsValidIndex(I))
    {
        FString Key,Value;Map->KeyProp->ExportTextItem_Direct(Key,Helper.GetKeyPtr(I),nullptr,nullptr,0);Map->ValueProp->ExportTextItem_Direct(Value,Helper.GetValuePtr(I),nullptr,nullptr,0);
        UE_LOG(LogTemp,Display,TEXT("FIST_WEAPON key=%s value=%s"),*Key,*Value);
    }
}
void Apply(UNiagaraSystem* Source,UBlueprint* BP)
{
    const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/FistWind/Backup/BP_Player_Sa.uasset");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
    if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),false)==COPY_OK);
    auto* S=LoadObject<UNiagaraSystem>(nullptr,TargetPath,nullptr,LOAD_NoWarn);
    if(!S)S=CastChecked<UNiagaraSystem>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(TEXT("NS_fist_Wind"),TEXT("/Game/VFX/Wind_VFX/VFX/Particles"),Source));
    auto& P=S->GetExposedParameters();
    auto Float=[&](const TCHAR* N,float V){FNiagaraVariable Var(FNiagaraTypeDefinition::GetFloatDef(),N);check(P.IndexOf(Var)!=INDEX_NONE);P.SetParameterValue<float>(V,Var);};
    auto Vector=[&](const TCHAR* N,FVector3f V){FNiagaraVariable Var(FNiagaraTypeDefinition::GetVec3Def(),N);check(P.IndexOf(Var)!=INDEX_NONE);P.SetParameterValue<FVector3f>(V,Var);};
    Float(TEXT("User.Lifetime Min"),.1f);Float(TEXT("User.Lifetime Max"),.18f);Float(TEXT("User.Lifetime Multiply"),1.f);
    Float(TEXT("User.Spawn Rate Curve Mult"),30.f);Float(TEXT("User.Loop Duration"),.6f);
    P.SetParameterValue<int32>(6,FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(),TEXT("User.Spawn Burst")));
    Float(TEXT("User.Dynamic Pan Curve Mult"),6.f);
    Float(TEXT("User.Scale Mult"),1.f);
    Vector(TEXT("User.Scale Vector Min"),FVector3f(.2f,.12f,.25f));Vector(TEXT("User.Scale Vector Max"),FVector3f(.35f,.2f,.4f));
    Vector(TEXT("User.Box Location Size"),FVector3f(10.f));Vector(TEXT("User.Box Location Offset"),FVector3f::ZeroVector);
    Float(TEXT("User.Box Location Size Mult"),1.f);
    TArray<FNiagaraVariable> Vars;P.GetParameters(Vars);
    bool CurveSet=false;
    for(auto V:Vars)if(V.GetName()==TEXT("User.Spawn Rate Curve"))
    {
        auto* Original=CastChecked<UNiagaraDataInterfaceCurve>(P.GetDataInterface(V));
        auto* Curve=DuplicateObject<UNiagaraDataInterfaceCurve>(Original,S);
        Curve->Curve.Reset();Curve->Curve.AddKey(0.f,1.f);Curve->Curve.AddKey(1.f,1.f);Curve->UpdateLUT();
        P.SetDataInterface(Curve,V);CurveSet=true;
    }
    check(CurveSet);
    S->SetWarmupTime(0.f);S->bFixedBounds=true;S->SetFixedBounds(FBox(FVector(-100),FVector(100)));
    for(auto& H:S->GetEmitterHandles())
    {
        auto* D=H.GetEmitterData();check(D);D->bLocalSpace=true;D->FixedBounds=FBox(FVector(-100),FVector(100));
    }
    S->RequestCompile(false);S->WaitForCompilationComplete();Save(S);
    UVTGFistWindComponent* Component=nullptr;
    for(auto* N:BP->SimpleConstructionScript->GetAllNodes())if(N->GetVariableName()==TEXT("FistWindAndRecovery"))Component=CastChecked<UVTGFistWindComponent>(N->ComponentTemplate);
    if(!Component)
    {
        auto* N=BP->SimpleConstructionScript->CreateNode(UVTGFistWindComponent::StaticClass(),TEXT("FistWindAndRecovery"));BP->SimpleConstructionScript->AddNode(N);
        Component=CastChecked<UVTGFistWindComponent>(N->ComponentTemplate);
    }
    Component->WindSystem=S;Component->RecoveryDelay=30.f;Component->PunchMontages.Empty();
    auto* Map=FindFProperty<FMapProperty>(BP->GeneratedClass,TEXT("WeaponDataMap"));check(Map);
    FScriptMapHelper Helper(Map,Map->ContainerPtrToValuePtr<void>(BP->GeneratedClass->GetDefaultObject()));
    auto* Struct=CastFieldChecked<FStructProperty>(Map->ValueProp);
    for(int32 I=0;I<Helper.GetMaxIndex();++I)if(Helper.IsValidIndex(I))
    {
        FNumericProperty* Key=CastField<FNumericProperty>(Map->KeyProp);
        if(auto* Enum=CastField<FEnumProperty>(Map->KeyProp))Key=Enum->GetUnderlyingProperty();
        check(Key);if(Key->GetSignedIntPropertyValue(Helper.GetKeyPtr(I))!=0)continue;
        for(TFieldIterator<FArrayProperty> Prop(Struct->Struct);Prop;++Prop)if(Prop->GetName().StartsWith(TEXT("ComboMontages")))
        {
            FScriptArrayHelper Arr(*Prop,Prop->ContainerPtrToValuePtr<void>(Helper.GetValuePtr(I)));
            auto* Object=CastFieldChecked<FObjectPropertyBase>(Prop->Inner);
            for(int32 J=0;J<Arr.Num();++J)Component->PunchMontages.Add(CastChecked<UAnimMontage>(Object->GetObjectPropertyValue(Arr.GetRawPtr(J))));
        }
    }
    check(Component->PunchMontages.Num()==3);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);check(Log.NumErrors==0);
    Save(BP);UE_LOG(LogTemp,Display,TEXT("FIST_APPLY_SUCCESS montages=3 lifespan=.10-.18 spawn=30 burst=6 recovery=30"));
}
}
extern bool VTGVerifyFistWind();
#include "VTGBattleBoundaryStyle.inl"
#include "VTGWindEnhancement.inl"
extern bool VTGVerifyEnhancedWind();
UVTGFistWindCommandlet::UVTGFistWindCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGFistWindCommandlet::Main(const FString& Params)
{
    if(FParse::Param(*Params,TEXT("FistTrail")))
    {
        const bool Verify=FParse::Param(*Params,TEXT("VerifyOnly"));
        const TCHAR* Path=TEXT("/Game/VFX/Wind_VFX/VFX/Particles/NS_Sa_FistTrail");
        auto* S=LoadObject<UNiagaraSystem>(nullptr,Path,nullptr,LOAD_NoWarn|LOAD_Quiet);
        if(!Verify)
        {
            if(!S)S=CastChecked<UNiagaraSystem>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(TEXT("NS_Sa_FistTrail"),TEXT("/Game/VFX/Wind_VFX/VFX/Particles"),LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/VFX/Wind_VFX/VFX/Particles/NS_Wind_Flows_Trails_02_Fast"))));
            auto& P=S->GetExposedParameters();
            auto Float=[&](const TCHAR* N,float V){FNiagaraVariable Var(FNiagaraTypeDefinition::GetFloatDef(),N);check(P.IndexOf(Var)!=INDEX_NONE);P.SetParameterValue<float>(V,Var);};
            auto Vector=[&](const TCHAR* N,FVector3f V){FNiagaraVariable Var(FNiagaraTypeDefinition::GetVec3Def(),N);check(P.IndexOf(Var)!=INDEX_NONE);P.SetParameterValue<FVector3f>(V,Var);};
            Float(TEXT("User.0_Trail Width"),.018f);
            P.SetParameterValue<FLinearColor>(FLinearColor(.55f,.85f,1.f,.65f),FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(),TEXT("User.0_Trail Color")));
            Float(TEXT("User.1_Movement Lifetime Min"),.18f);Float(TEXT("User.1_Movement Lifetime Max"),.24f);
            Float(TEXT("User.1_Movement Spawn Rate"),24.f);
            Float(TEXT("User.1_Movement Velocity Mult"),0.f);Float(TEXT("User.1_Movement Noise Strength"),0.f);
            Vector(TEXT("User.1_Movement Box Location Offset"),FVector3f::ZeroVector);
            Vector(TEXT("User.1_Movement Box Location Size"),FVector3f(0,5,5));
            S->SetWarmupTime(0.f);S->bFixedBounds=true;S->SetFixedBounds(FBox(FVector(-250),FVector(250)));
            for(auto& H:S->GetEmitterHandles())if(auto* E=H.GetEmitterData())
            {
                // Local carrier particles follow the knuckles; their location events
                // feed world-space ribbons, retaining the actual hand trajectory.
                E->bLocalSpace=H.GetName()==TEXT("Movement");
                TArray<UNiagaraScript*> Scripts;E->GetScripts(Scripts,false); for(auto* Script:Scripts)if(Script){TArray<FNiagaraVariable> Vars;auto& Rapid=Script->RapidIterationParameters;Rapid.GetParameters(Vars);for(auto V:Vars)if(V.GetType()==FNiagaraTypeDefinition::GetFloatDef()){const FString N=V.GetName().ToString();if(N.Contains(TEXT("InitializeRibbon.Lifetime")))Rapid.SetParameterValue<float>(.18f,V);if(N.Contains(TEXT("GenerateLocationEvent.Delay Before Sending Events")))Rapid.SetParameterValue<float>(0.f,V);if(N.Contains(TEXT("GenerateLocationEvent.Event Probability")))Rapid.SetParameterValue<float>(1.f,V);}}
            }
            S->RequestCompile(false);S->WaitForCompilationComplete();FistWindEditor::Save(S);
        }
        check(S);
        auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(BP);
        UVTGFistWindComponent* C=nullptr;for(auto* N:BP->SimpleConstructionScript->GetAllNodes())if(N->GetVariableName()==TEXT("FistWindAndRecovery"))C=Cast<UVTGFistWindComponent>(N->ComponentTemplate);
        check(C);
        if(!Verify){WindEnhancement::Backup(BP);C->WindSystem=S;C->WindScale=1.f;FBlueprintEditorUtils::MarkBlueprintAsModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);check(!Log.NumErrors);FistWindEditor::Save(BP);}
        check(C->WindSystem==S);check(S->GetEmitterHandles().Num()==2);
        UE_LOG(LogTemp,Display,TEXT("FIST_TRAIL_PASS verify=%d"),Verify);return 0;
    }
    if(FParse::Param(*Params,TEXT("TrailInspect")))
    {
        for(const TCHAR* Path:{TEXT("/Game/VFX/Wind_VFX/VFX/Particles/NS_Wind_Flows_Trails_01"),TEXT("/Game/VFX/Wind_VFX/VFX/Particles/NS_Wind_Flows_Trails_02_Fast")})
        {
            auto* S=LoadObject<UNiagaraSystem>(nullptr,Path);check(S);
            UE_LOG(LogTemp,Display,TEXT("TRAIL_SYSTEM %s"),Path);
            TArray<FNiagaraVariable> Vars;auto& P=S->GetExposedParameters();P.GetParameters(Vars);
            for(auto V:Vars){FString Value; if(V.GetType()==FNiagaraTypeDefinition::GetFloatDef())Value=FString::SanitizeFloat(P.GetParameterValue<float>(V));if(V.GetType()==FNiagaraTypeDefinition::GetVec3Def())Value=FVector(P.GetParameterValue<FVector3f>(V)).ToString();UE_LOG(LogTemp,Display,TEXT("TRAIL_PARAM %s %s %s"),*V.GetName().ToString(),*V.GetType().GetName(),*Value);}
            for(auto& H:S->GetEmitterHandles())if(auto* E=H.GetEmitterData()){UE_LOG(LogTemp,Display,TEXT("TRAIL_EMITTER %s local=%d"),*H.GetName().ToString(),E->bLocalSpace);for(auto* R:E->GetRenderers())UE_LOG(LogTemp,Display,TEXT("TRAIL_RENDERER %s"),*R->GetClass()->GetName());}
        }
        return 0;
    }
    if(FParse::Param(*Params,TEXT("FistOpacity"))){WindEnhancement::FistOpacity(FParse::Param(*Params,TEXT("VerifyOnly")));return 0;}
    if(FParse::Param(*Params,TEXT("WindEnhanceInspect"))){WindEnhancement::Inspect();return 0;}
    if(FParse::Param(*Params,TEXT("WindEnhance"))){WindEnhancement::Apply(FParse::Param(*Params,TEXT("VerifyOnly")));return 0;}
    if(FParse::Param(*Params,TEXT("WindEnhanceTest")))return VTGVerifyEnhancedWind()?0:1;
    if(FParse::Param(*Params,TEXT("BoundaryPolish")))
    {
        auto* W=UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar"));check(W);
        AActor* Actor=nullptr;for(TActorIterator<AActor> It(W);It;++It)if(It->GetClass()->GetName()==TEXT("BP_FightingAreaCollision_BarHologram_C"))Actor=*It;check(Actor);
        TMap<FName,UBoxComponent*> Boxes;for(UActorComponent* C:Actor->GetComponents())if(auto* B=Cast<UBoxComponent>(C))Boxes.Add(B->GetFName(),B);
        auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/Actors/FightingAreaCollision/BP_FightingAreaCollision_BarHologram"));check(BP);
        TMap<FName,TArray<USCS_Node*>> Groups;TMap<FName,FTransform> Transforms;
        for(auto* N:BP->SimpleConstructionScript->GetAllNodes())if(auto* M=Cast<UStaticMeshComponent>(N->ComponentTemplate);M && M->ComponentHasTag(TEXT("BattleStyle")))
        {
            FVector P=M->GetRelativeLocation();
            float Best=MAX_flt;FName Group;
            for(FName Name:{FName(TEXT("Box")),FName(TEXT("Box1")),FName(TEXT("Box2"))})
            {
                const bool X=Name==TEXT("Box");const float Value=X?P.Y:P.X;const FVector Center=Boxes[Name]->GetRelativeLocation();const float Line=X?Center.Y:Center.X;
                const float D=FMath::Min(FMath::Abs(Value-Line),FMath::Abs(Value-(Line+(Name==TEXT("Box2")?0:120.f))));
                if(D<Best){Best=D;Group=Name;}
            }
            Groups.FindOrAdd(Group).Add(N);
        }
        int32 Count=0;
        for(auto& Pair:Groups)
        {
            const bool X=Pair.Key==TEXT("Box");auto* B=Boxes[Pair.Key];const FVector Center=B->GetRelativeLocation();const FVector E=B->GetUnscaledBoxExtent()*B->GetRelativeScale3D();const float Half=X?E.X:E.Y;
            Pair.Value.Sort([&](const USCS_Node& A,const USCS_Node& BNode){const FVector APos=CastChecked<USceneComponent>(A.ComponentTemplate)->GetRelativeLocation();const FVector BPos=CastChecked<USceneComponent>(BNode.ComponentTemplate)->GetRelativeLocation();return X?APos.X<BPos.X:APos.Y<BPos.Y;});
            for(int32 I=0;I<Pair.Value.Num();++I)
            {
                auto* N=Pair.Value[I];auto* M=CastChecked<UStaticMeshComponent>(N->ComponentTemplate);
                const float Along=FMath::Lerp(-Half+115,Half-115,float(I)/(Pair.Value.Num()-1));
                const FVector Desired(Center.X+(X?Along:0),Center.Y+(X?0:Along),64.32f);
                M->SetRelativeScale3D(FVector(1,1,.75f));M->SetRelativeLocation(Desired-M->GetRelativeTransform().TransformVector(M->GetStaticMesh()->GetBounds().Origin));
                Transforms.Add(N->GetVariableName(),M->GetRelativeTransform());++Count;
            }
        }
        check(Count==23);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);check(!Log.NumErrors);FistWindEditor::Save(BP);
        Actor=nullptr;for(TActorIterator<AActor> It(W);It;++It)if(It->GetClass()->GetName()==TEXT("BP_FightingAreaCollision_BarHologram_C"))Actor=*It;check(Actor);
        for(UActorComponent* C:Actor->GetComponents())if(auto* M=Cast<UStaticMeshComponent>(C))if(auto* T=Transforms.Find(M->GetFName())){M->Modify();M->SetRelativeTransform(*T);}
        check(UEditorLoadingAndSavingUtils::SaveMap(W,W->GetOutermost()->GetName()));UE_LOG(LogTemp,Display,TEXT("BOUNDARY_POLISH_FINAL_PASS 23 template AND instance transforms centered and spaced"));return 0;
    }
    if(FParse::Param(*Params,TEXT("BoundaryStyle"))) { VTGSetupBoundaryStyle(); return 0; }
    if(FParse::Param(*Params,TEXT("WindDirection")))
    {
        auto* S=LoadObject<UNiagaraSystem>(nullptr,FistWindEditor::TargetPath);check(S);
        auto& P=S->GetExposedParameters();
        for(const TCHAR* N:{TEXT("User.Rotation Min"),TEXT("User.Rotation Max"),TEXT("User.Rotation Rate Vector Min"),TEXT("User.Rotation Rate Vector Max")})
            P.SetParameterValue<FVector3f>(FVector3f::ZeroVector,FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),N));
        P.SetParameterValue<FVector3f>(FVector3f(1,0,0),FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.Velocity Mult")));
        P.SetParameterValue<float>(0.f,FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.Rotation Rate")));
        FistWindEditor::Save(S);UE_LOG(LogTemp,Display,TEXT("WIND_DIRECTION_SUCCESS local X follows wrist-to-knuckle direction"));return 0;
    }
    auto* S=LoadObject<UNiagaraSystem>(nullptr,FistWindEditor::SourcePath);
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(S && BP);
    if(Params.Contains(TEXT("Verify")))return VTGVerifyFistWind()?0:1;
    if(Params.Contains(TEXT("Apply")))FistWindEditor::Apply(S,BP);
    else FistWindEditor::Inspect(S,BP);
    return 0;
}


