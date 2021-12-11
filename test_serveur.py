import socket
# from subprocess import call
import subprocess
import time
#import matplotlib.pyplot as plt

params = ["alpha"]

vals = [[0,1,0.01]]
moyenne = 100 # on va faire la moyenne sur 10 valeurs

def get_addr():
    hostname = socket.gethostname()
    local_ip = socket.gethostbyname(hostname)

    print(local_ip)

def call_serv(name,value):
    #subprocess.call(["./client2", "127.0.0.2", "8080", "bigfile.txt"])
    #call(["./server"]) le serveur tourne en continu donc pas besoin de le lancer
    #start = time.time()
    #output = subprocess.run(["./client1", "127.0.0.2", "8080", "bigfile.txt"], capture_output=True).stdout
    #output = subprocess.run(["./server"], timeout  = 10,capture_output=True).stdout
    #print(output)
    print("alpha = ",value)
    subprocess.run(["./server","8080",str(float(value))]) # pour avoir la sortie du programme : DEBUG

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
                #print("Version n°",moy," du serveur n°",y)
                #     debit = debit + call_client()

                call_serv(params[i],param)
                #call_client()
                #time.sleep(0.5)
                moy  = moy - 1
                # debit = debit/moyenne
                # params[i][y] = debit
            y = y-1


if __name__ == '__main__':
    print("Début main")
    #get_addr() # on va utiliser l'adresse pour lancer le script en ssh
    launch_serv()
    print("Fin main")
