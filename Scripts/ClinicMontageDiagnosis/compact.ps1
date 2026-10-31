$files = @('L5Clinic_Still','BPLM_Clinic','NE_PlayMontage_Start','NE_PlayMontage_EndOrTran','NE_PlayMontage_InterruptRes')
foreach ($f in $files) {
 $result = [System.Collections.Generic.List[string]]::new()
 foreach($line in Get-Content "Scripts/ClinicMontageDiagnosis/$f.t3d") {
  if($line -match '^      Begin Object Name="([^"]+)".*:(EventGraph|[^:]*Playing[^:]*|[^:]*Finished[^:]*)\.') { $result.Add($line.Trim()) }
  elseif($line -match '^         (FunctionReference|VariableReference|CustomFunctionName|EventReference|NodeComment)=') {$result.Add($line.Trim())}
  elseif($line -match '^         CustomProperties Pin .*PinName="([^"]+)"') {
   $pin=$Matches[1];$direction=if($line.Contains('Direction="EGPD_Output"')){'OUT'}else{'IN'}
   $link=if($line -match 'LinkedTo=\(([^)]+)\)'){$Matches[1]}else{''}
   $val=if($line -match '(?:DefaultValue|DefaultObject)="([^"]*)"'){$Matches[1]}else{''}
   $result.Add("  $pin $direction $val -> $link")
  }
 }
 $result | Set-Content "Scripts/ClinicMontageDiagnosis/$f.graph.txt"
}
