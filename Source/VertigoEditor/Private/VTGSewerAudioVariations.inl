#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeRandom.h"
#include "EdGraph/EdGraphNode.h"

namespace VTGSewerAudioVariations
{
int32 Run(bool VerifyOnly)
{
    using namespace VTGPolish;
    struct FEntry { const TCHAR* Cue; const TCHAR* Family; int32 A, B, C; };
    const FEntry Entries[] = {
        {TEXT("SC_EggMother_Roar"), TEXT("attack_long"), 3, 1, 2},
        {TEXT("SC_EggMother_Swing"), TEXT("attack_medium"), 3, 1, 2},
        {TEXT("SC_EggMother_Slam"), TEXT("attack_medium"), 4, 2, 5},
        {TEXT("SC_EggMother_Gas"), TEXT("breath_long"), 2, 1, 3},
        {TEXT("SC_EggMother_AcidSpit"), TEXT("attack_long"), 4, 3, 5},
        {TEXT("SC_EggMother_Hurt"), TEXT("hit"), 3, 1, 2},
        {TEXT("SC_EggMother_Death"), TEXT("death"), 1, 2, 4},
        {TEXT("SC_EggMother_EggDrop"), TEXT("other"), 1, 2, 3},
        {TEXT("SC_SeaEgg_Gas"), TEXT("attack_short"), 4, 3, 5},
        {TEXT("SC_SeaEgg_Hurt"), TEXT("hit"), 5, 4, 3},
        {TEXT("SC_SeaEgg_Death"), TEXT("death"), 3, 4, 5}
    };
    int32 Failures = 0;
    for (const auto& Entry : Entries)
    {
        auto* Cue = Load<USoundCue>(Root + TEXT("Audio/used/") + Entry.Cue);
        TArray<USoundWave*> Waves;
        for (int32 Index : {Entry.A, Entry.B, Entry.C})
            Waves.Add(Load<USoundWave>(Wave(*FString::Printf(TEXT("%s_%02d"), Entry.Family, Index))));
        if (!VerifyOnly)
        {
            // Preserve the authored overall pitch, volume, attenuation and concurrency.
            // Fold the previous modulator's midpoint into the new subtle variation.
            float Center = 1.f;
            for (USoundNode* Node : Cue->AllNodes)
                if (auto* Mod = Cast<USoundNodeModulator>(Node))
                    Center *= (Mod->PitchMin + Mod->PitchMax) * .5f;
            const FString File = FPackageName::LongPackageNameToFilename(Cue->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
            const FString Backup = FPaths::ProjectDir() / TEXT("Scripts/SewerAudioVariations/Backup") / FPaths::GetCleanFilename(File);
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
            if (!IFileManager::Get().FileExists(*Backup))
                check(IFileManager::Get().Copy(*Backup, *File) == COPY_OK);
            Cue->ResetGraph();
            auto* Mod = Cue->ConstructSoundNode<USoundNodeModulator>();
            Mod->PitchMin = Center * .96f;
            Mod->PitchMax = Center * 1.04f;
            Mod->VolumeMin = Mod->VolumeMax = 1.f;
            auto* Random = Cue->ConstructSoundNode<USoundNodeRandom>();
            Random->bRandomizeWithoutReplacement = true;
            Random->PreselectAtLevelLoad = 0;
            Random->bShouldExcludeFromBranchCulling = true;
            TArray<USoundNode*> Children;
            for (int32 I = 0; I < Waves.Num(); ++I)
            {
                auto* Player = Cue->ConstructSoundNode<USoundNodeWavePlayer>();
                Player->SetSoundWave(Waves[I]);
                Player->bLooping = false;
                Player->GraphNode->NodePosX = -650;
                Player->GraphNode->NodePosY = I * 160;
                Children.Add(Player);
            }
            Random->SetChildNodes(Children);
            Random->Weights.Init(1.f, Waves.Num());
            TArray<USoundNode*> ModChildren{Random};
            Mod->SetChildNodes(ModChildren);
            Random->GraphNode->ReconstructNode();
            Mod->GraphNode->ReconstructNode();
            Random->GraphNode->NodePosX = -400;
            Mod->GraphNode->NodePosX = -200;
            Cue->FirstNode = Mod;
            Cue->LinkGraphNodesFromSoundNodes();
            Cue->PostEditChange();
            Save(Cue);
        }
        auto* Mod = Cast<USoundNodeModulator>(Cue->FirstNode);
        auto* Random = Mod && Mod->ChildNodes.Num() == 1 ? Cast<USoundNodeRandom>(Mod->ChildNodes[0]) : nullptr;
        bool OK = Random && Random->ChildNodes.Num() == 3 && Random->bRandomizeWithoutReplacement && Random->PreselectAtLevelLoad == 0;
        OK &= Mod && Mod->PitchMin > 0 && FMath::IsNearlyEqual(Mod->PitchMax / Mod->PitchMin, 1.04f / .96f, .0001f);
        if (Random)
            for (int32 I = 0; I < Random->ChildNodes.Num(); ++I)
            {
                auto* Player = Cast<USoundNodeWavePlayer>(Random->ChildNodes[I]);
                OK &= Player && Waves.IsValidIndex(I) && Player->GetSoundWave() == Waves[I] && !Player->bLooping;
            }
        Failures += !OK;
        UE_LOG(LogTemp, Display, TEXT("AUDIO VARIATIONS %s %s: three sources, subtle pitch"), OK ? TEXT("PASS") : TEXT("FAIL"), Entry.Cue);
    }
    return Failures ? 1 : 0;
}
}
