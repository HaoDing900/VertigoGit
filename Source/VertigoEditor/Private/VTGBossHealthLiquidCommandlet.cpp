#include "VTGBossHealthLiquidCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

extern int32 VTGRenderCombatHUD(const FString& Output);

namespace VTGBossLiquid
{
const FString Folder = TEXT("/Game/Widget/BossHealthBar");
const FString MaterialName = TEXT("M_BossHealth_Caustics");
const FString InstanceName = TEXT("MI_BossHealth_Caustics");
const FString MaterialPath = Folder / MaterialName;
const FString InstancePath = Folder / InstanceName;
const FString WidgetPath = Folder / TEXT("WBP_BossHealthBar");
const FString TexturePath =
	TEXT("/Game/EnvArt/Water/Advanced_Water_Material/effects/Caustics/caustics_01_texture");

/** Custom-node pin name, Material Instance parameter name, default value, parameter group. */
struct FScalarSpec
{
	const TCHAR* Pin;
	const TCHAR* Parameter;
	float Default;
	const TCHAR* Group;
};

struct FVectorSpec
{
	const TCHAR* Pin;
	const TCHAR* Parameter;
	FLinearColor Default;
	const TCHAR* Group;
};

// One colour per layer, so a layer can be retinted - or switched off through its own intensity and
// opacity below - without disturbing any other.
const FVectorSpec Vectors[] = {
	{TEXT("DeepColor"), TEXT("DeepColor"), FLinearColor(.060f, .004f, .014f, 1.f), TEXT("1 - Color")},
	{TEXT("ShallowColor"), TEXT("ShallowColor"), FLinearColor(.620f, .080f, .110f, 1.f), TEXT("1 - Color")},
	{TEXT("CausticColor"), TEXT("CausticColor"), FLinearColor(.150f, .520f, .460f, 1.f), TEXT("1 - Color")},
	{TEXT("SurfaceColor"), TEXT("SurfaceColor"), FLinearColor(.700f, .920f, 1.f, 1.f), TEXT("1 - Color")},
	{TEXT("RimColor"), TEXT("RimColor"), FLinearColor(.620f, .880f, 1.f, 1.f), TEXT("1 - Color")},
	{TEXT("LeadColor"), TEXT("LeadingEdgeColor"), FLinearColor(.850f, .980f, 1.f, 1.f), TEXT("1 - Color")},
};

// Defaults are tuned for the bar's real shape, 1098 x 12 px: U and V tiling differ by roughly that
// aspect ratio so the caustic cells stay square, and the horizontal rim is far thinner than the
// vertical one for the same reason.
const FScalarSpec Scalars[] = {
	{TEXT("TilingU"), TEXT("CausticTilingU"), 3.f, TEXT("2 - Liquid")},
	{TEXT("TilingV"), TEXT("CausticTilingV"), .04f, TEXT("2 - Liquid")},
	{TEXT("FlowSpeed"), TEXT("FlowSpeed"), 1.f, TEXT("2 - Liquid")},
	{TEXT("Gain"), TEXT("CausticGain"), 1.6f, TEXT("2 - Liquid")},
	{TEXT("Contrast"), TEXT("CausticContrast"), 1.8f, TEXT("2 - Liquid")},
	{TEXT("Refraction"), TEXT("RefractionStrength"), .045f, TEXT("2 - Liquid")},
	{TEXT("DepthFalloff"), TEXT("DepthFalloff"), 1.f, TEXT("2 - Liquid")},
	{TEXT("SurfacePos"), TEXT("SurfacePosition"), .12f, TEXT("3 - Glass")},
	{TEXT("SurfaceWidth"), TEXT("SurfaceWidth"), .12f, TEXT("3 - Glass")},
	{TEXT("SurfaceIntensity"), TEXT("SurfaceIntensity"), .45f, TEXT("3 - Glass")},
	{TEXT("RimWidthY"), TEXT("RimWidthY"), .12f, TEXT("3 - Glass")},
	{TEXT("RimWidthX"), TEXT("RimWidthX"), .004f, TEXT("3 - Glass")},
	{TEXT("RimIntensity"), TEXT("RimIntensity"), .55f, TEXT("3 - Glass")},
	{TEXT("LeadWidth"), TEXT("LeadingEdgeWidth"), .012f, TEXT("3 - Glass")},
	{TEXT("LeadIntensity"), TEXT("LeadingEdgeIntensity"), 1.f, TEXT("3 - Glass")},
	{TEXT("BodyOpacity"), TEXT("BodyOpacity"), .7f, TEXT("4 - Opacity")},
	{TEXT("CausticOpacity"), TEXT("CausticOpacity"), .3f, TEXT("4 - Opacity")},
	{TEXT("SurfaceOpacity"), TEXT("SurfaceOpacity"), .25f, TEXT("4 - Opacity")},
	{TEXT("RimOpacity"), TEXT("RimOpacity"), .92f, TEXT("4 - Opacity")},
	{TEXT("LeadOpacity"), TEXT("LeadingEdgeOpacity"), .95f, TEXT("4 - Opacity")},
	{TEXT("GlobalOpacity"), TEXT("GlobalOpacity"), 1.f, TEXT("4 - Opacity")},
	{TEXT("ColorBlack"), TEXT("ColorBlackPoint"), 0.f, TEXT("5 - Levels")},
	{TEXT("ColorWhite"), TEXT("ColorWhitePoint"), 1.f, TEXT("5 - Levels")},
	{TEXT("ColorGamma"), TEXT("ColorGamma"), 1.f, TEXT("5 - Levels")},
	{TEXT("OpacityBlack"), TEXT("OpacityBlackPoint"), 0.f, TEXT("5 - Levels")},
	{TEXT("OpacityWhite"), TEXT("OpacityWhitePoint"), 1.f, TEXT("5 - Levels")},
	{TEXT("OpacityGamma"), TEXT("OpacityGamma"), 1.f, TEXT("5 - Levels")},
};

const TCHAR* TextureParameter = TEXT("CausticTexture");

/**
 * Two taps of one caustics texture, where the first warps the second: that single dependent read is
 * what reads as refraction through moving liquid. Everything else is UV maths - a depth gradient, a
 * surface band and an edge rim - so the shader stays in the same cost bracket as the flat version it
 * replaces: still two samples on one sampler.
 *
 * Four layers stack in a fixed order (body, caustics, surface band, glass edges). Each carries its
 * own colour, intensity and opacity, and the levels block at the end re-maps colour and alpha the
 * way an image editor would, so the look is tuned from the Material Instance rather than from here.
 */
FString LiquidCode()
{
	const TCHAR* Lines[] = {
		TEXT("float2 uv = saturate(UV);"),
		TEXT("float t = T * FlowSpeed;"),
		TEXT("float a = Texture2DSample(Tex, TexSampler,"),
		TEXT("        float2(uv.x * TilingU + t * 0.040, uv.y * TilingV + t * 0.018)).r;"),
		TEXT("float warp = (a - 0.5) * Refraction;"),
		TEXT("float b = Texture2DSample(Tex, TexSampler,"),
		TEXT("        float2(uv.x * TilingU * 0.8 - t * 0.025 + warp,"),
		TEXT("               uv.y * TilingV * -0.85 + t * 0.010 + warp)).r;"),
		TEXT("float caustic = saturate(pow(saturate(a * b * 2.2), max(Contrast, 0.01)) * Gain);"),
		TEXT(""),
		TEXT("// Body: light enters at the surface and the depth soaks it up."),
		TEXT("float3 body = lerp(ShallowColor, DeepColor, pow(uv.y, max(DepthFalloff, 0.01)));"),
		TEXT(""),
		TEXT("// Surface band just under the top edge - the meniscus that sells it as liquid."),
		TEXT("float band = saturate(1.0 - abs(uv.y - SurfacePos) / max(SurfaceWidth, 0.001));"),
		TEXT("band *= band;"),
		TEXT(""),
		TEXT("// Glass walls. The bar is far wider than it is tall, so X and Y get their own widths."),
		TEXT("float rimY = saturate(1.0 - min(uv.y, 1.0 - uv.y) / max(RimWidthY, 0.001));"),
		TEXT("float rimX = saturate(1.0 - min(uv.x, 1.0 - uv.x) / max(RimWidthX, 0.001));"),
		TEXT("float rim = max(rimY, rimX);"),
		TEXT("rim *= rim;"),
		TEXT("float lead = saturate((uv.x - (1.0 - LeadWidth)) / max(LeadWidth, 0.001));"),
		TEXT("lead *= lead;"),
		TEXT(""),
		TEXT("float3 color = body;"),
		TEXT("color += caustic * CausticColor;"),
		TEXT("color += band * SurfaceIntensity * (0.45 + 0.55 * caustic) * SurfaceColor;"),
		TEXT("color += rim * RimIntensity * RimColor;"),
		TEXT("color += lead * LeadIntensity * LeadColor;"),
		TEXT(""),
		TEXT("// Each layer's own opacity pulls the pixel towards opaque by that layer's coverage, so"),
		TEXT("// zeroing one removes it from the alpha without disturbing the others."),
		TEXT("float alpha = saturate(BodyOpacity);"),
		TEXT("alpha = lerp(alpha, 1.0, saturate(caustic * CausticOpacity));"),
		TEXT("alpha = lerp(alpha, 1.0, saturate(band * SurfaceOpacity));"),
		TEXT("alpha = lerp(alpha, 1.0, saturate(rim * RimOpacity));"),
		TEXT("alpha = lerp(alpha, 1.0, saturate(lead * LeadOpacity));"),
		TEXT(""),
		TEXT("// Levels, as in an image editor: black point, white point and gamma, colour and alpha"),
		TEXT("// apart. Colour keeps values above one so a bloom pass still has something to find."),
		TEXT("color = pow(max((color - ColorBlack) / max(ColorWhite - ColorBlack, 0.0001), 0.0),"),
		TEXT("            max(ColorGamma, 0.01));"),
		TEXT("alpha = pow(saturate((alpha - OpacityBlack) / max(OpacityWhite - OpacityBlack, 0.0001)),"),
		TEXT("            max(OpacityGamma, 0.01));"),
		TEXT("return float4(color, saturate(alpha * GlobalOpacity));"),
	};
	return FString::Join(Lines, TEXT("\n"));
}

template <class T> T* Load(const FString& Path)
{
	auto* O = LoadObject<T>(nullptr, *Path);
	checkf(O, TEXT("Missing %s"), *Path);
	return O;
}

void Backup(UObject* O)
{
	const FString From = FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),
	                                                            FPackageName::GetAssetPackageExtension());
	const FString To =
		FPaths::ProjectDir() / TEXT("Scripts/BossHealthLiquid/Backup") / (O->GetName() + TEXT(".uasset"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(To), true);
	if (IFileManager::Get().FileExists(*From) && !IFileManager::Get().FileExists(*To))
	{
		check(IFileManager::Get().Copy(*To, *From) == COPY_OK);
	}
}

