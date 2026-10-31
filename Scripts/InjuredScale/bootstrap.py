import unreal,time
_vtg_start=time.monotonic()
def _vtg_start_trace(dt):
    if time.monotonic()-_vtg_start<2:
        return
    unreal.unregister_slate_post_tick_callback(_vtg_boot_handle)
    exec(compile(open('G:/Epic/Unreal projects/VertigoGit/Scripts/InjuredScale/preview_trace.py').read(),'preview_trace.py','exec'),globals())
_vtg_boot_handle=unreal.register_slate_post_tick_callback(_vtg_start_trace)
