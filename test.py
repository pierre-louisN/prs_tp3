import socket
# from subprocess import call
import subprocess
import time
#import matplotlib.pyplot as plt

def get_addr():
    hostname = socket.gethostname()
    local_ip = socket.gethostbyname(hostname)

    print(local_ip)

def call_client():
    #subprocess.call(["./client2", "127.0.0.2", "8080", "bigfile.txt"])
    #call(["./server"]) le serveur tourne en continu donc pas besoin de le lancer
    start = time.time()
    #output = subprocess.run(["./client1", "127.0.0.2", "8080", "bigfile.txt"], capture_output=True).stdout
    output = subprocess.run(["./client1", "127.0.0.2", "8080", "bigfile.txt", "0"], capture_output=True).stdout
    #subprocess.call(["./client2", "127.0.0.2", "8080", "bigfile.txt"])
    end = time.time()
    #print(end - start)
    result = output.find('Total bytes received '.encode()) # donne l'index pour trouver le nombre d'octets 
    val = (int)(output[result+21:-1].decode('UTF-8'))
    print(val,"octets en",(end - start)," secondes")
    debit = (val/(end - start))
    print(debit,"octets/s")


if __name__ == '__main__':
    print("Début main")
    get_addr() # on va utiliser l'adresse pour lancer le script en ssh
    call_client()
    print("Fin main")
