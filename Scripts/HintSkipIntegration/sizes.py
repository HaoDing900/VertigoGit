import unreal
for name in ['WB_ControlsHints','WB_ControlsHintBlock','WB_ControlsHintKeyImage']:
 a=unreal.load_asset('/Game/Inventory/InventoryXBased/UMG/Inventory/ControlsHints/'+name)
 c=unreal.get_default_object(a.generated_class())
 try: unreal.log('HINT_TEMPLATE_SIZE '+name+' '+str(c.get_editor_property('Size')))
 except Exception: pass
