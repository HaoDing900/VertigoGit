// Included after VTGPolish's asset/reflection helpers; intentionally edits only sewer FX/audio assets.
namespace VTGSewerFX
{
using namespace VTGPolish;
void Backup(UObject* Asset)
{
 const auto File=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 if(!IFileManager::Get().FileExists(*File))return;
 const auto Target=FPaths::ProjectDir()/TEXT("Scripts/SewerFX/Backup")/(Asset->GetName()+FPackageName::GetAssetPackageExtension());
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target),true);
 if(!IFileManager::Get().FileExists(*Target))check(IFileManager::Get().Copy(*Target,*File)==COPY_OK);
}
FString Name(const UStruct* Type,void* Data,const TCHAR* FieldName)
{
 auto* P=Field(Type,FieldName);auto* Value=P->ContainerPtrToValuePtr<void>(Data);
 if(auto* N=CastField<FNameProperty>(P))return N->GetPropertyValue(Value).ToString();
 return CastFieldChecked<FStrProperty>(P)->GetPropertyValue(Value);
}
double Number(const UStruct* Type,void* Data,const TCHAR* FieldName)
{
 auto* P=CastFieldChecked<FNumericProperty>(Field(Type,FieldName));auto* V=P->ContainerPtrToValuePtr<void>(Data);
 return P->IsInteger()?double(P->GetSignedIntPropertyValue(V)):P->GetFloatingPointPropertyValue(V);
}
UObject* Object(const UStruct* Type,void* Data,const TCHAR* FieldName)
{return CastFieldChecked<FObjectPropertyBase>(Field(Type,FieldName))->GetObjectPropertyValue_InContainer(Data);}
void SetEffect(UAnimMontage* Montage,UNiagaraSystem* System,float Time,float Duration,FName Socket=NAME_None)
{
 Backup(Montage);
 UVTGAttackEffectNotify* Notify=nullptr;
 for(auto& Event:Montage->Notifies)if(auto* N=Cast<UVTGAttackEffectNotify>(Event.Notify)){check(!Notify);Notify=N;}
 if(!Notify)
 {
  Notify=NewObject<UVTGAttackEffectNotify>(Montage,NAME_None,RF_Transactional);
  auto& Event=Montage->Notifies.AddDefaulted_GetRef();Event.Notify=Notify;Event.NotifyName=TEXT("VTG_AttackEffect");Event.Guid=FGuid::NewGuid();Event.Link(Montage,Time);Event.TriggerTimeOffset=GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetAfter);
 }
 Notify->Effect=System;Notify->MediumEffect=nullptr;Notify->LargeEffect=nullptr;
 Notify->ActiveSeconds=Duration;Notify->SocketName=Socket;
 Montage->SortNotifies();Montage->PostEditChange();Save(Montage);
}
void Sound(const TCHAR* Cue,const TCHAR* WaveName,float Pitch=1)
{auto* Asset=Load<USoundCue>(Root+TEXT("Audio/")+Cue);Backup(Asset);VTGPolish::Sound(Cue,{Wave(WaveName)},Pitch);}