void Save(UObject* O)
{
	UPackage* P = O->GetOutermost();
	P->FullyLoad();
	ResetLoaders(P);
	P->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	check(UPackage::SavePackage(
		P, nullptr,
		*FPackageName::LongPackageNameToFilename(P->GetName(), FPackageName::GetAssetPackageExtension()), Args));
	UE_LOG(LogTemp, Display, TEXT("[BossLiquid] Saved %s"), *P->GetName());
}

template <class T> T* Expression(UMaterial* M, int32 X, int32 Y)
{
	return CastChecked<T>(UMaterialEditingLibrary::CreateMaterialExpression(M, T::StaticClass(), X, Y));
}

UMaterialExpressionComponentMask* Mask(UMaterial* M, UMaterialExpression* Source, bool bAlphaOnly, int32 X,
                                       int32 Y)
{
	auto* Node = Expression<UMaterialExpressionComponentMask>(M, X, Y);
	Node->R = Node->G = Node->B = !bAlphaOnly;
	Node->A = bAlphaOnly;
	Node->Input.Connect(0, Source);
	return Node;
}

UMaterial* BuildMaterial()
{
	UMaterial* M = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (M)
	{
		Backup(M);
	}
	else
	{
		M = NewObject<UMaterial>(CreatePackage(*MaterialPath), *MaterialName, RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(M);
	}
	UMaterialEditingLibrary::DeleteAllMaterialExpressions(M);
	M->MaterialDomain = MD_UI;
	M->BlendMode = BLEND_Translucent;
	M->bUsedWithEditorCompositing = false;

	// Pins feeding the Custom node, in the order they are declared on it.
	TArray<TPair<FName, UMaterialExpression*>> Pins;

	auto* UV = Expression<UMaterialExpressionTextureCoordinate>(M, -1500, -760);
	UV->UTiling = UV->VTiling = 1.f;
	Pins.Emplace(TEXT("UV"), UV);

	auto* Time = Expression<UMaterialExpressionTime>(M, -1500, -680);
	Pins.Emplace(TEXT("T"), Time);

	auto* Tex = Expression<UMaterialExpressionTextureObjectParameter>(M, -1500, -580);
	Tex->ParameterName = TextureParameter;
	Tex->Texture = Load<UTexture2D>(TexturePath);
	Tex->SamplerType = SAMPLERTYPE_Color;
	Tex->Group = TEXT("1 - Color");
	Pins.Emplace(TEXT("Tex"), Tex);

	int32 Y = -440;
	int32 Priority = 0;
	for (const FVectorSpec& Spec : Vectors)
	{
		auto* Param = Expression<UMaterialExpressionVectorParameter>(M, -1500, Y);
		Param->ParameterName = Spec.Parameter;
		Param->DefaultValue = Spec.Default;
		Param->Group = Spec.Group;
		Param->SortPriority = Priority++;
		// The Custom node types each input from the output it is fed, so mask RGBA down to float3.
		Pins.Emplace(Spec.Pin, Mask(M, Param, false, -1120, Y));
		Y += 90;
	}
	Priority = 0;
	for (const FScalarSpec& Spec : Scalars)
	{
		auto* Param = Expression<UMaterialExpressionScalarParameter>(M, -1500, Y);
		Param->ParameterName = Spec.Parameter;
		Param->DefaultValue = Spec.Default;
		Param->Group = Spec.Group;
		Param->SortPriority = Priority++;
		Pins.Emplace(Spec.Pin, Param);
		Y += 70;
	}

	auto* Custom = Expression<UMaterialExpressionCustom>(M, -620, -200);
	Custom->Description = TEXT("Liquid Caustics");
	Custom->OutputType = CMOT_Float4;
	Custom->Code = LiquidCode();
	Custom->Inputs.Empty();
	for (const TPair<FName, UMaterialExpression*>& Pin : Pins)
	{
		FCustomInput& In = Custom->Inputs.AddDefaulted_GetRef();
		In.InputName = Pin.Key;
		In.Input.Connect(0, Pin.Value);
	}

	// One Custom node drives both outputs; the compiler emits its body once and masks the result.
	check(UMaterialEditingLibrary::ConnectMaterialProperty(Mask(M, Custom, false, -220, -240), TEXT(""),
	                                                       MP_EmissiveColor));
	check(UMaterialEditingLibrary::ConnectMaterialProperty(Mask(M, Custom, true, -220, -140), TEXT(""), MP_Opacity));

	UMaterialEditingLibrary::RecompileMaterial(M);
	// Parameter lookups - ours, the Material Instance's and the editor's - read the material's cached
	// expression data, not the expressions. The recompile's PostEditChange leaves that cache holding
	// what the package loaded with, so rebuild it here, after the recompile and before the save, or
	// every default this pass changes reaches disk one build behind.
	M->UpdateCachedExpressionData();
	for (const FScalarSpec& Spec : Scalars)
	{
		float Value = 0.f;
		M->GetScalarParameterValue(FHashedMaterialParameterInfo(Spec.Parameter), Value);
		checkf(FMath::IsNearlyEqual(Value, Spec.Default), TEXT("%s cached as %f, expected %f"), Spec.Parameter,
		       Value, Spec.Default);
	}
	for (const FVectorSpec& Spec : Vectors)
	{
		FLinearColor Value;
		M->GetVectorParameterValue(FHashedMaterialParameterInfo(Spec.Parameter), Value);
		checkf(Value.Equals(Spec.Default), TEXT("%s cached as %s"), Spec.Parameter, *Value.ToString());
	}
	Save(M);
	return M;
}

UMaterialInstanceConstant* BuildInstance(UMaterial* Parent)
{
	auto* MI = LoadObject<UMaterialInstanceConstant>(nullptr, *InstancePath);
	if (!MI)
	{
		auto* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
		Factory->InitialParent = Parent;
		MI = CastChecked<UMaterialInstanceConstant>(
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
				.Get()
				.CreateAsset(InstanceName, Folder, UMaterialInstanceConstant::StaticClass(), Factory));
	}
	// Deliberately left with no overrides: the instance shows the material's defaults, and ticking a
	// parameter here overrides that one parameter only. An instance caches its parent's defaults, and
	// re-setting the same parent does not invalidate that cache, so a re-run would otherwise keep
	// serving the previous build's values for every default this pass changed.
	UMaterialEditingLibrary::SetMaterialInstanceParent(MI, Parent);
	MI->UpdateCachedData();
	UMaterialEditingLibrary::UpdateMaterialInstance(MI);
	Save(MI);
	return MI;
}

UProgressBar* FillBar(UWidgetBlueprint* BP)
{
	return CastChecked<UProgressBar>(BP->WidgetTree->FindWidget(TEXT("HealthBar")));
}

void AssignToWidget(UMaterialInstanceConstant* MI)
{
	auto* BP = Load<UWidgetBlueprint>(WidgetPath);
	Backup(BP);
	UProgressBar* HP = FillBar(BP);
	FProgressBarStyle Style = HP->GetWidgetStyle();
	UE_LOG(LogTemp, Display, TEXT("[BossLiquid] HealthBar fill was %s"),
	       *GetNameSafe(Style.FillImage.GetResourceObject()));
	Style.FillImage.SetResourceObject(MI);
	Style.FillImage.DrawAs = ESlateBrushDrawType::Image;
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
	HP->SetWidgetStyle(Style);
	HP->SetFillColorAndOpacity(FLinearColor::White);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FCompilerResultsLog Log;
	FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &Log);
	check(Log.NumErrors == 0);
	Save(BP);
}

