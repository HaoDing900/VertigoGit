#include "VTGSaveDisplayNamesCommandlet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "Save/VTGSaveDisplayNames.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "VTGManualLocalization.h"

namespace SaveDisplayNames
{
const TCHAR* LocationPath = TEXT("/Game/Widget/SaveLoad/DT_SaveLocationNames");
const TCHAR* PhasePath = TEXT("/Game/Widget/SaveLoad/DT_SaveStageNames");
/** One namespace, row name as the key: stable, greppable, and what the manual panel lists them under. */
const TCHAR* TextNamespace = TEXT("VTG.SaveDisplay");
const TCHAR* SourceCulture = TEXT("en-US");
const TCHAR* TargetCulture = TEXT("zh-Hans");

struct FSeed
{
    const TCHAR* Key;
    const TCHAR* English;
    const TCHAR* Chinese;
};

/**
 * The demo route, as authored on the wiki. Row names are the real level and stage names, so they are
 * copied verbatim - including L3PoliceStationToMaket's spelling and the bare "3Day1" - because a row
 * name that does not match what the save file records resolves to nothing.
 *
 * Two levels appear twice on the route under different stages. The location table is keyed by level,
 * so each keeps one wording and the stage carries the difference:
 *   Sa_AptDownAlley   - 公寓楼下 (wiki also wrote 居民公寓楼下 for the later visit)
 *   BlackMarketStreet - 商业模块 (wiki also wrote 商业街 for the later visit)
 */
const FSeed Locations[] = {
    {TEXT("L2Bar"), TEXT("“Triple Penny Fish”"), TEXT("“鱼贯三连”夜店")},
    {TEXT("L2_StreetOutsideBar"), TEXT("Commerce Module"), TEXT("商业区")},
    {TEXT("L2TunnelCine"), TEXT("Central Free District — Tunnel"), TEXT("自由区中部，隧道")},
    {TEXT("SaApartment"), TEXT("Apartment"), TEXT("公寓")},
    {TEXT("Sa_AptDownAlley"), TEXT("Apartment Downstairs"), TEXT("公寓楼下")},
    {TEXT("L_SewerUnderApartment"), TEXT("Residential Sewer System"), TEXT("住宅区下水道系统")},
    {TEXT("L3_CentralPoliceStation_FrontDoor_DownStream"), TEXT("The Sump Stream Police Station"), TEXT("下游区警局枢纽")},
    {TEXT("Tunnel_NearBlackMarket"), TEXT("Black Market Perimeter Tunnel"), TEXT("黑市外围隧道")},
    {TEXT("BlackMarketEntrance"), TEXT("Old Market Underpass, Black Market Periphery"), TEXT("旧市场地下通道，黑市外围")},
    {TEXT("BlackMarketStreet"), TEXT("“Black Market”, Commerce Module"), TEXT("“黑市”，商业模块")},
    {TEXT("AdultStore_BlackMarketStreet"), TEXT("“Silicone Valley”"), TEXT("“硅谷”")},
    {TEXT("ShoppingCenter_Yard"), TEXT("Commercial Plaza"), TEXT("商业广场")},
    {TEXT("Clinic"), TEXT("“Wellspring” Clinic, Commercial Plaza"), TEXT("“回春泉”诊所，商业广场")},
    {TEXT("Tunnel_ClinicToFactory"), TEXT("Upper Sump Passage"), TEXT("下游区上段通道")},
};
/** The stage column reads as time of day / investigation state, so several stages share one wording. */
const FSeed Phases[] = {
    {TEXT("Stage_Bar_Intro"), TEXT("Night"), TEXT("夜晚")},
    {TEXT("Stage_BarStreet_L2"), TEXT("Night"), TEXT("夜晚")},
    {TEXT("L2CineTunnelDriveWithFC"), TEXT("Night"), TEXT("夜晚")},
    {TEXT("L3Apt_Day1Morning"), TEXT("Daybreak"), TEXT("天亮时分")},
    {TEXT("L3PreSewerL"), TEXT("Daybreak"), TEXT("天亮时分")},
    {TEXT("L3Sewer"), TEXT("Daybreak"), TEXT("天亮时分")},
    {TEXT("3Day1"), TEXT("Morning"), TEXT("早晨")},
    {TEXT("L3Day1Morning"), TEXT("Morning"), TEXT("早晨")},
    {TEXT("L3PoliceStationToMaket"), TEXT("Worktime"), TEXT("工作时间")},
    {TEXT("L4BlackMarketEntrance"), TEXT("Worktime"), TEXT("工作时间")},
    {TEXT("L4MarketStage_BeforeAdult"), TEXT("Worktime"), TEXT("工作时间")},
    {TEXT("L4AdultStore_Case1"), TEXT("Worktime"), TEXT("工作时间")},
    {TEXT("L4MarketStage_AfterAdult"), TEXT("Investigation Ongoing"), TEXT("调查进行中")},
    {TEXT("L4MarketToClinic"), TEXT("Investigation Ongoing"), TEXT("调查进行中")},
    {TEXT("L5_ClinicYard"), TEXT("Worktime"), TEXT("工作时间")},
    {TEXT("L5ClinicFirstVisit"), TEXT("Worktime"), TEXT("工作时间")},
    {TEXT("L6ClinicToFactory"), TEXT("Worktime"), TEXT("工作时间")},
};

/**
 * A text identity the localization panel can actually see.
 *
 * FText::AsCultureInvariant is explicitly skipped by the gather (VTGManualLocalization.cpp checks
 * Text.IsCultureInvariant()), so seeding with it would produce rows that can never be translated -
 * which is the whole point of putting this wording in a table.
 */
FText Authored(FName Key, const TCHAR* English)
{
    return FText::ChangeKey(TextNamespace, Key.ToString(), FText::FromString(English));
}
bool IsTranslatable(const FText& Text)
{
    const auto Key = FTextInspector::GetKey(Text);
    return !Text.IsCultureInvariant() && Key.IsSet() && !Key.GetValue().IsEmpty();
}

UDataTable* Build(const TCHAR* Path, const FSeed* Seeds, int32 Count, bool bApply, bool bReseed, int32& OutWritten)
{
    OutWritten = 0;
    auto* Existing = LoadObject<UDataTable>(nullptr, Path);
    if (!bApply)
        return Existing;

    UDataTable* Table = Existing;
    if (!Table)
    {
        UPackage* Package = CreatePackage(Path);
        check(Package);
        Table = NewObject<UDataTable>(Package, *FPackageName::GetShortName(Path), RF_Public | RF_Standalone);
        check(Table);
        Table->RowStruct = FVTGSaveDisplayRow::StaticStruct();
        FAssetRegistryModule::AssetCreated(Table);
    }
    check(Table->RowStruct == FVTGSaveDisplayRow::StaticStruct());

    for (int32 I = 0; I < Count; ++I)
    {
        const FName Key(Seeds[I].Key);
        const auto* Current = Table->FindRow<FVTGSaveDisplayRow>(Key, TEXT("Seed"), /*bWarnIfMissing=*/false);
        const bool bUpToDate = Current && IsTranslatable(Current->Display) && Current->Display.ToString() == Seeds[I].English;
        // Wording authored in the editor is never clobbered by a plain -Apply; -Reseed says to put
        // the table back to what this file holds, which is what the wiki route is for. A row with no
        // text identity is rewritten either way - it predates the identity fix and could never
        // have been translated.
        if (bUpToDate || (Current && IsTranslatable(Current->Display) && !bReseed))
            continue;
        FVTGSaveDisplayRow Row;
        const FString Wording = (Current && !bReseed) ? Current->Display.ToString() : FString(Seeds[I].English);
        Row.Display = Authored(Key, *Wording);
        Table->AddRow(Key, Row);
        ++OutWritten;
    }
    if (OutWritten == 0)
        return Table;

    Table->Modify();
    UPackage* Package = Table->GetOutermost();
    Package->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    check(UPackage::SavePackage(
        Package, Table, *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()),
        Args));
    return Table;
}

