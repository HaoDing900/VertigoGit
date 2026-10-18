#include "VTGBPLMRatesCommandlet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "Misc/PackageName.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Knot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "MovieSceneSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

UVTGBPLMRatesCommandlet::UVTGBPLMRatesCommandlet(){IsClient=false;IsEditor=true;LogToConsole=true;}
int32 UVTGBPLMRatesCommandlet::Main(const FString& Params)
{
    const bool Apply=FParse::Param(*Params,TEXT("Apply"));
    const bool Verify=FParse::Param(*Params,TEXT("VerifyOnly"));
    int32 Differences=0,ChangedAssets=0;
    auto* Base=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/BPLM"));
    if(!Base || !Base->GeneratedClass)return 1;
    auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.SearchAllAssets(true);
    TSet<FTopLevelAssetPath> Derived;
    Registry.GetDerivedClassNames({Base->GeneratedClass->GetClassPathName()}, {},Derived);
    FARFilter Filter;Filter.PackagePaths.Add(TEXT("/Game"));Filter.bRecursivePaths=true;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());Filter.bRecursiveClasses=true;
    TArray<FAssetData> Assets;Registry.GetAssets(Filter,Assets);
    int32 Count=0;
    for(const auto& Asset:Assets)
    {
        FString ClassPath;Asset.GetTagValue(TEXT("GeneratedClass"),ClassPath);
        if(!Derived.Contains(FTopLevelAssetPath(FPackageName::ExportTextPathToObjectPath(ClassPath))))continue;
        auto* BP=Cast<UBlueprint>(Asset.GetAsset());if(!BP)continue;
        ++Count;UE_LOG(LogTemp,Display,TEXT("RATE BLUEPRINT %s"),*BP->GetPathName());
        bool Changed=false;
        auto BeforeChange=[&]()
        {
            ++Differences;if(!Apply || Changed)return;
            const FString Filename=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
            const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/BPLMRates/Backup")/(BP->GetName()+TEXT(".uasset"));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
            if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*Filename)==COPY_OK);
            Changed=true;
        };
        for(const auto& V:BP->NewVariables)if(V.VarName.ToString().Contains(TEXT("Rate")) || V.VarName.ToString().Contains(TEXT("Scale")))
        {UE_LOG(LogTemp,Display,TEXT("RATE VARIABLE %s default=%s"),*V.VarName.ToString(),*V.DefaultValue);}
        TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
        TSet<FName> RateVariables;
        TFunction<void(UEdGraphPin*)> CheckRate=[&](UEdGraphPin* Pin)
        {
            if(!Pin)return;
            if(Pin->LinkedTo.Num())
            {
                for(auto* Link:Pin->LinkedTo)
                {
                    if(auto* Get=Cast<UK2Node_VariableGet>(Link->GetOwningNode())){RateVariables.Add(Get->VariableReference.GetMemberName());}
                    else if(auto* Knot=Cast<UK2Node_Knot>(Link->GetOwningNode())){CheckRate(Knot->GetInputPin());}
                    else {UE_LOG(LogTemp,Error,TEXT("RATE unresolved source %s"),*Link->GetOwningNode()->GetPathName());++Differences;}
                }
                return;
            }
            UE_LOG(LogTemp,Display,TEXT("RATE CHECK literal %s.%s=%s"),*Pin->GetOwningNode()->GetName(),*Pin->PinName.ToString(),*Pin->DefaultValue);
            if(!FMath::IsNearlyEqual(FCString::Atod(*Pin->DefaultValue),1.))
            {BeforeChange();if(Apply)Pin->DefaultValue=TEXT("1.000000");}
        };
        for(auto* Graph:Graphs)for(UEdGraphNode* Node:Graph->Nodes)
        {
            auto* Call=Cast<UK2Node_CallFunction>(Node);
            if(!Call)continue;
            UFunction* Function=Call->GetTargetFunction();
            if(Function && Function->GetOwnerClass()->IsChildOf(UMovieSceneSequencePlayer::StaticClass()) && Function->GetFName()==TEXT("SetPlayRate"))CheckRate(Call->FindPin(TEXT("PlayRate")));
            if(Call->FunctionReference.GetMemberName()==TEXT("CreateLevelSequencePlayer"))
            {
                if(auto* Rate=Call->FindPin(TEXT("Settings_PlayRate"))){CheckRate(Rate);}
                else if(auto* Settings=Call->FindPin(TEXT("Settings")))
                {
                    if(Settings->LinkedTo.Num()){UE_LOG(LogTemp,Error,TEXT("RATE linked settings require review %s"),*Call->GetPathName());++Differences;}
                    else if(!Settings->DefaultValue.IsEmpty())
                    {
                        FMovieSceneSequencePlaybackSettings Value;
                        check(FMovieSceneSequencePlaybackSettings::StaticStruct()->ImportText(*Settings->DefaultValue,&Value,nullptr,PPF_None,GWarn,TEXT("BPLMRate")));
                        if(!FMath::IsNearlyEqual(Value.PlayRate,1.f))
                        {
                            BeforeChange();if(Apply){Value.PlayRate=1;FString Text;FMovieSceneSequencePlaybackSettings::StaticStruct()->ExportText(Text,&Value,nullptr,nullptr,PPF_None,nullptr);Settings->DefaultValue=Text;}
                        }
                    }
                }
            }
        }
        // Include retained tuning variables even if their sequence hookup was later removed.
        for(const auto& V:BP->NewVariables)
        {
            FString Name=V.VarName.ToString().Replace(TEXT(" "),TEXT("")).Replace(TEXT("_"),TEXT(""));
            if(Name.Contains(TEXT("PlayRate")) || Name.Contains(TEXT("PlayScale")))RateVariables.Add(V.VarName);
        }
        for(FName Name:RateVariables)
        {
            auto* Property=FindFProperty<FNumericProperty>(BP->GeneratedClass,Name);
            if(!Property || !Property->IsFloatingPoint()){UE_LOG(LogTemp,Error,TEXT("RATE nonnumeric variable %s"),*Name.ToString());return 3;}
            UObject* CDO=BP->GeneratedClass->GetDefaultObject();void* Address=Property->ContainerPtrToValuePtr<void>(CDO);
            const double Old=Property->GetFloatingPointPropertyValue(Address);
            UE_LOG(LogTemp,Display,TEXT("RATE CHECK variable %s.%s=%.6f"),*BP->GetName(),*Name.ToString(),Old);
            if(!FMath::IsNearlyEqual(Old,1.))
            {
                BeforeChange();if(Apply){Property->SetFloatingPointPropertyValue(Address,1.);for(auto& V:BP->NewVariables)if(V.VarName==Name)V.DefaultValue=TEXT("1.000000");}
            }
            for(auto* Graph:Graphs)for(UEdGraphNode* Node:Graph->Nodes)
                if(auto* Set=Cast<UK2Node_VariableSet>(Node);Set && Set->VariableReference.GetMemberName()==Name)CheckRate(Set->FindPin(Name));
        }
        if(Changed)
        {
            FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
            FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);if(Log.NumErrors)return 4;
            for(FName Name:RateVariables){auto* P=FindFProperty<FNumericProperty>(BP->GeneratedClass,Name);check(FMath::IsNearlyEqual(P->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(BP->GeneratedClass->GetDefaultObject())),1.));}
            UPackage* Package=BP->GetOutermost();Package->FullyLoad();ResetLoaders(Package);Package->MarkPackageDirty();
            FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
            if(!UPackage::SavePackage(Package,BP,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Args))return 5;
            ++ChangedAssets;
        }
        for(auto* Graph:Graphs)for(UEdGraphNode* Node:Graph->Nodes)
        {
            const FString Title=Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
            bool Relevant=Title.Contains(TEXT("Play Rate")) || Title.Contains(TEXT("Play Scale")) || Title.Contains(TEXT("Sequence Player"));
            for(auto* Pin:Node->Pins)Relevant |= Pin->PinName.ToString().Contains(TEXT("PlayRate")) || Pin->PinName.ToString().Contains(TEXT("PlayScale")) || Pin->DefaultValue.Contains(TEXT("PlayRate="));
            if(!Relevant)continue;
            UE_LOG(LogTemp,Display,TEXT("RATE NODE %s %s %s %s"),*Graph->GetName(),*Node->GetName(),*Node->GetClass()->GetName(),*Title.Replace(TEXT("\n"),TEXT(" ")));
            for(auto* Pin:Node->Pins)
            {
                FString Links;for(auto* Linked:Pin->LinkedTo)Links+=Linked->GetOwningNode()->GetName()+TEXT(".")+Linked->PinName.ToString()+TEXT(" ");
                UE_LOG(LogTemp,Display,TEXT("RATE PIN %s dir=%d default=%s links=%s"),*Pin->PinName.ToString(),int32(Pin->Direction),*Pin->DefaultValue,*Links);
            }
        }
    }
    UE_LOG(LogTemp,Display,TEXT("RATE AUDIT blueprints including base=%d differences=%d changed assets=%d"),Count,Differences,ChangedAssets);
    return Verify && Differences ? 2 : 0;
}
