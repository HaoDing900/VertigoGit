#include "VTGSewerBossIntroCommandlet.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Dialogue.h"
#include "NarrativeComponent.h"
#include "UObject/UnrealType.h"

namespace VTGSewerBossIntro
{
const TCHAR* ManagerPath = TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt.BPLM_SewerUnderApt");
const TCHAR* Dialogues[] =
{
    TEXT("/Game/Narrative/Dialogues/Levels/L3Sewer/L3Sewer_Still.L3Sewer_Still"),
    TEXT("/Game/Narrative/Dialogues/Levels/L3Sewer/L3Sewer_Freemove.L3Sewer_Freemove"),
};

int32 Check()
{
    auto* Blueprint = LoadObject<UBlueprint>(nullptr, ManagerPath);
    if (!Blueprint || !Blueprint->GeneratedClass) { UE_LOG(LogTemp, Error, TEXT("BOSS INTRO: BPLM_SewerUnderApt did not load")); return 1; }
    UClass* Manager = Blueprint->GeneratedClass;
    int32 Failures = 0;
    auto Expect = [&](bool bPass, const TCHAR* Label)
    {
        UE_LOG(LogTemp, Display, TEXT("BOSS INTRO %s: %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), Label);
        Failures += !bPass;
    };

    // StartBossEncounter reads this property by name to clear an ambient comment before the intro line starts.
    auto* NarrativeProperty = CastField<FObjectPropertyBase>(Manager->FindPropertyByName(TEXT("Narrative")));
    Expect(NarrativeProperty && NarrativeProperty->PropertyClass
        && NarrativeProperty->PropertyClass->IsChildOf(UNarrativeComponent::StaticClass()),
        TEXT("BPLM exposes a Narrative component property for the pre-intro dialogue exit"));

    // The retry replays this authored event, so the fallback depends on the name still existing.
    Expect(Manager->FindFunctionByName(TEXT("L3BossEncounter")) != nullptr,
        TEXT("BPLM still exposes L3BossEncounter for the deferred retry"));

    for (const TCHAR* Path : Dialogues)
    {
        auto* DialogueBlueprint = LoadObject<UBlueprint>(nullptr, Path);
        auto* Dialogue = DialogueBlueprint && DialogueBlueprint->GeneratedClass
            ? Cast<UDialogue>(DialogueBlueprint->GeneratedClass->GetDefaultObject()) : nullptr;
        if (!Dialogue) { UE_LOG(LogTemp, Error, TEXT("BOSS INTRO: %s did not load"), Path); ++Failures; continue; }
        // Exitable dialogues are cleared immediately; the rest fall through to the retry after they end.
        UE_LOG(LogTemp, Display, TEXT("BOSS INTRO path: %s bCanBeExited=%d -> %s"),
            *DialogueBlueprint->GetName(), Dialogue->bCanBeExited,
            Dialogue->bCanBeExited ? TEXT("exited before the intro starts") : TEXT("deferred retry after it ends"));
    }
    // RestoreEncounterCheckpointScene no longer repositions the barrier, so the placed actor has to already sit where the beat expects it.
    const FVector Authored(431.340695, 12980.735886, -749.559192);
    if (auto* Map = LoadObject<UWorld>(nullptr, TEXT("/Game/ThirdPerson/Maps/L_SewerUnderApartment")))
    {
        AActor* Fence = nullptr;
        for (AActor* Actor : Map->PersistentLevel->Actors)
            if (Actor && Actor->GetClass()->GetName().StartsWith(TEXT("BP_ConstructionFence_Round"))) Fence = Actor;
        if (!Fence) { UE_LOG(LogTemp, Error, TEXT("BOSS INTRO: no round construction fence in the level")); ++Failures; }
        else
        {
            // A map loaded outside a running world has unregistered components, so read the serialized transform.
            const FVector Placed = Fence->GetRootComponent() ? Fence->GetRootComponent()->GetRelativeLocation() : FVector::ZeroVector;
            UE_LOG(LogTemp, Display, TEXT("BOSS INTRO fence placed=%s authored=%s delta=%.2f"),
                *Placed.ToCompactString(), *Authored.ToCompactString(), (Placed - Authored).Size());
            Expect(Placed.Equals(Authored, 1.), TEXT("level places the entrance barrier where the sewer beat expects it"));
        }
    }
    UE_LOG(LogTemp, Display, TEXT("BOSS INTRO failures=%d"), Failures);
    return Failures == 0 ? 0 : 2;
}
}

UVTGSewerBossIntroCommandlet::UVTGSewerBossIntroCommandlet() { IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true; }

int32 UVTGSewerBossIntroCommandlet::Main(const FString&) { return VTGSewerBossIntro::Check(); }