void Report(const TCHAR* Label, UDataTable* Table)
{
    if (!Table)
    {
        UE_LOG(LogTemp, Display, TEXT("SAVE DISPLAY %s: missing"), Label);
        return;
    }
    for (const auto& Pair : Table->GetRowMap())
        if (const auto* Row = reinterpret_cast<const FVTGSaveDisplayRow*>(Pair.Value))
            UE_LOG(LogTemp, Display, TEXT("SAVE DISPLAY %s  %s = \"%s\""), Label, *Pair.Key.ToString(), *Row->Display.ToString());
}

/** Rows and seeds must agree one-for-one, or a stale row would ship unreviewed wording. */
void CheckMatchesSeeds(UDataTable* Table, const FSeed* Seeds, int32 Count)
{
    check(Table);
    checkf(Table->GetRowMap().Num() == Count, TEXT("%s has %d rows, expected %d"), *Table->GetName(), Table->GetRowMap().Num(), Count);
    for (int32 I = 0; I < Count; ++I)
    {
        const auto* Row = Table->FindRow<FVTGSaveDisplayRow>(FName(Seeds[I].Key), TEXT("Verify"), /*bWarnIfMissing=*/false);
        checkf(Row, TEXT("%s is missing row %s"), *Table->GetName(), Seeds[I].Key);
        checkf(IsTranslatable(Row->Display), TEXT("%s row %s has no text identity"), *Table->GetName(), Seeds[I].Key);
        checkf(Row->Display.ToString() == Seeds[I].English, TEXT("%s row %s reads \"%s\", expected \"%s\""), *Table->GetName(),
               Seeds[I].Key, *Row->Display.ToString(), Seeds[I].English);
    }
}
}

