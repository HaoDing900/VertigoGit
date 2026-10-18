import pathlib, re, sys
nodes = {}
current = None
data = pathlib.Path(sys.argv[1]).read_bytes()
for line in data.decode('utf-16' if data.startswith(b'\xff\xfe') else 'utf-8-sig').splitlines():
    match = re.match(r'      Begin Object Name="([^"]+)" ExportPath=".*?:([^:.]+)\.', line)
    if match:
        current = (match[2], match[1])
        nodes.setdefault(current, {'name': '', 'pins': []})
    elif line == '      End Object':
        current = None
    elif current:
        label = re.search(r'(?:MemberName|CustomFunctionName|SelectedFunctionName)="([^"]+)"', line)
        if label and 'CustomProperties' not in line:
            nodes[current]['name'] = label[1]
        if 'CustomProperties Pin' in line:
            name = re.search(r'PinName="([^"]*)"', line)
            link = re.search(r'LinkedTo=\(([^)]*)\)', line)
            val = re.search(r'DefaultValue="([^"]*)"', line)
            obj = re.search(r'DefaultObject="([^"]*)"', line)
            if name:
                nodes[current]['pins'].append(name[1] + ('=' + val[1] if val else '') + (' object=' + obj[1] if obj else '') + (' -> ' + link[1] if link else ''))
for key, node in nodes.items():
    if key[0].startswith('ExecuteUbergraph_') or key[0].endswith('_MERGED'):
        continue
    out = key[0] + '.' + key[1] + ' [' + node['name'] + '] ' + '; '.join(node['pins'])
    if len(sys.argv) < 3 or any(term.lower() in out.lower() for term in sys.argv[2:]):
        print(out)
