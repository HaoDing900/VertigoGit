int32 RepairDirectMail(bool VerifyOnly)
{
    using namespace AlleyFix;
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));
    check(BP);
    UEdGraph* Graph=BP->UbergraphPages[0];
    auto* Branch=Node(Graph,TEXT("K2Node_IfThenElse_57"));
    auto* NewMail=Pin(Branch,TEXT("then"));
    auto* OldMenu=Pin(Node(Graph,TEXT("K2Node_CallFunction_227")),TEXT("execute"));
    auto* Read=Pin(Node(Graph,TEXT("K2Node_CallFunction_278")),TEXT("execute"));
    auto* Forced=Pin(Node(Graph,TEXT("K2Node_CallFunction_119")),TEXT("then"));
    check(Forced->LinkedTo.Contains(Read));
    check(Pin(Branch,TEXT("else"))->LinkedTo.Contains(Pin(Node(Graph,TEXT("K2Node_CallFunction_122")),TEXT("execute"))));
    if(!VerifyOnly)
    {
        check(NewMail->LinkedTo.Num()==1 && (NewMail->LinkedTo.Contains(OldMenu)||NewMail->LinkedTo.Contains(Read)));
        const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        const FString Backup=FPaths::ProjectSavedDir()/TEXT("QA/MailDirectBefore/BP_Player_Sa.uasset");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
        if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
        NewMail->BreakAllPinLinks();
        Link(NewMail,Read);
        Branch->NodeComment=TEXT("VTG new mail: opening terminal starts the selected mail story immediately; otherwise open the normal terminal menu.");
        if(!Save(BP))return 1;
    }
    check(NewMail->LinkedTo.Num()==1 && NewMail->LinkedTo.Contains(Read));
    check(Forced->LinkedTo.Contains(Read));
    check(!NewMail->LinkedTo.Contains(OldMenu));
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
    if(Log.NumErrors || BP->Status==BS_Error)return 2;
    UE_LOG(LogTemp,Display,TEXT("DIRECT_MAIL PASS: new mail and forced mail share the animation/camera/dialogue path; no-mail menu unchanged; compile errors=0"));
    return 0;
}
