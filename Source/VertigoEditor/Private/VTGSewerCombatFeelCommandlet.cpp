#include "VTGSewerCombatFeelCommandlet.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Engine/StaticMesh.h"
#include "Combat/VTGCombatPresentation.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SimpleConstructionScript.h"
#include "Components/SceneComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NiagaraSystem.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Algo/Find.h"
#include "Combat/VTGCombatStatics.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Select.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"

namespace SewerFeel
{
const TCHAR* Root = TEXT("/Game/Characters/Monster/SeaMonsterBoss/");
const TCHAR* GotHit = TEXT("/Script/Engine.AnimMontage'/Game/Characters/Monster/SeaMonsterBoss/Montages/"
                           "AM_EggProducer_GotHit.AM_EggProducer_GotHit'");
const TCHAR* Splash = TEXT("/Game/VFX/VFX_EnemyAttack/VFX_AcidSpit/NS_AcidSplash_Med1");
/** Where the headbutt's damage window opens; the splash has to read at the same instant, not before it. */
const float HeadbuttImpact = .85f;
const float GotHitRate = 1.6f;

/** User-defined struct fields carry a GUID suffix, so match on the authored name and its prefix form. */
FProperty* Field(const UStruct* Struct, const FString& Name)
{
    for (TFieldIterator<FProperty> It(Struct); It; ++It)
        if (It->GetName() == Name || It->GetName().StartsWith(Name + TEXT("_")))
            return *It;
    checkf(false, TEXT("missing field %s"), *Name);
    return nullptr;
}
FString Text(FProperty* Property, const void* Data)
{
    FString Out;
    Property->ExportTextItem_Direct(Out, Data, nullptr, nullptr, PPF_None);
    return Out;
}
void Backup(UObject* Object)
{
    const FString File = FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),
                                                                 FPackageName::GetAssetPackageExtension());
    const FString To = FPaths::ProjectDir() / TEXT("Scripts/SewerCombatFeel/Backup") / FPaths::GetCleanFilename(File);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(To), true);
    if (!IFileManager::Get().FileExists(*To))
        check(IFileManager::Get().Copy(*To, *File) == COPY_OK);
}
void Save(UObject* Object)
{
    UPackage* Package = Object->GetOutermost();
    Package->FullyLoad();
    ResetLoaders(Package);
    Package->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    check(UPackage::SavePackage(
        Package, Object, *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()),
        Args));
}

UAnimMontage* Montage(const TCHAR* Name)
{
    auto* Found = LoadObject<UAnimMontage>(nullptr, *(FString(Root) + TEXT("Montages/") + Name));
    check(Found);
    return Found;
}

/** The only notify the splash can be: our own effect notify carrying the acid splash system. */
FAnimNotifyEvent* FindSplash(UAnimMontage* Headbutt)
{
    for (FAnimNotifyEvent& Event : Headbutt->Notifies)
        if (auto* Effect = Cast<UVTGAttackEffectNotify>(Event.Notify))
            if (Effect->Effect && Effect->Effect->GetName().Contains(TEXT("AcidSplash")))
                return &Event;
    return nullptr;
}

