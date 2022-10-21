import os
import time 

dt = (time.time_ns()/1000000)
#time.sleep(1) 
#print( (time.time_ns()/1000000)-dt)
#while(
#ret = os.system("./cap")
while(time.time_ns()/1000000 - dt < 1000) :
	ret = os.system("./cap")

print("Done")
