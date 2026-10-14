param([string]$File='Scripts/L2FightDiagnosis/BPLM_L2StreetBarFight.copy')
$s=Get-Content -LiteralPath $File -Raw
$blocks=[regex]::Matches($s,'(?ms)^      Begin Object Name="(?<id>[^"]+)"[^\r\n]*\r?\n(?<body>.*?)^      End Object')
foreach($b in $blocks){
 $id=$b.Groups['id'].Value;$body=$b.Groups['body'].Value
 $fn=[regex]::Match($body,'(?:FunctionReference|VariableReference|EventReference)=.*?MemberName="([^"]+)"').Groups[1].Value
 $custom=[regex]::Match($body,'CustomFunctionName="?([^\r\n"]+)').Groups[1].Value
 $pins=[regex]::Matches($body,'CustomProperties Pin \(.*')
 $out=@()
 foreach($p in $pins){$v=$p.Value;$name=[regex]::Match($v,'PinName="([^"]+)"').Groups[1].Value;$link=[regex]::Match($v,'LinkedTo=\((.*?)\),').Groups[1].Value;$def=[regex]::Match($v,'Default(?:Value|Object)="([^"]*)"').Groups[1].Value
 if($link -or $def){$out+="$name=$def->$link"}}
 "$id [$fn$custom] $($out -join '; ')"
}