int32 Audit(UDataTable* Phases)
{
    UE_LOG(LogTemp, Display, TEXT("FEEL AUDIT phases:\n%s"), *Phases->GetTableAsString());
    UAnimMontage* Headbutt = Montage(TEXT("AM_EggProducer_Headbutt"));
    if (const USkeleton* Skeleton = Headbutt->GetSkeleton())
    {
        const FReferenceSkeleton& Bones = Skeleton->GetReferenceSkeleton();
        for (int32 I = 0; I < Bones.GetNum(); ++I)
        {
            const FString Bone = Bones.GetBoneName(I).ToString();
            if (Bone.Contains(TEXT("head")) || Bone.Contains(TEXT("neck")) || Bone.Contains(TEXT("spine")) ||
                Bone.Contains(TEXT("jaw")))
                UE_LOG(LogTemp, Display, TEXT("FEEL AUDIT bone %s"), *Bone);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("FEEL AUDIT headbutt splash present=%d gotHit rate=%.3f"), FindSplash(Headbutt) != nullptr,
           Montage(TEXT("AM_EggProducer_GotHit"))->RateScale);
    for (const TCHAR* Name : {TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_Boss_SeaMonster_L3Sewer"),
                              TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_Boss_SeaMonster")})
        if (auto* Blueprint = LoadObject<UBlueprint>(nullptr, Name))
        {
            if (auto* Default = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()))
                UE_LOG(LogTemp, Display, TEXT("FEEL AUDIT %s scale=%s"), *Blueprint->GetName(),
                       *Default->GetActorScale3D().ToString());
            // The headbutt hitbox is found by component tag, so the tag has to exist on a real component.
            for (UClass* Class = Blueprint->GeneratedClass; Class; Class = Class->GetSuperClass())
                if (auto* Generated = Cast<UBlueprintGeneratedClass>(Class);
                    Generated && Generated->SimpleConstructionScript)
                    for (USCS_Node* Node : Generated->SimpleConstructionScript->GetAllNodes())
                        if (auto* Template = Cast<USceneComponent>(Node->ComponentTemplate))
                            UE_LOG(LogTemp, Display, TEXT("FEEL AUDIT %s component %s (%s) tags=[%s] socket=%s"),
                                   *Class->GetName(), *Node->GetVariableName().ToString(),
                                   *Template->GetClass()->GetName(), *FString::JoinBy(Template->ComponentTags, TEXT(","), [](const FName& Tag){ return Tag.ToString(); }),
                                   *Node->AttachToName.ToString());
        }
    return 0;
}

int32 Assets(UDataTable* Phases, bool bApply)
{
    uint8* Row = Phases->FindRowUnchecked(TEXT("SeaMonster_P1"));
    check(Row);
    FProperty* Reacts = Field(Phases->GetRowStruct(), TEXT("HitReacts"));
    auto* Abilities = CastFieldChecked<FArrayProperty>(Field(Phases->GetRowStruct(), TEXT("Abilities")));
    UScriptStruct* AbilityType = CastFieldChecked<FStructProperty>(Abilities->Inner)->Struct;
    FProperty* AbilityName = Field(AbilityType, TEXT("AbilityName"));
    FScriptArrayHelper List(Abilities, Abilities->ContainerPtrToValuePtr<void>(Row));

    auto NameOf = [AbilityName](const void* Data)
    { return Text(AbilityName, AbilityName->ContainerPtrToValuePtr<void>(const_cast<void*>(Data))).TrimQuotes(); };
    int32 HeadbuttIndex = INDEX_NONE, AcidIndex = INDEX_NONE;
    for (int32 I = 0; I < List.Num(); ++I)
    {
        const FString Label = NameOf(List.GetRawPtr(I));
        UE_LOG(LogTemp, Display, TEXT("FEEL ability %d = %s"), I, *Label);
        if (Label == TEXT("Headbutt"))
            HeadbuttIndex = I;
        if (Label == TEXT("AcidSpit"))
            AcidIndex = I;
    }
    check(AcidIndex != INDEX_NONE);

    const FString ReactsText = FString::Printf(
        TEXT("(HeadShot=None,HeadBone=\"\",FrontHit=\"%s\",BackHit=\"%s\",LeftHit=\"%s\",RightHit=\"%s\",BlockHit=None)"),
        GotHit, GotHit, GotHit, GotHit);

    if (bApply)
    {
        Backup(Phases);
        // The hit reacts and the headbutt were authored here before; this restores them, it does not invent them.
        void* ReactsData = Reacts->ContainerPtrToValuePtr<void>(Row);
        check(Reacts->ImportText_Direct(*ReactsText, ReactsData, nullptr, PPF_None, nullptr));
        if (HeadbuttIndex == INDEX_NONE)
        {
            // Copy a live melee-damage entry so every field the toolkit reads is already shaped correctly.
            HeadbuttIndex = List.AddValue();
            AbilityType->CopyScriptStruct(List.GetRawPtr(HeadbuttIndex), List.GetRawPtr(AcidIndex));
        }
        void* Headbutt = List.GetRawPtr(HeadbuttIndex);
        auto Set = [AbilityType, Headbutt](const FString& Path, const TCHAR* Value)
        {
            FString Remaining = Path;
            void* Data = Headbutt;
            UStruct* Owner = AbilityType;
            FString Leaf;
            while (Remaining.Split(TEXT("."), &Leaf, &Remaining))
            {
                auto* Nested = CastFieldChecked<FStructProperty>(Field(Owner, Leaf));
                Data = Nested->ContainerPtrToValuePtr<void>(Data);
                Owner = Nested->Struct;
            }
            FProperty* Property = Field(Owner, Remaining);
            checkf(Property->ImportText_Direct(Value, Property->ContainerPtrToValuePtr<void>(Data), nullptr, PPF_None, nullptr),
                   TEXT("could not set %s"), *Path);
        };
        Set(TEXT("AbilityName"), TEXT("Headbutt"));
        Set(TEXT("AbilityClass"), TEXT("/Script/Engine.BlueprintGeneratedClass'/Game/AI/BossAIToolkit/Blueprints/Abilities/"
                                       "Damage/BP_Ability_Hitbox.BP_Ability_Hitbox_C'"));
        Set(TEXT("ComponentTag"), TEXT("head"));
        Set(TEXT("Damage.DamageMultiplier"), TEXT("1.500000"));
        Set(TEXT("Targeting.MinDistance"), TEXT("0.000000"));
        // The reach the hitbox can actually cover; the splash below is what makes that reach readable.
        Set(TEXT("Targeting.MaxDistance"), TEXT("420.000000"));
        Set(TEXT("Modifier"), TEXT("(X=1.000000,Y=1.000000,Z=1.000000)"));
        Set(TEXT("Visuals.AnimMontage"), TEXT("/Script/Engine.AnimMontage'/Game/Characters/Monster/SeaMonsterBoss/Montages/"
                                              "AM_EggProducer_Headbutt.AM_EggProducer_Headbutt'"));
        Set(TEXT("Visuals.SpawnClass"), TEXT("None"));
        Set(TEXT("Visuals.EffectDelay"), TEXT("0.000000"));
        Set(TEXT("Duration"), TEXT("2.700000"));
        // Close enough to be a real reason to keep moving, far enough that it never overlaps its own montage.
        Set(TEXT("Cooldown"), TEXT("3.000000"));
        Phases->PostEditChange();
        Save(Phases);
    }

    const FString FinalReacts = Text(Reacts, Reacts->ContainerPtrToValuePtr<void>(Row));
    UE_LOG(LogTemp, Display, TEXT("FEEL hitReacts=%s"), *FinalReacts);
    check(FinalReacts.Contains(TEXT("AM_EggProducer_GotHit")));
    check(HeadbuttIndex != INDEX_NONE && NameOf(List.GetRawPtr(HeadbuttIndex)) == TEXT("Headbutt"));
    const FString HeadbuttText = Text(Abilities->Inner, List.GetRawPtr(HeadbuttIndex));
    UE_LOG(LogTemp, Display, TEXT("FEEL headbutt=%s"), *HeadbuttText);
    // Exported user-defined struct fields keep their GUID suffix, so match the field name and its value apart.
    check(HeadbuttText.Contains(TEXT("BP_Ability_Hitbox")) && HeadbuttText.Contains(TEXT("AM_EggProducer_Headbutt")) &&
          HeadbuttText.Contains(TEXT("=420.000000")) && HeadbuttText.Contains(TEXT("MaxDistance")));

    // A recoil that outlasts the player's combo cadence would spend the fight interrupting itself.
    UAnimMontage* Recoil = Montage(TEXT("AM_EggProducer_GotHit"));
    if (bApply && !FMath::IsNearlyEqual(Recoil->RateScale, GotHitRate))
    {
        Backup(Recoil);
        check(FMath::IsNearlyEqual(Recoil->RateScale, 1.f));
        Recoil->RateScale = GotHitRate;
        Recoil->PostEditChange();
        Save(Recoil);
    }
    check(FMath::IsNearlyEqual(Recoil->RateScale, GotHitRate));
    UE_LOG(LogTemp, Display, TEXT("FEEL gotHit length=%.3fs at rate %.2f"), Recoil->GetPlayLength() / Recoil->RateScale,
           Recoil->RateScale);

    UAnimMontage* Headbutt = Montage(TEXT("AM_EggProducer_Headbutt"));
    if (bApply && !FindSplash(Headbutt))
    {
        Backup(Headbutt);
        auto* System = LoadObject<UNiagaraSystem>(nullptr, Splash);
        check(System);
        auto* Effect = NewObject<UVTGAttackEffectNotify>(Headbutt, NAME_None, RF_Transactional);
        Effect->Effect = System;
        // The skeleton has no "head" bone: spine_006 is its top, and is where HeadHitbox hangs.
        Effect->SocketName = TEXT("spine_006");
        // The boss is anchored and barely turns its root, so the splash takes its direction from the target.
        Effect->bOrientToTarget = true;
        Effect->ActiveSeconds = 1.f;
        Effect->EffectScale = 1.5f;
        FAnimNotifyEvent Event;
        Event.NotifyName = TEXT("VTG_AttackEffect");
        Event.Notify = Effect;
        Event.NotifyStateClass = nullptr;
        Event.Link(Headbutt, HeadbuttImpact);
        Headbutt->Notifies.Add(Event);
        Headbutt->SortNotifies();
        Headbutt->PostEditChange();
        Save(Headbutt);
    }
    const FAnimNotifyEvent* Found = FindSplash(Headbutt);
    check(Found);
    check(FMath::IsNearlyEqual(Found->GetTriggerTime(), HeadbuttImpact, .001f));
    check(CastChecked<UVTGAttackEffectNotify>(Found->Notify)->bOrientToTarget);
    UE_LOG(LogTemp, Display, TEXT("FEEL splash at %.3fs socket=%s"), Found->GetTriggerTime(),
           *CastChecked<UVTGAttackEffectNotify>(Found->Notify)->SocketName.ToString());
    return 0;
}

/** Montages the player feels directly: how long until the swing lands, and when dodging stops hurting. */
const TCHAR* PlayerRoot = TEXT("/Game/Characters/Sa/Anm/");
struct FRate
{
    const TCHAR* Path;
    float Rate;
};
// These two landed at 0.43s and 0.31s at rate 1, well past the 0.1-0.2s this genre reads as responsive.
// 1.45 overshot in play, so both sit at a gentler 1.2: 0.36s and 0.26s.
const FRate HammerRates[] = {{TEXT("Sa_Hammer/Combo/Hammer_Attack_2Combo_1_Root_Montage"), 1.2f},
                             {TEXT("Sa_Hammer/Combo/Hammer_Attack_2Combo_2_Root_Montage"), 1.2f}};
const TCHAR* DodgeMontages[] = {TEXT("Dodge/ANM_Sa_Dodge_F2_Montage"), TEXT("Dodge/ANM_Sa_Dodge_B_Montage"),
                                TEXT("Dodge/ANM_Sa_Dodge_L_Montage"), TEXT("Dodge/ANM_Sa_Dodge_R_Montage")};

int32 PlayerMontages(bool bApply)
{
    for (const FRate& Entry : HammerRates)
    {
        auto* Swing = LoadObject<UAnimMontage>(nullptr, *(FString(PlayerRoot) + Entry.Path));
        check(Swing);
        if (bApply && !FMath::IsNearlyEqual(Swing->RateScale, Entry.Rate))
        {
            Backup(Swing);
            // Either the authored rate or whichever rate an earlier run of this pass left behind.
            check(FMath::IsNearlyEqual(Swing->RateScale, 1.f) || FMath::IsNearlyEqual(Swing->RateScale, 1.45f) ||
                  FMath::IsNearlyEqual(Swing->RateScale, 1.3f));
            Swing->RateScale = Entry.Rate;
            Swing->PostEditChange();
            Save(Swing);
        }
        check(FMath::IsNearlyEqual(Swing->RateScale, Entry.Rate));
        for (const FAnimNotifyEvent& Event : Swing->Notifies)
            if (Event.NotifyStateClass && Event.NotifyStateClass->GetClass()->GetName().StartsWith(TEXT("Notify_Damage")))
                UE_LOG(LogTemp, Display, TEXT("FEEL %s damage lands at %.3fs real (rate %.2f)"), *Swing->GetName(),
                       Event.GetTriggerTime() / Swing->RateScale, Swing->RateScale);
    }

    for (const TCHAR* Path : DodgeMontages)
    {
        auto* Dodge = LoadObject<UAnimMontage>(nullptr, *(FString(PlayerRoot) + Path));
        check(Dodge);
        int32 Windows = 0;
        for (FAnimNotifyEvent& Event : Dodge->Notifies)
        {
            UAnimNotifyState* State = Event.NotifyStateClass;
            const bool bBroken = State && State->GetClass()->GetName().StartsWith(TEXT("Notify_Sa_DodgeInvincible"));
            if (!bBroken && !Cast<UVTGDodgeInvincibleNotify>(State))
                continue;
            ++Windows;
            if (bApply && bBroken)
            {
                Backup(Dodge);
                // Same window, a notify that actually grants i-frames on Sa and leaves mesh collision alone.
                Event.NotifyStateClass = NewObject<UVTGDodgeInvincibleNotify>(Dodge, NAME_None, RF_Transactional);
                Event.NotifyName = TEXT("Dodge I-Frames");
            }
        }
        check(Windows == 1);
        if (bApply)
        {
            Dodge->PostEditChange();
            Save(Dodge);
        }
        for (const FAnimNotifyEvent& Event : Dodge->Notifies)
            if (auto* Window = Cast<UVTGDodgeInvincibleNotify>(Event.NotifyStateClass))
                UE_LOG(LogTemp, Display, TEXT("FEEL %s i-frames %.3f -> %.3f"), *Dodge->GetName(), Event.GetTriggerTime(),
                       Event.GetTriggerTime() + Event.GetDuration());
        check(Dodge->Notifies.ContainsByPredicate(
            [](const FAnimNotifyEvent& Event) { return Cast<UVTGDodgeInvincibleNotify>(Event.NotifyStateClass) != nullptr; }));
    }
    return 0;
}
// =================================================================================================
//  Sewer difficulty: egg health and boss damage
// =================================================================================================

/**
 * Absolute targets, not multipliers, so re-running this pass cannot compound them.
 * Boss damage is x1.5 of the values this file set earlier today; Spawn Eggs carries no damage of its
 * own and the egg phase row is deliberately untouched, so spawned eggs keep matching the placed ones.
 */
const TCHAR* EggBlueprint = TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_Egg");
/** One number for every egg: the class default spawned eggs use and the per-instance override on each
    placed one. An absolute target, so re-running this pass is a no-op rather than another multiply. */
const float EggHealth = 100.f;
const int32 PlacedEggCount = 9;
struct FDamage
{
    const TCHAR* Ability;
    float Multiplier;
};
const FDamage BossDamage[] = {{TEXT("Toxic Gas"), 1.2f}, {TEXT("AcidSpit"), 1.125f}, {TEXT("Headbutt"), 2.25f}};

float Number(UObject* Object, const TCHAR* Name)
{
    FProperty* Property = Field(Object->GetClass(), Name);
    return float(CastFieldChecked<FNumericProperty>(Property)->GetFloatingPointPropertyValue(
        Property->ContainerPtrToValuePtr<void>(Object)));
}
void SetNumber(UObject* Object, const TCHAR* Name, float Value)
{
    FProperty* Property = Field(Object->GetClass(), Name);
    CastFieldChecked<FNumericProperty>(Property)->SetFloatingPointPropertyValue(
        Property->ContainerPtrToValuePtr<void>(Object), Value);
}
/** The node that declares the behavior component, and the class it is declared on. */
USCS_Node* BossBehaviorNode(UBlueprint* Blueprint, UClass*& OutOwner)
{
    check(Blueprint);
    // The component may be declared on a parent Blueprint, so walk the whole class chain.
    for (UClass* Class = Blueprint->GeneratedClass; Class; Class = Class->GetSuperClass())
        if (auto* Generated = Cast<UBlueprintGeneratedClass>(Class); Generated && Generated->SimpleConstructionScript)
            for (USCS_Node* Node : Generated->SimpleConstructionScript->GetAllNodes())
                if (Node->ComponentTemplate &&
                    Node->ComponentTemplate->GetClass()->GetName().StartsWith(TEXT("BPC_Boss_Behavior")))
                {
                    OutOwner = Class;
                    return Node;
                }
    OutOwner = nullptr;
    return nullptr;
}

/**
 * What Spawn Eggs hands its summons. The behavior component is declared on BP_Boss_Base, which the
 * sea monster itself also inherits, so its 2500 cannot simply be rewritten: that is the boss's own
 * health. The egg gets a child override instead - exactly what selecting an inherited component in
 * the child Blueprint and typing a new value produces - and BP_Boss_Base is left alone.
 */
int32 EggClassDefault(bool bApply, bool bAuditOnly)
{
    auto* Blueprint = LoadObject<UBlueprint>(nullptr, EggBlueprint);
    check(Blueprint);
    UClass* Owner = nullptr;
    USCS_Node* Node = BossBehaviorNode(Blueprint, Owner);
    check(Node && Owner);
    const bool bInherited = Owner != Blueprint->GeneratedClass;
    UE_LOG(LogTemp, Display, TEXT("FEEL egg behavior declared on %s (inherited=%d) health=%.3f max=%.3f"), *Owner->GetName(),
           bInherited, Number(Node->ComponentTemplate, TEXT("Health")), Number(Node->ComponentTemplate, TEXT("MaxHealth")));
    if (bAuditOnly)
        return 0;

    auto Effective = [Blueprint, Node, bInherited]() -> UActorComponent*
    {
        if (!bInherited)
            return Node->ComponentTemplate;
        UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(/*bCreateIfNecessary=*/false);
        UActorComponent* Override = Handler ? Handler->GetOverridenComponentTemplate(FComponentKey(Node)) : nullptr;
        return Override ? Override : Node->ComponentTemplate;
    };

    if (bApply && !FMath::IsNearlyEqual(Number(Effective(), TEXT("MaxHealth")), EggHealth))
    {
        Backup(Blueprint);
        UActorComponent* Target = Node->ComponentTemplate;
        if (bInherited)
        {
            UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(/*bCreateIfNecessary=*/true);
            check(Handler);
            const FComponentKey Key(Node);
            Target = Handler->GetOverridenComponentTemplate(Key);
            if (!Target)
                Target = Handler->CreateOverridenComponentTemplate(Key);
            check(Target && Target != Node->ComponentTemplate);
        }
        Target->Modify();
        SetNumber(Target, TEXT("MaxHealth"), EggHealth);
        SetNumber(Target, TEXT("Health"), EggHealth);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Log);
        if (Log.NumErrors)
            return 30;
        Save(Blueprint);
    }
    UE_LOG(LogTemp, Display, TEXT("FEEL egg class default now health=%.3f max=%.3f (BP_Boss_Base still %.3f)"),
           Number(Effective(), TEXT("Health")), Number(Effective(), TEXT("MaxHealth")),
           Number(Node->ComponentTemplate, TEXT("MaxHealth")));
    check(FMath::IsNearlyEqual(Number(Effective(), TEXT("MaxHealth")), EggHealth) &&
          FMath::IsNearlyEqual(Number(Effective(), TEXT("Health")), EggHealth));
    // Touching the shared parent would have set the sea monster's own health to the egg's.
    check(!bInherited || !FMath::IsNearlyEqual(Number(Node->ComponentTemplate, TEXT("MaxHealth")), EggHealth));
    return 0;
}

/**
 * Every egg in the sewer overrides the class default on its own instance, so the health that matters
 * is written per placed actor and the level is what has to be saved.
 */
int32 PlacedEggs(bool bApply, bool bAuditOnly)
{
    auto* Level = LoadObject<UWorld>(nullptr, TEXT("/Game/ThirdPerson/Maps/L_SewerUnderApartment"));
    check(Level && Level->PersistentLevel);
    const float Target = EggHealth;
    int32 Count = 0;
    bool bChanged = false;
    for (AActor* Actor : Level->PersistentLevel->Actors)
    {
        if (!Actor || !Actor->GetClass()->GetName().StartsWith(TEXT("BP_SeaMonster_Egg")))
            continue;
        UActorComponent* Behavior = nullptr;
        for (UActorComponent* Component : Actor->GetComponents())
            if (Component->GetClass()->GetName().StartsWith(TEXT("BPC_Boss_Behavior")))
                Behavior = Component;
        if (!Behavior)
            continue;
        ++Count;
        const float Max = Number(Behavior, TEXT("MaxHealth"));
        if (!bAuditOnly && bApply && !FMath::IsNearlyEqual(Max, Target))
        {
            // 60 was authored, 90 was this pass before the target became one flat number.
            checkf(FMath::IsNearlyEqual(Max, 60.f) || FMath::IsNearlyEqual(Max, 90.f),
                   TEXT("%s is at %.3f, which is neither value this pass knows how to replace"), *Actor->GetName(), Max);
            Behavior->Modify();
            SetNumber(Behavior, TEXT("MaxHealth"), Target);
            SetNumber(Behavior, TEXT("Health"), Target);
            bChanged = true;
        }
        UE_LOG(LogTemp, Display, TEXT("FEEL placed egg %s health=%.3f max=%.3f"), *Actor->GetName(),
               Number(Behavior, TEXT("Health")), Number(Behavior, TEXT("MaxHealth")));
        if (!bAuditOnly)
            check(FMath::IsNearlyEqual(Number(Behavior, TEXT("MaxHealth")), Target));
    }
    UE_LOG(LogTemp, Display, TEXT("FEEL placed eggs=%d target=%.1f"), Count, Target);
    if (bAuditOnly)
        return 0;
    check(Count == PlacedEggCount);
    if (!bChanged)
        return 0;
    UPackage* Package = Level->GetOutermost();
    const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetMapPackageExtension());
    const FString To = FPaths::ProjectDir() / TEXT("Scripts/SewerCombatFeel/Backup") / FPaths::GetCleanFilename(File);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(To), true);
    if (!IFileManager::Get().FileExists(*To))
        check(IFileManager::Get().Copy(*To, *File) == COPY_OK);
    Package->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    check(UPackage::SavePackage(Package, Level, *File, Args));
    UE_LOG(LogTemp, Display, TEXT("FEEL saved %s"), *File);
    return 0;
}

