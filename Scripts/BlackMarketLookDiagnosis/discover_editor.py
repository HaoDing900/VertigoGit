import sys,time,json
sys.path.insert(0,r'G:/Epic/UE_5.3/Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python')
import remote_execution
r=remote_execution.RemoteExecution();r.start();time.sleep(2)
print(json.dumps(r.remote_nodes));r.stop()
