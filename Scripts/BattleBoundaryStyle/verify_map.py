import unreal,json,re
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/BattleBoundaryStyle'
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar')
actors=unreal.EditorLevelLibrary.get_all_level_actors();lm=next(a for a in actors if a.get_class().get_name()=='BPLM_L2StreetBarFight_C');a=lm.get_editor_property('FightAreaCollision')
assert a and a.get_class().get_name()=='BP_FightingAreaCollision_BarHologram_C'
expected=json.loads((out/'FightingArea-BP_FightingAreaCollision_C_1.json').read_text())
def nums(s):return [float(n) for n in re.findall(r'[xyzw]: (-?[0-9.]+)',s)]
def same(x,y):return len(x)==len(y) and all(abs(a-b)<.001 for a,b in zip(x,y))
assert same(nums(str(a.get_actor_transform())),nums(expected['transform']))
for old in expected['components']:
 if old['class']!='BoxComponent':continue
 c=next(c for c in a.get_components_by_class(unreal.BoxComponent) if c.get_name()==old['name'])
 assert same(nums(str(c.get_relative_transform())),nums(old['relative'])),old['name']
 assert same(nums(str(c.get_unscaled_box_extent())),nums(old['box_extent']))
meshes=[c for c in a.get_components_by_class(unreal.StaticMeshComponent) if 'BattleStyle' in [str(t) for t in c.get_editor_property('component_tags')]]
assert len(meshes)==23
for c in meshes:
 assert c.get_editor_property('static_mesh').get_name()=='SM_HologramLogo'
 assert c.get_material(0).get_name()=='MI_HologramLogo_VTGRestrict'
 assert not c.is_visible()
assert not any(x.get_class().get_name()=='BP_FightingAreaCollision_C' for x in actors)
unreal.log('BOUNDARY_RELOAD_PASS reference, actor transform, all original collision shapes, 23 exact meshes/materials, inactive default')