int32 BossDamageScale(bool bApply, UDataTable* Phases)
{
    uint8* Row = Phases->FindRowUnchecked(TEXT("SeaMonster_P1"));
    check(Row);
    auto* Abilities = CastFieldChecked<FArrayProperty>(Field(Phases->GetRowStruct(), TEXT("Abilities")));
    UScriptStruct* AbilityType = CastFieldChecked<FStructProperty>(Abilities->Inner)->Struct;
    FProperty* NameField = Field(AbilityType, TEXT("AbilityName"));
    auto* DamageField = CastFieldChecked<FStructProperty>(Field(AbilityType, TEXT("Damage")));
    FProperty* MultiplierField = Field(DamageField->Struct, TEXT("DamageMultiplier"));
    FScriptArrayHelper List(Abilities, Abilities->ContainerPtrToValuePtr<void>(Row));
    int32 Matched = 0;
    for (int32 I = 0; I < List.Num(); ++I)
    {
        void* Data = List.GetRawPtr(I);
        const FString Label = Text(NameField, NameField->ContainerPtrToValuePtr<void>(Data)).TrimQuotes();
        const FDamage* Wanted = Algo::FindByPredicate(BossDamage, [&Label](const FDamage& Entry) { return Label == Entry.Ability; });
        if (!Wanted)
            continue;
        ++Matched;
        void* Damage = DamageField->ContainerPtrToValuePtr<void>(Data);
        auto* Multiplier = CastFieldChecked<FNumericProperty>(MultiplierField);
        void* Value = MultiplierField->ContainerPtrToValuePtr<void>(Damage);
        if (bApply)
            Multiplier->SetFloatingPointPropertyValue(Value, Wanted->Multiplier);
        const float Now = float(Multiplier->GetFloatingPointPropertyValue(Value));
        UE_LOG(LogTemp, Display, TEXT("FEEL boss damage %s multiplier=%.4f"), *Label, Now);
        check(FMath::IsNearlyEqual(Now, Wanted->Multiplier, .0001f));
    }
    check(Matched == UE_ARRAY_COUNT(BossDamage));
    if (bApply)
    {
        Phases->PostEditChange();
        Save(Phases);
    }
    return 0;
}