UVTGSaveDisplayNamesCommandlet::UVTGSaveDisplayNamesCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UVTGSaveDisplayNamesCommandlet::Main(const FString& Params)
{
    using namespace SaveDisplayNames;
    const bool bApply = FParse::Param(*Params, TEXT("Apply"));
    const bool bReseed = FParse::Param(*Params, TEXT("Reseed"));
    int32 WrittenLocations = 0, WrittenPhases = 0;
    UDataTable* LocationTable = Build(LocationPath, Locations, UE_ARRAY_COUNT(Locations), bApply, bReseed, WrittenLocations);
    UDataTable* PhaseTable = Build(PhasePath, Phases, UE_ARRAY_COUNT(Phases), bApply, bReseed, WrittenPhases);
    Report(TEXT("location"), LocationTable);
    Report(TEXT("phase   "), PhaseTable);
    UE_LOG(LogTemp, Display, TEXT("SAVE DISPLAY rows written: locations=%d phases=%d"), WrittenLocations, WrittenPhases);

    // Point the settings at the tables first: the resolve checks below go through the settings, so
    // doing it afterwards would make the very first apply assert against a table nothing points at.
    if (bApply)
    {
        check(LocationTable && PhaseTable);
        auto* Settings = GetMutableDefault<UVTGSaveDisplayNamesSettings>();
        check(Settings);
        Settings->LocationNames = LocationTable;
        Settings->PhaseNames = PhaseTable;
        // SaveConfig writes the per-user file; defaultconfig settings belong in Config/DefaultGame.ini.
        Settings->TryUpdateDefaultConfigFile();
        UE_LOG(LogTemp, Display, TEXT("SAVE DISPLAY settings -> %s | %s"), *Settings->LocationNames.ToString(),
               *Settings->PhaseNames.ToString());
    }

    // Two steps, as the localization notes require: collect the text identities into
    // Localization/ManualTranslations.json, then compile them into Content/Localization/Vertigo.
    if (bApply)
    {
        FString Error;
        check(VTGManualLocalization::GatherAssetAndSave(LocationTable, Error));
        check(VTGManualLocalization::GatherAssetAndSave(PhaseTable, Error));
        // These strings exist only to be read by the player, so the "player-visible" box is a
        // statement of fact here - and without it Compile drops them and the panel disables their
        // translation fields. Untick it in the panel if a row ever stops being player-facing.
        for (UDataTable* Table : {LocationTable, PhaseTable})
            for (const auto& Pair : Table->GetRowMap())
                checkf(VTGManualLocalization::ApproveDisplayText(TextNamespace, Pair.Key.ToString(), Error), TEXT("%s"), *Error);
        // Origin and en-US are the same wording here, written out rather than left to fall back, so
        // the panel shows an English field that is filled in like any other language.
        auto Translate = [&Error](const FSeed* Seeds, int32 Count)
        {
            for (int32 I = 0; I < Count; ++I)
            {
                checkf(VTGManualLocalization::SetTranslation(TextNamespace, Seeds[I].Key, SourceCulture, Seeds[I].English, Error),
                       TEXT("%s"), *Error);
                checkf(VTGManualLocalization::SetTranslation(TextNamespace, Seeds[I].Key, TargetCulture, Seeds[I].Chinese, Error),
                       TEXT("%s"), *Error);
            }
        };
        Translate(Locations, UE_ARRAY_COUNT(Locations));
        Translate(Phases, UE_ARRAY_COUNT(Phases));
        const bool bCompiled = VTGManualLocalization::Compile(Error);
        UE_LOG(LogTemp, Display, TEXT("SAVE DISPLAY localization gathered; compile=%d %s"), bCompiled, *Error);
        check(bCompiled);
    }

    // Resolved the way the save menu resolves it, on both paths - so a plain run proves the wiring
    // that was written to config still holds, not just that the tables exist on disk.
    UE_LOG(LogTemp, Display, TEXT("SAVE DISPLAY resolve L2_StreetOutsideBar = \"%s\" / Stage_BarStreet_L2 = \"%s\""),
           *UVTGSaveDisplayNames::ResolveLocation(TEXT("L2_StreetOutsideBar")).ToString(),
           *UVTGSaveDisplayNames::ResolvePhase(TEXT("Stage_BarStreet_L2")).ToString());
    // An unmapped level still reads as something; an unmapped stage deliberately reads as nothing.
    UE_LOG(LogTemp, Display, TEXT("SAVE DISPLAY unmapped level = \"%s\" unmapped stage = \"%s\""),
           *UVTGSaveDisplayNames::ResolveLocation(TEXT("Some_Unmapped_Level")).ToString(),
           *UVTGSaveDisplayNames::ResolvePhase(TEXT("Stage_Unmapped")).ToString());
    check(!UVTGSaveDisplayNames::ResolveLocation(TEXT("Some_Unmapped_Level")).IsEmpty());
    check(UVTGSaveDisplayNames::ResolvePhase(TEXT("Stage_Unmapped")).IsEmpty());
    // Every route entry must resolve through the settings, not just exist in the asset.
    for (const FSeed& Seed : Locations)
        checkf(UVTGSaveDisplayNames::ResolveLocation(Seed.Key).ToString() == Seed.English, TEXT("location %s resolved to \"%s\""),
               Seed.Key, *UVTGSaveDisplayNames::ResolveLocation(Seed.Key).ToString());
    for (const FSeed& Seed : Phases)
        checkf(UVTGSaveDisplayNames::ResolvePhase(FName(Seed.Key)).ToString() == Seed.English, TEXT("phase %s resolved to \"%s\""),
               Seed.Key, *UVTGSaveDisplayNames::ResolvePhase(FName(Seed.Key)).ToString());
    CheckMatchesSeeds(LocationTable, Locations, UE_ARRAY_COUNT(Locations));
    CheckMatchesSeeds(PhaseTable, Phases, UE_ARRAY_COUNT(Phases));
    UE_LOG(LogTemp, Display, TEXT("SAVE DISPLAY PASS (%d locations, %d phases)"), (int32)UE_ARRAY_COUNT(Locations),
           (int32)UE_ARRAY_COUNT(Phases));
    return 0;
}
