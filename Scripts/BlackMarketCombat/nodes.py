import pathlib,re,json,sys
src=pathlib.Path(sys.argv[1]);stack=[];nodes=[]
raw=src.read_bytes(); text=raw.decode('utf16' if raw.startswith(b'\xff\xfe') else 'utf8')
for line in text.splitlines():
 s=line.strip()
 if s.startswith('Begin Object '): stack.append({'head':s,'lines':[]})
 elif s=='End Object':
  ob=stack.pop()
  if any('CustomProperties Pin' in l for l in ob['lines']): nodes.append(ob)
 elif stack: stack[-1]['lines'].append(s)
result=[]
for ob in nodes:
 h=ob['head'];lines=ob['lines']
 if 'ExecuteUbergraph' in h: continue
 result.append(h)
 for l in lines:
  if any(l.startswith(p) for p in ['FunctionReference=','VariableReference=','EventReference=','CustomFunctionName=','NodeComment=','MacroGraphReference=']):result.append(l)
  if l.startswith('CustomProperties Pin'):
   fields=re.findall(r'(?:PinName="[^"]*"|Direction="[^"]*"|DefaultValue="(?:\\.|[^"\\])*"|DefaultObject="[^"]*"|LinkedTo=\([^)]*\))',l)
   result.append('  '+' '.join(fields))
src.with_suffix('.nodes.txt').write_text('\n'.join(result),encoding='utf8')