// =================================================================================================
//  BP_Player_Sa: what happens on the frame a hit lands, and what a dodge is worth
// =================================================================================================

const TCHAR* PlayerBlueprint = TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa");
const TCHAR* ShakeClass = TEXT("/Game/OtherMaterials/Vefects/Stylized_Galaxy_Shader/Demo/Shared/Cam/"
                               "Sudden_Cam_Shake_Slight.Sudden_Cam_Shake_Slight_C");
/** The first hit of every combo used to pass 0 here, so the hit the player feels most had no weight at all. */
const TCHAR* FirstHitStop = TEXT("0.070000");
/** What BP_Hammer itself puts in the player hand; WeaponDataMap has to agree with it. */
const TCHAR* HammerMeshPath = TEXT("/Game/Weapon/Hammer/Hammer");

UEdGraphPin* Pin(UEdGraphNode* Node, const TCHAR* Name)
{
    for (UEdGraphPin* Candidate : Node->Pins)
        if (Candidate->PinName == FName(Name))
            return Candidate;
    checkf(false, TEXT("no pin %s on %s"), Name, *Node->GetName());
    return nullptr;
}
/** Identify nodes by the function they call, never by the node name the editor hands out. */
template <typename TNode> TNode* Only(UEdGraph* Graph, TFunctionRef<bool(TNode*)> Matches)
{
    TNode* Found = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (auto* Candidate = Cast<TNode>(Node); Candidate && Matches(Candidate))
        {
            checkf(!Found, TEXT("more than one node matches"));
            Found = Candidate;
        }
    return Found;
}
template <typename TNode> TNode* First(UEdGraph* Graph, TFunctionRef<bool(TNode*)> Matches)
{
    for (UEdGraphNode* Node : Graph->Nodes)
        if (auto* Candidate = Cast<TNode>(Node); Candidate && Matches(Candidate))
            return Candidate;
    return nullptr;
}
/** Both damage events share a shape: the event, then a story-mode gate whose else branch carries the hit. */
UEdGraphPin* AfterStoryGate(UEdGraph* Graph, const TCHAR* Event)
{
    auto* Entry = Only<UK2Node_Event>(
        Graph, [Event](UK2Node_Event* Node) { return Node->EventReference.GetMemberName() == FName(Event); });
    check(Entry);
    auto* Story = CastChecked<UK2Node_IfThenElse>(Pin(Entry, TEXT("then"))->LinkedTo[0]->GetOwningNode());
    UEdGraphPin* Else = Pin(Story, TEXT("else"));
    check(!Else->LinkedTo.IsEmpty());
    return Else;
}
UK2Node_IfThenElse* GateOn(UEdGraphPin* Exec)
{
    return Exec->LinkedTo.IsEmpty() ? nullptr : Cast<UK2Node_IfThenElse>(Exec->LinkedTo[0]->GetOwningNode());
}
UK2Node_CallFunction* Calls(UEdGraph* Graph, const TCHAR* Member)
{
    return Only<UK2Node_CallFunction>(
        Graph, [Member](UK2Node_CallFunction* Node) { return Node->FunctionReference.GetMemberName() == FName(Member); });
}
template <typename TNode> TNode* Add(UEdGraph* Graph, UEdGraphNode* Beside, int32 Column, int32 Row)
{
    auto* Node = NewObject<TNode>(Graph);
    Graph->AddNode(Node, false, false);
    Node->CreateNewGuid();
    Node->PostPlacedNewNode();
    Node->NodePosX = Beside->NodePosX + Column;
    Node->NodePosY = Beside->NodePosY + Row;
    return Node;
}
UK2Node_CallFunction* AddCall(UEdGraph* Graph, UFunction* Function, UEdGraphNode* Beside, int32 Column, int32 Row)
{
    auto* Node = Add<UK2Node_CallFunction>(Graph, Beside, Column, Row);
    Node->SetFromFunction(Function);
    Node->AllocateDefaultPins();
    return Node;
}
/** DefaultToSelf would resolve this at compile time; wiring it leaves nothing to infer from the graph. */
void WireSelf(UEdGraph* Graph, UK2Node_CallFunction* Node, const TCHAR* Name)
{
    UEdGraphPin* Target = Pin(Node, Name);
    if (!Target->LinkedTo.IsEmpty())
        return;
    auto* Self = Add<UK2Node_Self>(Graph, Node, -200, 80);
    Self->AllocateDefaultPins();
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Self->Pins[0], Target));
}

