param([string]$Path)
$s=Get-Content $Path -Raw
$lines=[System.Collections.Generic.List[string]]::new()
foreach($m in [regex]::Matches($s,'(?ms)^      Begin Object Name="([^"]+)".*?^      End Object')){
 $lines.Add($m.Groups[1].Value)
 foreach($l in ($m.Value -split "`n")){
  if($l -match 'FunctionReference=|CustomFunctionName=|VariableReference=|EventReference=|DelegatePropertyName=|ComponentPropertyName=|NodeComment=|StructType='){$lines.Add($l.Trim())}
  if($l -match 'CustomProperties Pin'){
   $pn=[regex]::Match($l,'PinName="([^"]+)"').Groups[1].Value
   $ln=[regex]::Match($l,'LinkedTo=\((.*?)\)').Groups[1].Value
   $def=[regex]::Match($l,'Default(?:Object|Value)=(.*?),').Groups[1].Value
   if($ln -or $def){$lines.Add(' '+$pn+' => '+$ln+' default '+$def)}
  }
 }
}
$lines | Set-Content ($Path+'.summary.txt') -Encoding utf8