int32 Repair(bool Verify,bool Test)
{
 auto* Egg=Load<UBlueprint>(Root+TEXT("BP_SeaMonster_Egg"));
 auto* Boss=Load<UBlueprint>(Root+TEXT("BP_Boss_SeaMonster"));
 auto* Projectile=Load<UBlueprint>(Root+TEXT("BP_SeaMonster_AcidProjectile"));
 auto* Table=Load<UDataTable>(Root+TEXT("DT_SeaMonster_Phases"));
 auto* Gas=Load<UAnimMontage>(Root+TEXT("Montages/AM_SeaMonsterEgg_ReleaseGas"));
 auto* Spit=Load<UAnimMontage>(Root+TEXT("Montages/AM_EggProducer_AcidSpit"));
 auto* Burst=Load<UNiagaraSystem>(TEXT("/Game/Resource/VFX/BloodPack/BrainBurst/Niagara/NS_BrainBurst_Cyan"));
 auto* Blood=Load<UNiagaraSystem>(TEXT("/Game/Resource/VFX/BloodPack/Hit/Niagara/NS_BulletHit_Cyan_Med1"));
 auto* Poison=Load<UNiagaraSystem>(TEXT("/Game/VFX/VFX_EnemyAttack/VFX_PoisonCloud/NS_PoisonCloud_Mid"));
 auto* Acid=Load<UNiagaraSystem>(TEXT("/Game/VFX/VFX_EnemyAttack/VFX_AcidSpit/NS_AcidSpit_Burst"));
 auto* RS=Table->GetRowStruct();auto* AP=CastFieldChecked<FArrayProperty>(Field(RS,TEXT("Abilities")));UScriptStruct* AS=CastFieldChecked<FStructProperty>(AP->Inner)->Struct;
 auto* BossRow=Table->FindRowUnchecked(TEXT("SeaMonster_P1"));check(BossRow);
 FScriptArrayHelper Abilities(AP,AP->ContainerPtrToValuePtr<void>(BossRow));
 int32 AcidIndex=INDEX_NONE;
 for(int I=0;I<Abilities.Num();++I)if(Name(AS,Abilities.GetRawPtr(I),TEXT("AbilityName"))==TEXT("AcidSpit")){check(AcidIndex==INDEX_NONE);AcidIndex=I;}
 if(!Verify)
 {
  for(auto* BP:{Egg,Boss})
  {
   Backup(BP);auto* Feedback=Component<UVTGEnemyFeedbackComponent>(BP,TEXT("CombatFeedback"));Feedback->HitEffect=Blood;
   if(BP==Egg)Feedback->DeathEffect=Burst;
   Compile(BP);Save(BP);
  }
  // Fixed requested mid-sized effect: remove the previous scale-based Small/Mid/Large choice.
  SetEffect(Gas,Poison,.8f,1.7f);
  SetEffect(Spit,Acid,.7f,.6f,TEXT("spine_006"));
  Backup(Projectile);
  auto* Visual=Component<UNiagaraComponent>(Projectile,TEXT("AcidVisual"));Visual->SetAsset(Acid);Visual->SetAutoActivate(true);
  for(auto* N:Projectile->SimpleConstructionScript->GetAllNodes())
  {
   if(auto* Move=Cast<UProjectileMovementComponent>(N->ComponentTemplate)){Move->InitialSpeed=850;Move->MaxSpeed=850;Move->ProjectileGravityScale=.12f;}
   // The toolkit's placeholder sphere is only collision; the acid Niagara is the visible projectile.
   if(auto* Mesh=Cast<UStaticMeshComponent>(N->ComponentTemplate))Mesh->SetVisibility(false);
  }
  Compile(Projectile);Num(Projectile->GeneratedClass,Projectile->GeneratedClass->GetDefaultObject(),TEXT("InitialLifeSpan"),5);Save(Projectile);
  Backup(Table);
  if(AcidIndex==INDEX_NONE){check(Abilities.Num()>0);AcidIndex=Abilities.AddValue();AS->CopyScriptStruct(Abilities.GetRawPtr(AcidIndex),Abilities.GetRawPtr(0));}
  auto* Ability=Abilities.GetRawPtr(AcidIndex);
  Label(AS,Ability,TEXT("AbilityName"),TEXT("AcidSpit"));
  Obj(AS,Ability,TEXT("AbilityClass"),Load<UBlueprint>(TEXT("/Game/AI/BossAIToolkit/Blueprints/Abilities/Damage/BP_Ability_Projectile"))->GeneratedClass);
  Label(AS,Ability,TEXT("ComponentTag"),TEXT("spine_006"));Num(AS,Ability,TEXT("Duration"),3);Num(AS,Ability,TEXT("Cooldown"),7);
  *CastFieldChecked<FStructProperty>(Field(AS,TEXT("Modifier")))->ContainerPtrToValuePtr<FVector>(Ability)=FVector(1,0,0);
  UScriptStruct* Type=nullptr;auto* Target=Struct(AS,Ability,TEXT("Targeting"),Type);Num(Type,Target,TEXT("MinDistance"),450);Num(Type,Target,TEXT("MaxDistance"),1800);
  auto* Damage=Struct(AS,Ability,TEXT("Damage"),Type);Num(Type,Damage,TEXT("DamageMultiplier"),.75);
  auto* Visuals=Struct(AS,Ability,TEXT("Visuals"),Type);Obj(Type,Visuals,TEXT("AnimMontage"),Spit);Obj(Type,Visuals,TEXT("SpawnClass"),Projectile->GeneratedClass);Obj(Type,Visuals,TEXT("ParticleEffects"),nullptr);
  auto* EggRow=Table->FindRowUnchecked(TEXT("SeaMonsterEgg_P1"));check(EggRow);FScriptArrayHelper EggAbilities(AP,AP->ContainerPtrToValuePtr<void>(EggRow));check(EggAbilities.Num()>0);
  auto* EggVisuals=Struct(AS,EggAbilities.GetRawPtr(0),TEXT("Visuals"),Type);Obj(Type,EggVisuals,TEXT("ParticleEffects"),nullptr);Save(Table);
  Sound(TEXT("SC_EggMother_Roar"),TEXT("attack_long_03"));Sound(TEXT("SC_EggMother_Swing"),TEXT("attack_medium_03"));Sound(TEXT("SC_EggMother_Slam"),TEXT("attack_medium_04"));Sound(TEXT("SC_EggMother_Gas"),TEXT("breath_long_02"));Sound(TEXT("SC_EggMother_Hurt"),TEXT("hit_03"));
  Sound(TEXT("SC_SeaEgg_Gas"),TEXT("attack_short_04"),1.2f);Sound(TEXT("SC_SeaEgg_Hurt"),TEXT("hit_05"),1.2f);
  // Give the acid montage its own cue so changing the gas sound later does not change the spit.
  const auto CuePath=Root+TEXT("Audio/SC_EggMother_AcidSpit");auto* Cue=LoadObject<USoundCue>(nullptr,*CuePath);
  if(!Cue)Cue=CastChecked<USoundCue>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(TEXT("SC_EggMother_AcidSpit"),Root+TEXT("Audio"),Load<USoundCue>(Root+TEXT("Audio/SC_EggMother_Gas"))));
  Backup(Cue);VTGPolish::Sound(TEXT("SC_EggMother_AcidSpit"),{Wave(TEXT("attack_long_04"))});
  bool AudioFound=false;for(auto& Event:Spit->Notifies)if(auto* Notify=Cast<UAnimNotify_PlaySound>(Event.Notify)){Notify->Sound=Cue;AudioFound=true;}
  if(!AudioFound){auto& Event=Spit->Notifies.AddDefaulted_GetRef();auto* Notify=NewObject<UAnimNotify_PlaySound>(Spit);Notify->Sound=Cue;Event.Notify=Notify;Event.NotifyName=TEXT("AcidSpitSound");Event.Guid=FGuid::NewGuid();Event.Link(Spit,.7f);}
  Spit->SortNotifies();Spit->PostEditChange();Save(Spit);
 }
 for(auto* BP:{Egg,Boss,Projectile})Compile(BP);
 for(auto* BP:{Egg,Boss})
 {
  auto* Feedback=Component<UVTGEnemyFeedbackComponent>(BP,TEXT("CombatFeedback"));check(Feedback->HitEffect==Blood);if(BP==Egg)check(Feedback->DeathEffect==Burst);
 }
 check(AcidIndex!=INDEX_NONE);auto* Ability=Abilities.GetRawPtr(AcidIndex);UScriptStruct* Type=nullptr;
 auto* Visuals=Struct(AS,Ability,TEXT("Visuals"),Type);check(Object(Type,Visuals,TEXT("AnimMontage"))==Spit && Object(Type,Visuals,TEXT("SpawnClass"))==Projectile->GeneratedClass);
 auto* Target=Struct(AS,Ability,TEXT("Targeting"),Type);check(Number(Type,Target,TEXT("MinDistance"))==450 && Number(Type,Target,TEXT("MaxDistance"))==1800);
 auto* Damage=Struct(AS,Ability,TEXT("Damage"),Type);check(Number(Type,Damage,TEXT("DamageMultiplier"))>0);
 int NotifyCount=0;for(const auto& Event:Gas->Notifies)if(auto* Notify=Cast<UVTGAttackEffectNotify>(Event.Notify)){++NotifyCount;check(Notify->Effect==Poison && !Notify->MediumEffect && !Notify->LargeEffect);}check(NotifyCount==1);
 check(Component<UNiagaraComponent>(Projectile,TEXT("AcidVisual"))->GetAsset()==Acid);
 if(Test)
 {
  auto* Player=Load<UBlueprint>(TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));
  if(!VTGTestCombatPolish(Player,Boss))return 2;
 }
 UE_LOG(LogTemp,Display,TEXT("SEWER FX PASS: egg burst, fixed Mid poison, both blood reactions, one toolkit acid ability (450-1800 cm), Niagara projectile and montage; assets compiled."));
 return 0;
}
}