int32 Player(bool bApply)
{
    auto* Blueprint = LoadObject<UBlueprint>(nullptr, PlayerBlueprint);
    check(Blueprint);
    UEdGraph* Graph = nullptr;
    for (UEdGraph* Page : Blueprint->UbergraphPages)
        if (Page->GetName() == TEXT("EventGraph"))
            Graph = Page;
    check(Graph);
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    UFunction* Feedback = UVTGCombatStatics::StaticClass()->FindFunctionByName(TEXT("PlayMeleeHitFeedback"));
    UFunction* Invincible = UVTGCombatStatics::StaticClass()->FindFunctionByName(TEXT("IsDodgeInvincible"));
    check(Feedback && Invincible);
    auto* Shake = LoadObject<UClass>(nullptr, ShakeClass);
    check(Shake);

    // ---- the frame a punch connects -------------------------------------------------------------
    UK2Node_CallFunction* Targets = Calls(Graph, TEXT("FindMeleeTargets"));
    UK2Node_CallFunction* Landed = Calls(Graph, TEXT("PlayMeleeHitFeedback"));
    UK2Node_CallFunction* OldStop = Calls(Graph, TEXT("HitStop"));
    check(Targets && (Landed || OldStop));
    UEdGraphPin* Duration = (Landed ? Landed : OldStop)->FindPinChecked(Landed ? TEXT("HitStopSeconds") : TEXT("Duration"));
    auto* Choice = Cast<UK2Node_Select>(Duration->LinkedTo.IsEmpty() ? nullptr : Duration->LinkedTo[0]->GetOwningNode());
    check(Choice);
    UEdGraphPin* FirstOption = Pin(Choice, TEXT("Option 0"));
    if (bApply && FirstOption->DefaultValue != FirstHitStop)
    {
        check(FirstOption->DefaultValue == TEXT("0.0") || FirstOption->DefaultValue.IsEmpty());
        FirstOption->DefaultValue = FirstHitStop;
    }
    if (bApply && !Landed)
    {
        // One node now owns the whole impact: freeze the attacker, freeze what it hit, shake the camera.
        UEdGraphPin* Incoming = Pin(OldStop, TEXT("execute"))->LinkedTo[0];
        Landed = AddCall(Graph, Feedback, OldStop, 0, 0);
        Landed->NodePosX = OldStop->NodePosX;
        Landed->NodePosY = OldStop->NodePosY;
        check(Schema->TryCreateConnection(Incoming, Pin(Landed, TEXT("execute"))));
        check(Schema->TryCreateConnection(Pin(Targets, TEXT("ReturnValue")), Pin(Landed, TEXT("Targets"))));
        check(Schema->TryCreateConnection(Duration->LinkedTo[0], Pin(Landed, TEXT("HitStopSeconds"))));
        Pin(Landed, TEXT("CameraShake"))->DefaultObject = Shake;
        WireSelf(Graph, Landed, TEXT("Attacker"));
        Graph->RemoveNode(OldStop);
    }
    check(Landed);
    UE_LOG(LogTemp, Display, TEXT("FEEL hit feedback: firstHitStop=%s targets=%d shake=%s"), *FirstOption->DefaultValue,
           Pin(Landed, TEXT("Targets"))->LinkedTo.Num(), *GetNameSafe(Pin(Landed, TEXT("CameraShake"))->DefaultObject));
    check(FirstOption->DefaultValue == FirstHitStop);
    check(Pin(Landed, TEXT("Targets"))->LinkedTo.Num() == 1 && Pin(Landed, TEXT("CameraShake"))->DefaultObject == Shake);

    // ---- what a dodge is worth --------------------------------------------------------------------
    UK2Node_CallFunction* SlowMo = First<UK2Node_CallFunction>(
        Graph, [](UK2Node_CallFunction* Node) { return Node->FunctionReference.GetMemberName() == FName("PerfectDodge_SlowMo"); });
    UK2Node_IfThenElse* PointGate = GateOn(AfterStoryGate(Graph, TEXT("ReceivePointDamage")));
    // Before this pass the radial path ran straight into Process Damage, with no gate of its own at all.
    UEdGraphPin* RadialElse = AfterStoryGate(Graph, TEXT("ReceiveRadialDamage"));
    check(SlowMo && PointGate);

    if (bApply)
    {
        UEdGraphPin* Condition = Pin(PointGate, TEXT("Condition"));
        if (Condition->LinkedTo.IsEmpty() ||
            !Cast<UK2Node_CallFunction>(Condition->LinkedTo[0]->GetOwningNode()))
        {
            // The montage-long IsDodging? made the sloppy tail of the dodge safer than its tight window.
            UEdGraphNode* Stale = Condition->LinkedTo.IsEmpty() ? nullptr : Condition->LinkedTo[0]->GetOwningNode();
            Schema->BreakPinLinks(*Condition, true);
            auto* PointReads = AddCall(Graph, Invincible, PointGate, -340, 40);
            WireSelf(Graph, PointReads, TEXT("Actor"));
            check(Schema->TryCreateConnection(Pin(PointReads, TEXT("ReturnValue")), Condition));
            if (Stale && Stale->Pins.ContainsByPredicate([](UEdGraphPin* P) { return !P->LinkedTo.IsEmpty(); }) == false)
                Graph->RemoveNode(Stale);
        }
        if (!GateOn(RadialElse))
        {
            // Radial damage never asked whether the player was dodging, so the gas could not be dodged at all.
            UEdGraphPin* Damage = RadialElse->LinkedTo[0];
            auto* Gate = Add<UK2Node_IfThenElse>(Graph, RadialElse->GetOwningNode(), 300, 160);
            Gate->AllocateDefaultPins();
            Schema->BreakPinLinks(*RadialElse, true);
            check(Schema->TryCreateConnection(RadialElse, Pin(Gate, TEXT("execute"))));
            auto* RadialReads = AddCall(Graph, Invincible, Gate, -340, 40);
            WireSelf(Graph, RadialReads, TEXT("Actor"));
            check(Schema->TryCreateConnection(Pin(RadialReads, TEXT("ReturnValue")), Pin(Gate, TEXT("Condition"))));
            check(Schema->TryCreateConnection(
                Pin(Gate, TEXT("then")),
                Pin(AddCall(Graph, SlowMo->GetTargetFunction(), Gate, 300, -60), TEXT("execute"))));
            check(Schema->TryCreateConnection(Pin(Gate, TEXT("else")), Damage));
        }
    }
    auto Reads = [Schema](UK2Node_IfThenElse* Gate)
    {
        UEdGraphPin* Condition = Pin(Gate, TEXT("Condition"));
        auto* Source = Condition->LinkedTo.IsEmpty() ? nullptr : Cast<UK2Node_CallFunction>(Condition->LinkedTo[0]->GetOwningNode());
        return Source && Source->FunctionReference.GetMemberName() == FName("IsDodgeInvincible");
    };
    UK2Node_IfThenElse* RadialGate = GateOn(RadialElse);
    UE_LOG(LogTemp, Display, TEXT("FEEL dodge gates: point=%d radial=%d"), Reads(PointGate), RadialGate && Reads(RadialGate));
    check(Reads(PointGate) && RadialGate && Reads(RadialGate));

    // ---- the hammer the player is holding after a retry ------------------------------------------
    // BP_Hammer sets MeleeWeapon's mesh to the authored hammer when it is picked up, but EquipMeleeWeapon
    // re-reads WeaponDataMap, which still pointed at the sample-content prop. Any path that re-equips -
    // a checkpoint retry above all - swapped the hammer back to the old model.
    UObject* Defaults = Blueprint->GeneratedClass->GetDefaultObject();
    auto* Weapons = CastFieldChecked<FMapProperty>(Field(Defaults->GetClass(), TEXT("WeaponDataMap")));
    FProperty* Model = Field(CastFieldChecked<FStructProperty>(Weapons->ValueProp)->Struct, TEXT("WeaponModel"));
    auto* ModelObject = CastFieldChecked<FObjectPropertyBase>(Model);
    FScriptMapHelper Entries(Weapons, Weapons->ContainerPtrToValuePtr<void>(Defaults));
    auto* Authored = LoadObject<UStaticMesh>(nullptr, HammerMeshPath);
    check(Authored);
    int32 WithModel = 0;
    for (int32 I = 0; I < Entries.GetMaxIndex(); ++I)
    {
        if (!Entries.IsValidIndex(I))
            continue;
        void* Slot = ModelObject->ContainerPtrToValuePtr<void>(Entries.GetValuePtr(I));
        UObject* Current = ModelObject->GetObjectPropertyValue(Slot);
        if (!Current && !bApply)
            continue;
        if (!Current)
            continue;
        ++WithModel;
        if (bApply && Current != Authored)
        {
            check(Current->GetName() == TEXT("SM_Prop_Hammer") || Current == Authored);
            Defaults->Modify();
            ModelObject->SetObjectPropertyValue(Slot, Authored);
        }
        UE_LOG(LogTemp, Display, TEXT("FEEL weapon model = %s"), *GetPathNameSafe(ModelObject->GetObjectPropertyValue(Slot)));
        check(ModelObject->GetObjectPropertyValue(Slot) == Authored);
    }
    // Only the hammer carries a model; the fists entry leaves it empty.
    check(WithModel == 1);

    if (!bApply)
        return 0;
    // Idempotent: nodes added by an earlier run of this pass get their self pins wired too.
    WireSelf(Graph, Landed, TEXT("Attacker"));
    for (UK2Node_IfThenElse* Gate : {PointGate, RadialGate})
        WireSelf(Graph, CastChecked<UK2Node_CallFunction>(Pin(Gate, TEXT("Condition"))->LinkedTo[0]->GetOwningNode()),
                 TEXT("Actor"));
    Backup(Blueprint);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
    UE_LOG(LogTemp, Display, TEXT("FEEL player compile errors=%d warnings=%d"), Results.NumErrors, Results.NumWarnings);
    if (Results.NumErrors || Blueprint->Status == BS_Error)
        return 20;
    Save(Blueprint);
    return 0;
}
} // namespace SewerFeel

UVTGSewerCombatFeelCommandlet::UVTGSewerCombatFeelCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UVTGSewerCombatFeelCommandlet::Main(const FString& Params)
{
    using namespace SewerFeel;
    auto* Phases = LoadObject<UDataTable>(nullptr, *(FString(Root) + TEXT("DT_SeaMonster_Phases")));
    check(Phases);
    if (FParse::Param(*Params, TEXT("Audit")))
    {
        EggClassDefault(false, true);
        PlacedEggs(false, true);
        return Audit(Phases);
    }
    const bool bApply = FParse::Param(*Params, TEXT("Apply"));
    int32 Result = Assets(Phases, bApply);
    if (Result == 0)
        Result = PlayerMontages(bApply);
    if (Result == 0)
        Result = EggClassDefault(bApply, false);
    if (Result == 0)
        Result = PlacedEggs(bApply, false);
    if (Result == 0)
        Result = BossDamageScale(bApply, Phases);
    if (Result == 0)
        Result = Player(bApply);
    UE_LOG(LogTemp, Display, TEXT("SEWER COMBAT FEEL %s"), Result == 0 ? TEXT("PASS") : TEXT("FAIL"));
    return Result;
}
