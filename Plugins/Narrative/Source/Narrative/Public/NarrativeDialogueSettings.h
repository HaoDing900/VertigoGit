// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeDialogueSettings.generated.h"

/**
 * Runtime dialogue settings for narrative 
 */
UCLASS(config = Engine, defaultconfig)
class NARRATIVE_API UNarrativeDialogueSettings : public UObject
{
	GENERATED_BODY()
	
public:

	UNarrativeDialogueSettings();

	//Optional buffer of silence added to the end of dialogue lines
	UPROPERTY(EditAnywhere, config, Category = "Dialogue Settings", meta = (ClampMin = 0.01))
	float DialogueLineAudioSilence; 

	//How long should the text be displayed for at a minimum? Since default letters per minute is 25 this prevents a reply like "no" from being played too quickly
	UPROPERTY(EditAnywhere, config, Category = "Dialogue Settings", meta = (ClampMin=0.01))
	float MinDialogueTextDisplayTime;

	//If a dialogue doesn't have audio supplied, how long should the text be displayed on the screen for? Lower letters per minute means player gets more time
	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category = "Dialogue Settings", meta = (ClampMin = 1))
	float LettersPerSecondLineDuration;

	//Final multiplier applied to the auto "After Reading Time" duration. Set to 3-4 to make lines linger much longer
	//(useful for languages like Chinese where one character is a whole word). 1 = no change.
	//Used as the fallback when the current language isn't listed in ReadingTimeMultiplierPerLanguage below.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category = "Dialogue Settings", meta = (ClampMin = 0.1))
	float ReadingTimeMultiplier;

	//Per-language overrides for the reading-time multiplier. Key can be either a full culture code (e.g. "zh-Hans",
	//"zh-Hant", "en-US") or a two-letter ISO language code (e.g. "zh", "ja", "en"). The active language is matched
	//against the full culture first, then the two-letter code; the first hit wins, otherwise ReadingTimeMultiplier
	//is used. Example: add "zh-Hant" -> 5, "zh" -> 4, "en" -> 1.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category = "Dialogue Settings")
	TMap<FString, float> ReadingTimeMultiplierPerLanguage;

	//If there is only one player response available, should we autoselect it, regardless of whether bAutoSelect is checked?
	UPROPERTY(EditAnywhere, config, Category = "Dialogue Settings")
	bool bAutoSelectSingleResponse;

	//Expiremental - won't autoarrange old dialogues, and you'll need to move your nodes into the correct position yourself. 
	//Also makes dialogue nodes sort themselves from left to right instead of top to bottom

	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category = "Graph Options")
	bool bEnableVerticalWiring;

	//Default speaker colors
	UPROPERTY(EditAnywhere, config, Category = "Graph Style")
	TArray<FLinearColor> SpeakerColors;

	// --- Avatar Display (BP_Narrative3Overlay only) ---
	// These caps only affect the avatar images displayed inside BP_Narrative3Overlay. The source textures are never
	// modified - the avatar is downscaled into a render target of this size for display only, so the texture's native
	// size (LOD 0, e.g. 1024) is preserved everywhere else.

	//Max render-target resolution (pixels, square) for the MAIN avatar (AvtProfile_Main) in BP_Narrative3Overlay.
	//For a 1024 source this is the "LOD 1" cap. Default 512.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category = "Avatar Display", meta = (ClampMin = 1))
	int32 AvatarMainMaxResolution;

	//Max render-target resolution (pixels, square) for the SIDE avatar (AvtProfile_Side) in BP_Narrative3Overlay.
	//For a 1024 source this is the "LOD 2" cap. Default 256.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, config, Category = "Avatar Display", meta = (ClampMin = 1))
	int32 AvatarSideMaxResolution;
};
