#include "VTGCombatPolishCommandlet.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Combat/VTGCombatPresentation.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Texture2D.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTime.h"
#include "Misc/PackageName.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "UI/VTGBossHealthBarComponent.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

extern bool VTGTestCombatPolish(UBlueprint *, UBlueprint *);
extern int32 VTGRenderCombatHUD(const FString &);
namespace VTGPolish
{
const FString Root = TEXT("/Game/Characters/Monster/SeaMonsterBoss/");
TSet<UPackage *> Saved;
template <class T> T *Load(const FString &Path)
{
    auto *O = LoadObject<T>(nullptr, *Path);
    checkf(O, TEXT("Missing %s"), *Path);
    return O;
}
void Save(UObject *O)
{
    auto *P = O->GetOutermost();
    P->MarkPackageDirty();
    FSavePackageArgs A;
    A.TopLevelFlags = RF_Public | RF_Standalone;
    A.SaveFlags = SAVE_NoError;
    check(UPackage::SavePackage(
        P, nullptr, *FPackageName::LongPackageNameToFilename(P->GetName(), FPackageName::GetAssetPackageExtension()),
        A));
    Saved.Add(P);
    UE_LOG(LogTemp, Display, TEXT("[CombatPolish] Saved %s"), *P->GetName());
}
void Compile(UBlueprint *B)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);
    FCompilerResultsLog L;
    FKismetEditorUtilities::CompileBlueprint(B, EBlueprintCompileOptions::SkipGarbageCollection, &L);
    checkf(L.NumErrors == 0, TEXT("Compile failed: %s"), *B->GetName());
}
template <class T> T *Component(UBlueprint *B, FName Name)
{
    for (auto *N : B->SimpleConstructionScript->GetAllNodes())
        if (N->GetVariableName() == Name)
            return CastChecked<T>(N->ComponentTemplate);
    auto *N = B->SimpleConstructionScript->CreateNode(T::StaticClass(), Name);
    B->SimpleConstructionScript->AddNode(N);
    return CastChecked<T>(N->ComponentTemplate);
}
FProperty *Field(const UStruct *S, const TCHAR *Prefix)
{
    for (TFieldIterator<FProperty> I(S); I; ++I)
        if (I->GetName() == Prefix || I->GetName().StartsWith(FString(Prefix) + TEXT("_")))
            return *I;
    checkf(false, TEXT("Missing field %s in %s"), Prefix, *S->GetName());
    return nullptr;
}
void Num(const UStruct *S, void *Data, const TCHAR *Name, double Value)
{
    auto *P = CastFieldChecked<FNumericProperty>(Field(S, Name));
    auto *V = P->ContainerPtrToValuePtr<void>(Data);
    if (P->IsInteger())
        P->SetIntPropertyValue(V, int64(Value));
    else
        P->SetFloatingPointPropertyValue(V, Value);
}
void Obj(const UStruct *S, void *Data, const TCHAR *Name, UObject *Value)
{
    auto *P = CastFieldChecked<FObjectPropertyBase>(Field(S, Name));
    P->SetObjectPropertyValue_InContainer(Data, Value);
}
void Label(const UStruct *S, void *Data, const TCHAR *Name, const FString &Value)
{
    auto *P = Field(S, Name);
    auto *V = P->ContainerPtrToValuePtr<void>(Data);
    if (auto *T = CastField<FTextProperty>(P))
        T->SetPropertyValue(V, FText::FromString(Value));
    else if (auto *N = CastField<FNameProperty>(P))
        N->SetPropertyValue(V, FName(Value));
    else
        CastFieldChecked<FStrProperty>(P)->SetPropertyValue(V, Value);
}
void *Struct(const UStruct *S, void *Data, const TCHAR *Name, UScriptStruct *&Type)
{
    auto *P = CastFieldChecked<FStructProperty>(Field(S, Name));
    Type = P->Struct;
    return P->ContainerPtrToValuePtr<void>(Data);
}
void Effect(UAnimMontage *M, float Time, const FString &Path, float Seconds, FName Socket = NAME_None,
            bool Variants = false)
{
    for (const auto &N : M->Notifies)
        if (N.NotifyName == TEXT("VTG_AttackEffect"))
            return;
    auto *N = NewObject<UVTGAttackEffectNotify>(M, NAME_None, RF_Transactional);
    N->Effect = Load<UNiagaraSystem>(Path);
    N->ActiveSeconds = Seconds;
    N->SocketName = Socket;
    if (Variants)
    {
        N->MediumEffect = Load<UNiagaraSystem>(TEXT("/Game/VFX/VFX_EnemyAttack/VFX_PoisonCloud/NS_PoisonCloud_Mid"));
        N->LargeEffect = Load<UNiagaraSystem>(TEXT("/Game/VFX/VFX_EnemyAttack/VFX_PoisonCloud/NS_PoisonCloud_Large"));
    }
    auto &Event = M->Notifies.AddDefaulted_GetRef();
    Event.Notify = N;
    Event.NotifyName = TEXT("VTG_AttackEffect");
    Event.Link(M, Time);
    Event.TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetAfter);
    Event.Guid = FGuid::NewGuid();
    M->SortNotifies();
    M->PostEditChange();
    Save(M);
}
FString Wave(const TCHAR *Name)
{
    return FString(TEXT("/Game/SFX/MonsterNCreatures/BST09_BEASTIARIUM_VENOMBRUTE/"
                        "BST09_BEASTIARIUM_VENOMBRUTE_DESIGNED_KIT/PS_DK_BST09_BEASTIARIUM__")) +
           Name;
}
void Sound(const TCHAR *Cue, const TArray<FString> &Waves, float Pitch = 1.f)
{
    auto *C = Load<USoundCue>(Root + TEXT("Audio/") + Cue);
    int Index = 0;
    for (USoundNode *N : C->AllNodes)
        if (auto *W = Cast<USoundNodeWavePlayer>(N))
        {
            W->SetSoundWave(Load<USoundWave>(Waves[Index++ % Waves.Num()]));
        }
    check(Index > 0);
    C->PitchMultiplier = Pitch;
    C->PostEditChange();
    Save(C);
}
UMaterial *Caustics()
{
    const FString Path = TEXT("/Game/Widget/BossHealthBar/M_BossHealth_Caustics");
    if (auto *Existing = LoadObject<UMaterial>(nullptr, *Path))
        return Existing;
    auto *M = NewObject<UMaterial>(CreatePackage(*Path), TEXT("M_BossHealth_Caustics"), RF_Public | RF_Standalone);
    M->MaterialDomain = MD_UI;
    M->BlendMode = BLEND_Translucent;
    auto *UV = NewObject<UMaterialExpressionTextureCoordinate>(M);
    auto *Time = NewObject<UMaterialExpressionTime>(M);
    auto *Tex = NewObject<UMaterialExpressionTextureObject>(M);
    Tex->Texture =
        Load<UTexture2D>(TEXT("/Game/EnvArt/Water/Advanced_Water_Material/effects/Caustics/caustics_01_texture"));
    auto *C = NewObject<UMaterialExpressionCustom>(M);
    C->OutputType = CMOT_Float3;
    C->Code = TEXT("float a=Texture2DSample(Tex,TexSampler,UV*float2(6,1)+float2(T*.04,T*.018)).r; float "
                   "b=Texture2DSample(Tex,TexSampler,UV*float2(5,-1)+float2(-T*.025,T*.01)).r; return "
                   "float3(.25,.035,.055)+pow(saturate(a*b*2),2)*float3(.12,.45,.35);");
    C->Inputs.Empty();
    for (const auto &Pair :
         TArray<TPair<FName, UMaterialExpression *>>{{TEXT("UV"), UV}, {TEXT("T"), Time}, {TEXT("Tex"), Tex}})
    {
        auto &In = C->Inputs.AddDefaulted_GetRef();
        In.InputName = Pair.Key;
        In.Input.Connect(0, Pair.Value);
    }
    auto *Opacity = NewObject<UMaterialExpressionConstant>(M);
    Opacity->R = 1;
    for (auto *E : TArray<UMaterialExpression *>{UV, Time, Tex, C, Opacity})
        M->GetExpressionCollection().AddExpression(E);
    M->GetEditorOnlyData()->EmissiveColor.Connect(0, C);
    M->GetEditorOnlyData()->Opacity.Connect(0, Opacity);
    M->PostEditChange();
    FAssetRegistryModule::AssetCreated(M);
    Save(M);
    return M;
}
} // namespace VTGPolish
UVTGCombatPolishCommandlet::UVTGCombatPolishCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}
int32 UVTGCombatPolishCommandlet::Main(const FString &Params)
{
    using namespace VTGPolish;
    FString PreviewFile;
    if (FParse::Value(*Params, TEXT("Preview="), PreviewFile))
        return VTGRenderCombatHUD(PreviewFile);
    if(Params.Contains(TEXT("PolishUI")))
    {
        auto* B=Load<UWidgetBlueprint>(TEXT("/Game/Widget/BossHealthBar/WBP_BossHealthBar"));
        auto* Name=CastChecked<UTextBlock>(B->WidgetTree->FindWidget(TEXT("BossNameText")));
        auto Font=Name->GetFont();Font.FontObject=Load<UObject>(TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Regular_Font"));Font.TypefaceFontName=TEXT("Default");Name->SetFont(Font);
        auto* HP=CastChecked<UProgressBar>(B->WidgetTree->FindWidget(TEXT("HealthBar")));
        HP->SetBarFillStyle(EProgressBarFillStyle::Scale);
        Compile(B);Save(B);return 0;
    }
    auto *Player = Load<UBlueprint>(TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));
    auto *Egg = Load<UBlueprint>(Root + TEXT("BP_SeaMonster_Egg"));
    auto *Boss = Load<UBlueprint>(Root + TEXT("BP_Boss_SeaMonster"));
    if (Params.Contains(TEXT("VerifyOnly")))
    {
        for (auto *B : {Player, Egg, Boss})
        {
            FCompilerResultsLog L;
            FKismetEditorUtilities::CompileBlueprint(B, EBlueprintCompileOptions::SkipGarbageCollection, &L);
            if (L.NumErrors)
                return 1;
        }
        check(Player->SimpleConstructionScript->FindSCSNode(TEXT("CombatPresentation")));
        check(Egg->SimpleConstructionScript->FindSCSNode(TEXT("CombatFeedback")));
        auto *Table = Load<UDataTable>(Root + TEXT("DT_SeaMonster_Phases"));
        auto *P = CastFieldChecked<FArrayProperty>(Field(Table->GetRowStruct(), TEXT("Abilities")));
        FScriptArrayHelper A(P, P->ContainerPtrToValuePtr<void>(Table->FindRowUnchecked(TEXT("SeaMonster_P1"))));
        check(A.Num() == 4);
        if (Params.Contains(TEXT("Test")) && !VTGTestCombatPolish(Player, Boss))
            return 2;
        UE_LOG(LogTemp, Display,
               TEXT("[CombatPolish] Verification passed: player/enemy blueprints compile; four boss abilities."));
        return 0;
    }
    auto *UI = Component<UVTGPlayerPresentationComponent>(Player, TEXT("CombatPresentation"));
    UI->HeartFull = Load<UTexture2D>(TEXT("/Game/2DArt/UI/Action/UI_SaHealth_Filled"));
    UI->HeartEmpty = Load<UTexture2D>(TEXT("/Game/2DArt/UI/Action/UI_SaHealth_Empty"));
    UI->HeartHurt = Load<UTexture2D>(TEXT("/Game/2DArt/UI/Action/UI_SaHealth_Hurt"));
    for (auto &Var : Player->NewVariables)
        if (Var.VarName == TEXT("Action Cam_Target Arm Length"))
            Var.DefaultValue = TEXT("240.0");
    Compile(Player);
    Num(Player->GeneratedClass, Player->GeneratedClass->GetDefaultObject(), TEXT("Action Cam_Target Arm Length"), 240);
    Save(Player);
    for (auto *B : {Egg, Boss})
    {
        auto *F = Component<UVTGEnemyFeedbackComponent>(B, TEXT("CombatFeedback"));
        F->DisplayName = FText::FromString(B == Egg ? TEXT("眼囊 (Blight Eye)") : TEXT("目孵巢 (Eyed Broodnest)"));
        F->HitEffect = Load<UNiagaraSystem>(TEXT("/Game/Resource/VFX/BloodPack/Hit/Niagara/NS_BulletHit_Cyan_Med1"));
        if (B == Egg)
            F->DeathEffect =
                Load<UNiagaraSystem>(TEXT("/Game/Resource/VFX/BloodPack/BrainBurst/Niagara/NS_BrainBurst_Cyan"));
        if (B == Boss)
            Component<UVTGBossHealthBarComponent>(B, TEXT("BossHealthBar"))->DisplayName = F->DisplayName;
        Compile(B);
        Save(B);
    }
    auto *Bar = Load<UWidgetBlueprint>(TEXT("/Game/Widget/BossHealthBar/WBP_BossHealthBar"));
    auto *HP = CastChecked<UProgressBar>(Bar->WidgetTree->FindWidget(TEXT("HealthBar")));
    FProgressBarStyle Style = HP->GetWidgetStyle();
    Style.FillImage.SetResourceObject(Caustics());
    Style.FillImage.DrawAs = ESlateBrushDrawType::Image;
    Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
    HP->SetWidgetStyle(Style);
    HP->SetFillColorAndOpacity(FLinearColor::White);
    Compile(Bar);
    Save(Bar);
    // Keep existing spatial attenuation and randomization in each cue; replace its wave sources.
    Sound(TEXT("SC_EggMother_Roar"), {Wave(TEXT("attack_long_01")), Wave(TEXT("attack_long_02"))});
    Sound(TEXT("SC_EggMother_Swing"), {Wave(TEXT("attack_short_01"))});
    Sound(TEXT("SC_EggMother_Slam"), {Wave(TEXT("attack_medium_02"))});
    Sound(TEXT("SC_EggMother_Gas"), {Wave(TEXT("breath_long_01"))});
    Sound(TEXT("SC_EggMother_Hurt"), {Wave(TEXT("hit_01")), Wave(TEXT("hit_02"))});
    Sound(TEXT("SC_EggMother_Death"), {Wave(TEXT("death_01"))});
    Sound(TEXT("SC_EggMother_EggDrop"), {Wave(TEXT("other_01"))});
    Sound(TEXT("SC_SeaEgg_Gas"), {Wave(TEXT("attack_short_03"))}, 1.2f);
    Sound(TEXT("SC_SeaEgg_Hurt"), {Wave(TEXT("hit_04"))}, 1.2f);
    Sound(TEXT("SC_SeaEgg_Death"), {Wave(TEXT("death_03"))}, 1.2f);
    auto *EggGas = Load<UAnimMontage>(Root + TEXT("Montages/AM_SeaMonsterEgg_ReleaseGas"));
    auto *BossGas = Load<UAnimMontage>(Root + TEXT("Montages/AM_EggProducer_ToxicGas"));
    Effect(EggGas, .8f, TEXT("/Game/VFX/VFX_EnemyAttack/VFX_PoisonCloud/NS_PoisonCloudSmall"), 1.7f, NAME_None, true);
    Effect(BossGas, .6f, TEXT("/Game/VFX/VFX_EnemyAttack/VFX_PoisonCloud/NS_PoisonCloud_Large"), 2.5f);
    // A separate projectile and montage leave the existing toolkit assets and gas attack intact.
    auto &Assets = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    const FString ProjectilePath = Root + TEXT("BP_SeaMonster_AcidProjectile");
    auto *Projectile = LoadObject<UBlueprint>(nullptr, *ProjectilePath);
    if (!Projectile)
        Projectile = CastChecked<UBlueprint>(
            Assets.DuplicateAsset(TEXT("BP_SeaMonster_AcidProjectile"), Root.LeftChop(1),
                                  Load<UBlueprint>(TEXT("/Game/AI/BossAIToolkit/Blueprints/Misc/BP_Projectile_Base"))));
    for (auto *N : Projectile->SimpleConstructionScript->GetAllNodes())
    {
        if (auto *Mesh = Cast<UStaticMeshComponent>(N->ComponentTemplate))
        {
            Mesh->SetStaticMesh(Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere")));
            Mesh->SetRelativeScale3D(FVector(.24f));
        }
        if (auto *Move = Cast<UProjectileMovementComponent>(N->ComponentTemplate))
        {
            Move->InitialSpeed = 850;
            Move->MaxSpeed = 850;
            Move->ProjectileGravityScale = .12f;
        }
    }
    auto *FX = Component<UNiagaraComponent>(Projectile, TEXT("AcidVisual"));
    FX->SetAsset(Load<UNiagaraSystem>(TEXT("/Game/VFX/VFX_EnemyAttack/VFX_AcidSpit/NS_AcidSpit_Burst")));
    FX->SetAutoActivate(true);
    Compile(Projectile);
    Num(Projectile->GeneratedClass, Projectile->GeneratedClass->GetDefaultObject(), TEXT("InitialLifeSpan"), 5);
    Save(Projectile);
    auto *Spit = LoadObject<UAnimMontage>(nullptr, *(Root + TEXT("Montages/AM_EggProducer_AcidSpit")));
    if (!Spit)
        Spit = CastChecked<UAnimMontage>(
            Assets.DuplicateAsset(TEXT("AM_EggProducer_AcidSpit"), Root + TEXT("Montages"),
                                  Load<UAnimMontage>(Root + TEXT("Montages/AM_EggProducer_Headbutt"))));
    Effect(Spit, .7f, TEXT("/Game/VFX/VFX_EnemyAttack/VFX_AcidSpit/NS_AcidSpit_Burst"), .6f, TEXT("spine_006"));
    auto *Table = Load<UDataTable>(Root + TEXT("DT_SeaMonster_Phases"));
    auto *RS = Table->GetRowStruct();
    for (FName Row : {FName(TEXT("SeaMonster_P1")), FName(TEXT("SeaMonsterEgg_P1"))})
    {
        auto *Data = Table->FindRowUnchecked(Row);
        check(Data);
        const bool IsBoss = Row == TEXT("SeaMonster_P1");
        Label(RS, Data, TEXT("PhaseName"), IsBoss ? TEXT("目孵巢 (Eyed Broodnest)") : TEXT("眼囊 (Blight Eye)"));
        auto *AP = CastFieldChecked<FArrayProperty>(Field(RS, TEXT("Abilities")));
        UScriptStruct *AS = CastFieldChecked<FStructProperty>(AP->Inner)->Struct;
        FScriptArrayHelper A(AP, AP->ContainerPtrToValuePtr<void>(Data));
        // Replace only the corresponding Cascade gas visual; original assets remain on disk.
        UScriptStruct *VS = nullptr;
        void *V = Struct(AS, A.GetRawPtr(IsBoss ? 1 : 0), TEXT("Visuals"), VS);
        Obj(VS, V, TEXT("ParticleEffects"), nullptr);
        if (IsBoss && A.Num() == 3)
        {
            int New = A.AddValue();
            AS->CopyScriptStruct(A.GetRawPtr(New), A.GetRawPtr(0));
            void *Ability = A.GetRawPtr(New);
            Label(AS, Ability, TEXT("AbilityName"), TEXT("AcidSpit"));
            Obj(AS, Ability, TEXT("AbilityClass"),
                Load<UBlueprint>(TEXT("/Game/AI/BossAIToolkit/Blueprints/Abilities/Damage/BP_Ability_Projectile"))
                    ->GeneratedClass);
            Label(AS, Ability, TEXT("ComponentTag"), TEXT("spine_006"));
            Num(AS, Ability, TEXT("Duration"), 3);
            Num(AS, Ability, TEXT("Cooldown"), 7);
            auto *Modifier = CastFieldChecked<FStructProperty>(Field(AS, TEXT("Modifier")));
            *Modifier->ContainerPtrToValuePtr<FVector>(Ability) = FVector(1, 0, 0);
            UScriptStruct *TS = nullptr;
            void *Target = Struct(AS, Ability, TEXT("Targeting"), TS);
            Num(TS, Target, TEXT("MinDistance"), 450);
            Num(TS, Target, TEXT("MaxDistance"), 1800);
            UScriptStruct *DS = nullptr;
            void *Damage = Struct(AS, Ability, TEXT("Damage"), DS);
            Num(DS, Damage, TEXT("DamageMultiplier"), .75);
            V = Struct(AS, Ability, TEXT("Visuals"), VS);
            Obj(VS, V, TEXT("AnimMontage"), Spit);
            Obj(VS, V, TEXT("SpawnClass"), Projectile->GeneratedClass);
            Obj(VS, V, TEXT("ParticleEffects"), nullptr);
        }
    }
    Save(Table);
    UE_LOG(LogTemp, Display, TEXT("[CombatPolish] Completed. %d packages saved."), Saved.Num());
    return 0;
}
