#include "UI/VTGQuestSequenceBox.h"
#include "UI/VTGSequenceSettings.h"
#include "UI/VTGSequenceDialogue.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Widgets/Layout/SBox.h"

bool UVTGQuestSequenceBox::GetSequenceDefault(ULevelSequence* Sequence)
{
    const auto* Settings=Sequence?Cast<UVTGSequenceSettings>(Sequence->GetAssetUserDataOfClass(UVTGSequenceSettings::StaticClass())):nullptr;
    return !Settings || Settings->bHideQuestUI;
}
void UVTGQuestSequenceBox::SetSequenceDefault(ULevelSequence* Sequence,bool Hide)
{
    if(!Sequence)return;Sequence->Modify();
    auto* Settings=Cast<UVTGSequenceSettings>(Sequence->GetAssetUserDataOfClass(UVTGSequenceSettings::StaticClass()));
    if(!Settings){Settings=NewObject<UVTGSequenceSettings>(Sequence,NAME_None,RF_Transactional);Sequence->AddAssetUserData(Settings);}
    Settings->Modify();Settings->bHideQuestUI=Hide;Sequence->MarkPackageDirty();
}
bool UVTGQuestSequenceBox::IsHiddenForWorld(const UWorld* World)
{
    if(!World || (World->WorldType!=EWorldType::Game && World->WorldType!=EWorldType::PIE))return false;
    for(TActorIterator<ALevelSequenceActor> It(World);It;++It)
        if(IsValid(*It) && GetSequenceDefault(It->GetSequence()))
            if(const auto* Player=It->GetSequencePlayer())
            {
                const auto* Gate=It->FindComponentByClass<UVTGSequenceDialogueGate>();
                // Pause At End emits OnFinished while retaining a paused last frame.
                if(Player->IsPlaying() || (Player->IsPaused() && (!Gate || Gate->IsPlaybackActive())))return true;
            }
    return false;
}
TSharedRef<SWidget> UVTGQuestSequenceBox::RebuildWidget()
{
    const TWeakObjectPtr<UVTGQuestSequenceBox> Weak(this);
    const auto Content=Super::RebuildWidget();
    // UWidget visibility/animations target the OUTER box. The inner gate cannot be
    // overwritten by quest notifications, and does not write any UMG state to restore.
    return SNew(SBox)
        [SNew(SBox).Tag(TEXT("VTG.QuestSequenceGate")).Visibility_Lambda([Weak](){return Weak.IsValid() && IsHiddenForWorld(Weak->GetWorld())?EVisibility::Hidden:EVisibility::Visible;})
            [Content]];
}
