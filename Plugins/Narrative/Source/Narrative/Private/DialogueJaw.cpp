#include "Dialogue.h"
#include "NarrativeJawComponent.h"
#include "NarrativeDialogueSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"

void UDialogue::StopJawMovement()
{
    if (ActiveJawComponent.IsValid()) ActiveJawComponent->Stop(this);
    ActiveJawComponent.Reset();
}

void UDialogue::StartJawMovement(UDialogueNode* Node, const FDialogueLine& Line, const FSpeakerInfo& Speaker)
{
    StopJawMovement();
    if (!GetWorld() || !bEnableJawMovement || !Node || !Node->bEnableJawMovement || Speaker.SpeakingActorTag.IsNone()) return;
    const int32 Characters = UNarrativeJawComponent::CountVisibleCharacters(Line.Text.ToString());
    if (!Characters || !FMath::IsFinite(JawDurationMultiplier) || JawDurationMultiplier <= 0) return;
    AActor* Target = nullptr;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (!IsValid(*It)) continue;
        // Narrative montage events use tags on the character's skeletal mesh (SaP / SaC).
        // Resolve those to their owner, while retaining support for actor-level tags.
        bool Matches = It->ActorHasTag(Speaker.SpeakingActorTag);
        if (!Matches)
        {
            TInlineComponentArray<USkeletalMeshComponent*> Meshes;
            It->GetComponents(Meshes);
            for (auto* Mesh : Meshes)
                if (IsValid(Mesh) && Mesh->ComponentHasTag(Speaker.SpeakingActorTag)) { Matches = true; break; }
        }
        if (!Matches) continue;
        if (Target)
        {
            UE_LOG(LogTemp, Warning, TEXT("Narrative jaw: tag '%s' matches multiple actors; no jaw binding for speaker '%s'."), *Speaker.SpeakingActorTag.ToString(), *Speaker.SpeakerID.ToString());
            return;
        }
        Target = *It;
    }
    if (!Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("Narrative jaw: no actor or skeletal mesh component with tag '%s' for speaker '%s'."), *Speaker.SpeakingActorTag.ToString(), *Speaker.SpeakerID.ToString());
        return;
    }
    const auto* Settings = GetDefault<UNarrativeDialogueSettings>();
    float Multiplier = Settings->ReadingTimeMultiplier;
    const auto Language = FInternationalization::Get().GetCurrentLanguage();
    if (const float* M = Settings->ReadingTimeMultiplierPerLanguage.Find(Language->GetName())) Multiplier = *M;
    else if (const float* ShortMultiplier = Settings->ReadingTimeMultiplierPerLanguage.Find(Language->GetTwoLetterISOLanguageName())) Multiplier = *ShortMultiplier;
    const float Duration = FMath::Max(Characters / FMath::Max(1.f, Settings->LettersPerSecondLineDuration), Settings->MinDialogueTextDisplayTime)
        * Multiplier * DefaultLineReadingTimeMultiplier * JawDurationMultiplier;
    if (!FMath::IsFinite(Duration) || Duration <= 0) return;
    auto* State = Target->FindComponentByClass<UNarrativeJawComponent>();
    if (!State)
    {
        State = NewObject<UNarrativeJawComponent>(Target);
        Target->AddInstanceComponent(State);
        State->RegisterComponent();
    }
    State->Start(this, Duration);
    ActiveJawComponent = State;
}
