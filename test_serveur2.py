import os
import sys
# from subprocess import call
import subprocess
import time
#import matplotlib.pyplot as plt

params = ["alpha","window_size","window_factor","window_return","beta","rto_factor"]
default = [0.999,1,1,1,0.9,4]

vals = [[0,0.1,0.01],[1,1000,100],[1,100,10],[1,1000,100]]
moyenne = 2 # on va faire la moyenne sur 10 valeurs

# def get_addr():
#     hostname = socket.gethostname()
#     local_ip = socket.gethostbyname(hostname)
#     print(local_ip)

def call_serv(name,value):
    #stdout=subprocess.DEVNULL
    print("param : ",name," = ",str(value))
    if(name=="alpha"):
        subprocess.run(["./serveur2-pln","8080",str(float(value)),str(default[1]),str(default[2]),str(default[3]),str(default[4]),str(default[5]),"0"],timeout=60) # pour avoir la sortie du programme : DEBUG
    elif (name=="window_size"):
        subprocess.run(["./serveur2-pln","8080",str(default[0]),str(value),str(default[2]),str(default[3]),str(default[4]),str(default[5]),"0"], timeout=60) # pour avoir la sortie du programme : DEBUG
    elif (name=="window_factor"):
        subprocess.run(["./serveur2-pln","8080",str(default[0]),str(default[1]),str(value),str(default[3]),str(default[4]),str(default[5]),"0"],timeout=60) # pour avoir la sortie du programme : DEBUG
    elif (name=="window_return"):
        subprocess.run(["./serveur2-pln","8080",str(default[0]),str(default[1]),str(default[2]),str(value),str(default[4]),str(default[5]),"0"], timeout=60) # pour avoir la sortie du programme : DEBUG
    elif (name=="beta"):
        subprocess.run(["./serveur2-pln","8080",str(default[0]),str(default[1]),str(default[2]),str(default[3]),str(value),str(default[5]),"0"],timeout=60) # pour avoir la sortie du programme : DEBUG
    elif (name=="rto_factor"):
        subprocess.run(["./serveur2-pln","8080",str(default[0]),str(default[1]),str(default[2]),str(default[3]),str(default[4]),str(value),"0"],timeout=60) # pour avoir la sortie du programme : DEBUG
    else: 
        print("Invalid parameter")
        
    
def launch_serv():
    #f = open("debit_client.txt", "w")
    for i in range(len(params)):
        #print(i)
        #f = open("params_serv_"+params[i]+".txt", "w")
        y = vals[i][1]/vals[i][2]
        #print(y)
        
        while(y+1>0):
            moy = moyenne
            debit = 0
            while(moy>0): 
                param = round(vals[i][1]-(y*vals[i][2]),4)
                try :
                    call_serv(params[i],param)
                except subprocess.TimeoutExpired:
                    print("Time out")
                    #continue
                
                moy  = moy - 1
            y = y-1


if __name__ == '__main__':
    print("Début main")
    #get_addr() # on va utiliser l'adresse pour lancer le script en ssh
    os.system("make")
    mode = ""
    #launch_serv()
    if len(sys.argv) <= 1:
        print('no arguments passed')
    else :
        mode = sys.argv[1]
    
    if(mode=="1"):
        launch_serv()
    else:
        call_serv("alpha",0.99)
    
    
    print("Fin main")