bool Verify()
{
	auto* M = Load<UMaterial>(MaterialPath);
	if (M->MaterialDomain != MD_UI || M->BlendMode != BLEND_Translucent)
	{
		UE_LOG(LogTemp, Error, TEXT("[BossLiquid] %s is not a translucent UI material"), *MaterialName);
		return false;
	}
	auto* MI = Load<UMaterialInstanceConstant>(InstancePath);
	if (MI->Parent != M)
	{
		UE_LOG(LogTemp, Error, TEXT("[BossLiquid] %s does not parent to %s"), *InstanceName, *MaterialName);
		return false;
	}

	bool bOK = true;
	for (const FScalarSpec& Spec : Scalars)
	{
		float Value = 0.f;
		float Direct = 0.f;
		M->GetScalarParameterValue(FHashedMaterialParameterInfo(Spec.Parameter), Direct);
		if (!MI->GetScalarParameterValue(FHashedMaterialParameterInfo(Spec.Parameter), Value) ||
		    !FMath::IsNearlyEqual(Value, Spec.Default))
		{
			UE_LOG(LogTemp, Error, TEXT("[BossLiquid] scalar %s: instance %f, material %f, expected %f"),
			       Spec.Parameter, Value, Direct, Spec.Default);
			bOK = false;
		}
	}
	for (const FVectorSpec& Spec : Vectors)
	{
		FLinearColor Value;
		if (!MI->GetVectorParameterValue(FHashedMaterialParameterInfo(Spec.Parameter), Value) ||
		    !Value.Equals(Spec.Default))
		{
			UE_LOG(LogTemp, Error, TEXT("[BossLiquid] vector %s missing or off default"), Spec.Parameter);
			bOK = false;
		}
	}
	UTexture* Texture = nullptr;
	if (!MI->GetTextureParameterValue(FHashedMaterialParameterInfo(TextureParameter), Texture) || !Texture)
	{
		UE_LOG(LogTemp, Error, TEXT("[BossLiquid] texture parameter %s missing"), TextureParameter);
		bOK = false;
	}

	// A commandlet does not cache material shaders on load, so ask for the compile that the error
	// list and the statistics are read from. Nothing is saved here, so this stays a read-only check.
	UMaterialEditingLibrary::RecompileMaterial(M);
	if (FMaterialResource* Resource = M->GetMaterialResource(ERHIFeatureLevel::SM6))
	{
		for (const FString& Error : Resource->GetCompileErrors())
		{
			UE_LOG(LogTemp, Error, TEXT("[BossLiquid] shader: %s"), *Error);
			bOK = false;
		}
	}
	// The two reads live inside the Custom node, so they do not show up in NumPixelTextureSamples;
	// the sampler count is what bounds the texture work, and the instruction count the rest. Both
	// ceilings are regression guards, well above what this material measures (3 samplers, ~131
	// instructions), not targets.
	const FMaterialStatistics Stats = UMaterialEditingLibrary::GetStatistics(M);
	UE_LOG(LogTemp, Display, TEXT("[BossLiquid] cost: %d pixel instructions, %d samplers, %d texture samples"),
	       Stats.NumPixelShaderInstructions, Stats.NumSamplers, Stats.NumPixelTextureSamples);
	if (Stats.NumSamplers > 3 || Stats.NumPixelShaderInstructions > 200)
	{
		UE_LOG(LogTemp, Error, TEXT("[BossLiquid] shader cost has grown past its budget"));
		bOK = false;
	}

	UObject* Fill = FillBar(Load<UWidgetBlueprint>(WidgetPath))->GetWidgetStyle().FillImage.GetResourceObject();
	if (Fill != MI)
	{
		UE_LOG(LogTemp, Error, TEXT("[BossLiquid] HealthBar fill is %s, expected %s"), *GetNameSafe(Fill),
		       *InstanceName);
		bOK = false;
	}

	UE_LOG(LogTemp, Display, TEXT("[BossLiquid] %s: %d scalar, %d vector and 1 texture parameter"),
	       bOK ? TEXT("PASS") : TEXT("FAIL"), int32(UE_ARRAY_COUNT(Scalars)), int32(UE_ARRAY_COUNT(Vectors)));
	return bOK;
}
} // namespace VTGBossLiquid

UVTGBossHealthLiquidCommandlet::UVTGBossHealthLiquidCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UVTGBossHealthLiquidCommandlet::Main(const FString& Params)
{
	using namespace VTGBossLiquid;
	FString PreviewFile;
	if (FParse::Value(*Params, TEXT("Preview="), PreviewFile))
	{
		return VTGRenderCombatHUD(PreviewFile);
	}
	if (FParse::Param(*Params, TEXT("Apply")))
	{
		AssignToWidget(BuildInstance(BuildMaterial()));
	}
	return Verify() ? 0 : 1;
}
