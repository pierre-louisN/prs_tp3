import socket
import sys
# from subprocess import call
import subprocess
import time
import matplotlib
matplotlib.use('Agg') # no UI backend

import matplotlib.pyplot as plt
import numpy as np

params = ["alpha"]

vals = [[0,1,0.1]]

# chaque param va contenir des dictionnaires 
# un dictionnaire => 0.000 : 65000 où 65000 est le débit, 
moyenne = 10 # on va faire la moyenne sur 10 valeurs

def get_addr():
    hostname = socket.gethostname()
    local_ip = socket.gethostbyname(hostname)

    print(local_ip)

def call_client():
    print("Nouveau client")
    #subprocess.call(["./client2", "127.0.0.2", "8080", "bigfile.txt"])
    #call(["./server"]) le serveur tourne en continu donc pas besoin de le lancer
    start = time.time()
    #output = subprocess.run(["./client1", "127.0.0.2", "8080", "bigfile.txt"], capture_output=True).stdout
    #output = subprocess.run(["./client1", "127.0.0.2", "8080", "bigfile.txt", "0"], capture_output=True).stdout
    output = subprocess.run(["./client1", "127.0.0.2", "8080", "fleur.jpg"], timeout=10,capture_output=True).stdout
    #subprocess.call(["./client2", "127.0.0.2", "8080", "bigfile.txt"])
    end = time.time()
    #print(end - start)
    result = output.find('Total bytes received '.encode()) # donne l'index pour trouver le nombre d'octets 
    print(result)
    val = (int)(output[result+21:-1].decode('UTF-8'))
    print(val,"octets en",(end - start)," secondes")
    debit = (val/(end - start))
    #print(debit,"octets/s")
    return debit

def get_rate():
    for i in range(len(params)):
        #print(i)
        #f = open("params_serv_"+params[i]+".txt", "w")
        y = vals[i][1]/vals[i][2]
        #res.append(params[i])
        max=0 #on va vouloir trouver le maximum pour chaque paramètre
        params[i] = []
        params[i].append({})
        while(y>0):
            moy = moyenne
            debit = 0
            calc = 0
            while(moy>0): 
                print("Version n° ",moy," du client n° ",y)
                try :
                    debit = debit + call_client()
                    calc = calc + 1
                except subprocess.SubprocessError:
                    print("Time out")
                    #moy = moy + 1
                debit = debit + call_client()
                #subprocess.call(["./client1", "127.0.0.2", "8080", "bigfile.txt"]) # pour avoir la sortie du programme : DEBUG
                #call_client()
                time.sleep(0.1)
                moy  = moy - 1
            debit = debit/calc
            params[i][0][y] = debit
            if(debit>max):
                max = debit
            params[i].append(max)
            y = y-1
            print("fin de la première boucle")


def plot_res():
    for i in range(len(params)):
        #plt.plot(*zip(*sorted(test.items())))
        plt.plot(*zip(*sorted(params[i].items())))
        plt.title("Débit en fonction de"+params[i]+"au cours du temps")

        #plt.show()
        plt.savefig("matplotlib_"+params[i]+".png")  #savefig, don't show


if __name__ == '__main__':
    print("Début main")
    #call_client()
    # if len(sys.argv) <= 1:
    #     print('no arguments passed')
    #     nbre_clients = 10
    # else :
    #     nbre_clients = sys.argv[1]
    get_rate()

    plot_res()

    print("Fin main")

