#include "NiagaraRendererProperties.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "NiagaraDataInterfaceColorCurve.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "StaticMeshResources.h"
#include "Misc/FileHelper.h"

namespace WindEnhancement
{
void Inspect()
{
 auto* S=LoadObject<UNiagaraSystem>(nullptr,FistWindEditor::TargetPath);check(S);auto& P=S->GetExposedParameters();TArray<FNiagaraVariable> Vars;P.GetParameters(Vars);
 for(auto V:Vars)
 {
  UE_LOG(LogTemp,Display,TEXT("WIND_PARAMETER %s type=%s"),*V.GetName().ToString(),*V.GetType().GetName());
  if(V.GetType()==FNiagaraTypeDefinition::GetColorDef())UE_LOG(LogTemp,Display,TEXT("WIND_COLOR %s = %s"),*V.GetName().ToString(),*P.GetParameterValue<FLinearColor>(V).ToString());
  if(V.GetName()==TEXT("User.Color Curve"))if(auto* C=Cast<UNiagaraDataInterfaceColorCurve>(P.GetDataInterface(V)))for(const auto& K:C->AlphaCurve.GetConstRefOfKeys())UE_LOG(LogTemp,Display,TEXT("WIND_ALPHA %s t=%f value=%f"),*V.GetName().ToString(),K.Time,K.Value);
 }
 for(auto& H:S->GetEmitterHandles())if(auto* E=H.GetEmitterData())for(auto* R:E->GetRenderers())
 {
  TArray<UMaterialInterface*> Materials;R->GetUsedMaterials(nullptr,Materials);
  for(auto* M:Materials)if(M){UE_LOG(LogTemp,Display,TEXT("WIND_MATERIAL %s blend=%d"),*M->GetPathName(),int32(M->GetBlendMode()));TArray<FMaterialParameterInfo> Info;TArray<FGuid> IDs;M->GetAllScalarParameterInfo(Info,IDs);for(auto I:Info){float V=0;M->GetScalarParameterValue(I,V);UE_LOG(LogTemp,Display,TEXT("WIND_MATERIAL_SCALAR %s = %f"),*I.Name.ToString(),V);}}
 }
 auto* Hammer=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/AnimationAsset/MC_Sample/Demo/Objects/Hammer/SM_Prop_Hammer"));check(Hammer);
 UE_LOG(LogTemp,Display,TEXT("HAMMER_BOUNDS %s"),*Hammer->GetBoundingBox().ToString());for(UStaticMeshSocket* Socket:Hammer->Sockets)if(Socket)UE_LOG(LogTemp,Display,TEXT("HAMMER_SOCKET %s loc=%s rot=%s"),*Socket->SocketName.ToString(),*Socket->RelativeLocation.ToString(),*Socket->RelativeRotation.ToString());
 if(auto* Data=Hammer->GetRenderData();Data && Data->LODResources.Num())
 {
  FString CSV=TEXT("x,y,z\n");const auto& Positions=Data->LODResources[0].VertexBuffers.PositionVertexBuffer;
  for(uint32 I=0;I<Positions.GetNumVertices();++I){auto V=Positions.VertexPosition(I);CSV+=FString::Printf(TEXT("%f,%f,%f\n"),V.X,V.Y,V.Z);}
  FFileHelper::SaveStringToFile(CSV,*(FPaths::ProjectDir()/TEXT("Scripts/CombatWindEnhancement/hammer-vertices.csv")));
 }
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(BP);
 for(auto* N:BP->SimpleConstructionScript->GetAllNodes())if(N->GetVariableName()==TEXT("MeleeWeapon"))if(auto* Mesh=Cast<UStaticMeshComponent>(N->ComponentTemplate))UE_LOG(LogTemp,Display,TEXT("HAMMER_COMPONENT rel=%s socket=%s"),*Mesh->GetRelativeTransform().ToString(),*N->AttachToName.ToString());
}
void Backup(UObject* O)
{
 const auto File=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());const auto Copy=FPaths::ProjectDir()/TEXT("Scripts/CombatWindEnhancement/Backup")/(O->GetName()+TEXT(".uasset"));
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Copy),true);if(IFileManager::Get().FileExists(*File) && !IFileManager::Get().FileExists(*Copy))check(IFileManager::Get().Copy(*Copy,*File,false)==COPY_OK);
}
void Enhance(UNiagaraSystem* System,bool Hammer)
{
 auto& P=System->GetExposedParameters();
 auto Color=[&](FName Name,FLinearColor Value){FNiagaraVariable V(FNiagaraTypeDefinition::GetColorDef(),Name);check(P.IndexOf(V)!=INDEX_NONE);P.SetParameterValue<FLinearColor>(Value,V);};
 Color(TEXT("User.Color Minimum"),FLinearColor(.864f*1.15f,.928f*1.15f,.960f*1.15f,Hammer?.22f:.24f));
 Color(TEXT("User.Color Maximum"),FLinearColor(.637f*1.15f,.86905f*1.15f,.910f*1.15f,Hammer?.50f:.58f));
 FNiagaraVariable CurveVar(FNiagaraTypeDefinition(UNiagaraDataInterfaceColorCurve::StaticClass()),TEXT("User.Color Curve"));
 auto* Source=CastChecked<UNiagaraDataInterfaceColorCurve>(P.GetDataInterface(CurveVar));auto* Curve=DuplicateObject<UNiagaraDataInterfaceColorCurve>(Source,System);
 Curve->AlphaCurve.Reset();Curve->AlphaCurve.AddKey(0,0);Curve->AlphaCurve.AddKey(.12f,1);Curve->AlphaCurve.AddKey(.7f,.9f);Curve->AlphaCurve.AddKey(1,0);Curve->UpdateLUT();P.SetDataInterface(Curve,CurveVar);
 // Leave density, short lifetimes, direction and emitter count unchanged.
 System->RequestCompile(false);System->WaitForCompilationComplete();FistWindEditor::Save(System);
}
void FistOpacity(bool Verify)
{
 auto* System=LoadObject<UNiagaraSystem>(nullptr,FistWindEditor::TargetPath);check(System);
 auto& P=System->GetExposedParameters();
 if(!Verify)
 {
  const FString File=FPackageName::LongPackageNameToFilename(System->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
  const FString Copy=FPaths::ProjectDir()/TEXT("Scripts/CombatWindEnhancement/BackupOpacity/NS_fist_Wind.uasset");
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Copy),true);
  if(!IFileManager::Get().FileExists(*Copy))check(IFileManager::Get().Copy(*Copy,*File,false)==COPY_OK);
 }
 for(const auto& Entry:TArray<TPair<FName,float>>{{TEXT("User.Color Minimum"),.24f},{TEXT("User.Color Maximum"),.58f}})
 {
  const FNiagaraVariable V(FNiagaraTypeDefinition::GetColorDef(),Entry.Key);check(P.IndexOf(V)!=INDEX_NONE);
  FLinearColor Color=P.GetParameterValue<FLinearColor>(V);
  UE_LOG(LogTemp,Display,TEXT("FIST_OPACITY %s before=%s targetAlpha=%.2f verify=%d"),*Entry.Key.ToString(),*Color.ToString(),Entry.Value,Verify);
  if(!Verify){Color.A=Entry.Value;P.SetParameterValue<FLinearColor>(Color,V);}
  check(FMath::IsNearlyEqual(P.GetParameterValue<FLinearColor>(V).A,Entry.Value));
 }
 if(!Verify)FistWindEditor::Save(System);
 UE_LOG(LogTemp,Display,TEXT("FIST_OPACITY_PASS verify=%d"),Verify);
}
void Apply(bool Verify)
{
 auto* Fist=LoadObject<UNiagaraSystem>(nullptr,FistWindEditor::TargetPath);auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(Fist && BP);
 const FString HammerPath=TEXT("/Game/VFX/Wind_VFX/VFX/Particles/NS_hammer_Wind");auto* Hammer=LoadObject<UNiagaraSystem>(nullptr,*HammerPath,nullptr,LOAD_NoWarn|LOAD_Quiet);
 if(!Verify)
 {
  Backup(Fist);Backup(BP);Enhance(Fist,false);
  if(!Hammer)Hammer=CastChecked<UNiagaraSystem>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(TEXT("NS_hammer_Wind"),TEXT("/Game/VFX/Wind_VFX/VFX/Particles"),Fist));
  Backup(Hammer);Enhance(Hammer,true);
 }
 check(Hammer);
 UVTGFistWindComponent* C=nullptr;for(auto* N:BP->SimpleConstructionScript->GetAllNodes())if(N->GetVariableName()==TEXT("FistWindAndRecovery"))C=Cast<UVTGFistWindComponent>(N->ComponentTemplate);check(C);
 if(!Verify)
 {
  C->WindScale=1.25f;C->HammerWindSystem=Hammer;C->HammerMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/AnimationAsset/MC_Sample/Demo/Objects/Hammer/SM_Prop_Hammer"));check(C->HammerMesh);
  C->HammerWindOffset=FVector(0,-20.4,55.62);C->HammerWindRotation=FRotator(0,-90,0);C->HammerWindScale=1.4f;C->HammerMontages.Empty();
  auto* Map=FindFProperty<FMapProperty>(BP->GeneratedClass,TEXT("WeaponDataMap"));check(Map);FScriptMapHelper Values(Map,Map->ContainerPtrToValuePtr<void>(BP->GeneratedClass->GetDefaultObject()));auto* Struct=CastFieldChecked<FStructProperty>(Map->ValueProp);
  for(int32 I=0;I<Values.GetMaxIndex();++I)if(Values.IsValidIndex(I))
  {
   auto* Key=CastField<FNumericProperty>(Map->KeyProp);if(auto* Enum=CastField<FEnumProperty>(Map->KeyProp))Key=Enum->GetUnderlyingProperty();check(Key);if(Key->GetSignedIntPropertyValue(Values.GetKeyPtr(I))!=1)continue;
   for(TFieldIterator<FArrayProperty> Prop(Struct->Struct);Prop;++Prop)if(Prop->GetName().StartsWith(TEXT("ComboMontages")))
   {FScriptArrayHelper A(*Prop,Prop->ContainerPtrToValuePtr<void>(Values.GetValuePtr(I)));auto* Object=CastFieldChecked<FObjectPropertyBase>(Prop->Inner);for(int32 J=0;J<A.Num();++J)C->HammerMontages.Add(CastChecked<UAnimMontage>(Object->GetObjectPropertyValue(A.GetRawPtr(J))));}
  }
  check(C->HammerMontages.Num()==3);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);check(!Log.NumErrors);FistWindEditor::Save(BP);
  UE_LOG(LogTemp,Display,TEXT("WIND_ENHANCEMENT_PLAYER errors=%d warnings=%d"),Log.NumErrors,Log.NumWarnings);
 }
 check(C->WindScale==1.25f && C->HammerWindSystem==Hammer && C->HammerMontages.Num()==3);
 for(UAnimMontage* M:C->HammerMontages)for(const auto& E:M->Notifies)if(E.NotifyStateClass)UE_LOG(LogTemp,Display,TEXT("HAMMER_NOTIFY %s %s %.3f..%.3f"),*M->GetName(),*E.NotifyStateClass->GetClass()->GetName(),E.GetTime(),E.GetTime()+E.GetDuration());
 UE_LOG(LogTemp,Display,TEXT("WIND_ENHANCEMENT_ASSET_PASS verify=%d"),Verify);
}
}
