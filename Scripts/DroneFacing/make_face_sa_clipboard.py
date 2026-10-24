from pathlib import Path
import uuid
nodes=[]
def pin(name,category,sub='',obj=None,out=False,value=None,link=None,hidden=False):
 gid=uuid.uuid4().hex.upper();s=f'   CustomProperties Pin (PinId={gid},PinName="{name}"'
 if out:s+=',Direction="EGPD_Output"'
 s+=f',PinType.PinCategory="{category}"'
 if sub:s+=f',PinType.PinSubCategory="{sub}"'
 if obj:s+=f',PinType.PinSubCategoryObject="{obj}"'
 if value is not None:s+=f',DefaultValue="{value}"'
 if link:s+=f',LinkedTo=({link[0]} {link[1]},)'
 if hidden:s+=',bHidden=True'
 return gid,s+')'
def node(name,lib,func,x,y,pins):
 s=f'Begin Object Class=/Script/BlueprintGraph.K2Node_CallFunction Name="{name}"\n   bIsPureFunc=True\n   FunctionReference=(MemberParent="/Script/CoreUObject.Class\'/Script/Engine.{lib}\'",MemberName="{func}")\n   NodePosX={x}\n   NodePosY={y}\n   NodeGuid={uuid.uuid4().hex.upper()}\n'
 s+='\n'.join(p[1] for p in pins)+'\nEnd Object';nodes.append(s)
# Stable pin IDs are shared by both ends of every link.
def p(name,cat,**kw):return pin(name,cat,**kw)
char="/Script/CoreUObject.Class'/Script/Engine.Character'";actor="/Script/CoreUObject.Class'/Script/Engine.Actor'";vec="/Script/CoreUObject.ScriptStruct'/Script/CoreUObject.Vector'";rot="/Script/CoreUObject.ScriptStruct'/Script/CoreUObject.Rotator'"
names=['FaceSa_Player','FaceSa_Location','FaceSa_LookAt','FaceSa_Break','FaceSa_Offset','FaceSa_Result']
# Plan the wiring first, then generate the pins with bilateral links.
specs=[ [('PlayerIndex','int',False,'0',None),('WorldContextObject','object',False,None,None),('ReturnValue','object',True,None,char)], [('self','object',False,None,actor),('ReturnValue','struct',True,None,vec)], [('Start','struct',False,'3402.651004,1370.583811,248.160009',vec),('Target','struct',False,None,vec),('ReturnValue','struct',True,None,rot)], [('InRot','struct',False,None,rot),('Roll','real',True,None,None),('Pitch','real',True,None,None),('Yaw','real',True,None,None)], [('A','real',False,None,None),('B','real',False,'-90.0',None),('ReturnValue','real',True,None,None)], [('Roll','real',False,'0.0',None),('Pitch','real',False,'0.0',None),('Yaw','real',False,None,None),('ReturnValue','struct',True,None,rot)] ]
ids={(i,j):uuid.uuid4().hex.upper() for i,a in enumerate(specs) for j in range(len(a))}
links={}
for a,b in [((0,2),(1,0)),((1,1),(2,1)),((2,2),(3,0)),((3,3),(4,0)),((4,2),(5,2))]:links[a]=b;links[b]=a
for i,a in enumerate(specs):
 pins=[]
 for j,(name,cat,out,value,obj) in enumerate(a):
  _,s=pin(name,cat,sub='double' if cat=='real' else '',obj=obj,out=out,value=value,link=(names[links[(i,j)][0]],ids[links[(i,j)]]) if (i,j) in links else None,hidden=(i==0 and j==1))
  s=s.replace(s.split('PinId=')[1].split(',')[0],ids[i,j]);pins.append((ids[i,j],s))
 lib,func=[('GameplayStatics','GetPlayerCharacter'),('Actor','K2_GetActorLocation'),('KismetMathLibrary','FindLookAtRotation'),('KismetMathLibrary','BreakRotator'),('KismetMathLibrary','Add_DoubleDouble'),('KismetMathLibrary','MakeRotator')][i]
 node(names[i],lib,func,i*260,0,pins)
Path('Docs/IPHADrone_FaceSa_Clipboard.txt').write_text('\n'.join(nodes)+'\n',encoding='utf8')
