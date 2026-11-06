import unreal
w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/PoliceStation/L3_CentralPoliceStation_FrontDoor_DownStream')
for label,x,y in [('approach_start',983.444397,1806.473877),('approach_end',1018.154541,1596.237183)]:
 for dx,dy in [(0,0),(-30,0),(30,0),(0,-30),(0,30)]:
  start=unreal.Vector(x+dx,y+dy,200)
  end=unreal.Vector(x+dx,y+dy,45)
  for complex in [True,False]:
   r=unreal.SystemLibrary.line_trace_single(w,start,end,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,complex,[],unreal.DrawDebugTrace.NONE,True)
   unreal.log('POLICE_MAP_TRACE '+label+' offset='+str((dx,dy))+' complex='+str(complex)+' result='+str(r))
