#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "../inc/errExit.h"
#include "../inc/sharedmemory.h"
#include "../inc/semaphore.h"
#include "../inc/constant.h"

//clientExec è il programma che si preoccupa dell'esecuzione dei servizi
//Salva,Invia e stamapa

const int SERVICE_PRINT = 1;    //constanti per memorizzare il servizio
const int SERVICE_SAVE = 2;     //vengono memorizzati in un intero per confrontare 
const int SERVICE_SEND = 3;

const char *IPC_SHD_MEM_KEY_PATH = "../IPC_KEYS/ipc_key_mem.conf"; //inserisce la path dei file come costanti
const char *IPC_SEM_KEY_PATH = "../IPC_KEYS/ipc_key_sem.conf";

int main (int argc, char *argv[]) { //argc sono tutti gli argomenti da riga di comando (es. ./clientExec Giulio 12345 ciao.txt ciao123 --> 4 argomenti)
                                    //argv è un puntutatore ad ogni elemento di argc

    if(argc < 3) {  //Se ci sono meno di 3 argomenti ritorna errore n.b. che ./clientExec conta anch'esso come argomento
        errExit("Wrong arugments number, minimum 3 please...");
    }

    char *userCode = argv[1]; //userCode punterà al primo argomento di argv (id dell'utente es. Giulio)
    long key = strtol(argv[2], NULL, 10);   //strtol --> string to long - su key viene salvata l'interno numero convertito da una stringa
                                            //NULL --> indica che può essere qualsiasi lunghezza - 10 perché in base 10

    //---SEMAFORO---//-------------------------------------------------------------------------------------------------------------------------
    key_t semkey = ftok(IPC_SEM_KEY_PATH, 'a'); //system call per generare una chiave random (file to key) la chiave sarà la stessa del server
    if(semkey == -1) {                          //in quanto gli viene specificato il percorso del file
        errExit("Creation token for semaphore failed");
    }
    int semid = getSemaphore(semkey, 1);	//funzione contenuta in semaphore.c che serve ad ottenere il semaforo in esecuzione nel server				
    
    
    //---MEMORIA CONDIVISA---//----------------------------------------------------------------------------------------------------------------
    //Accedo alla memoria condivisa, attach, utilizzo dei semafori
    key_t memkey = ftok(IPC_SHD_MEM_KEY_PATH, 'a'); //stessa roba del semaforo però per la memoria condivisa
    if(memkey == -1) {
        errExit("Creation token for memory failed");
    }
    int shmid = getSharedMemory(memkey, sizeof(struct Memoryrow) * (int) LENGTH_SHARED_MEM);	//Serve per ottenere la memoria condivisa prendendo la dimensione 
                                                                                    	        //di una righa e la moltiplica per il numero delle righe
    struct Memoryrow *pointer = (struct Memoryrow*) attachSharedMemory(shmid, 0);	

    //find and mark key, verifica la validità della chiave-----------------------------------------------------------------------------------
    enterInCriticalSection(semid); //si assicura di entrare in modo protetto nella memoria condivisa
    int findMark = findAndMark(userCode, key, pointer); //funzione in sharedmemory.c che verifica la validità della chiave 
                                                        //serve per trovare la chiave per poterla rimuovere
    exitFromCriticalSection(semid); //esce dal semaforo in quanto non serve più accedere alla memoria condivisa
    
    freeSharedMemory(pointer);					//dopo aver verificato la validità della chiave libero la shdmem
    
    //Se troviamo una chiave----------------------------------------------------------------------------------------------------------------
    /*Nel caso in cui find mark vale -1 allora la chiave è gia tata utilizzata
      Nel caso in cui valgo 0 la chiave è inesistente
      Nel caso in cui sia maggiore di 0 significa che c'è una chiave di cui è già stato rimosso l'usercode
      
      La chiave ora mi serve per riconoscere il servizio */
    if(findMark > 0) {
        printf("Chiave trovata e rimossa dalla memoria!\n");						
        int length = argc-2;           		//toglie nome programma e userCode così che rimanga la chiave         
        char *args[length+1];     			//nuova variabile che punta a tutti gli elementi della righa di comando (per il nome del programma)
        int j = 3;                          //per saltare il nome del programma, l'user code e la chiave
        for(int i=1; i<length; i++) {       //ciclo for per copiare gli elementi in argv in args
            args[i] = argv[j];              //argv[j] J --> copia così gli elementi da 3 in poi su args
            j++;
        }
        args[length] = (char *)NULL;    		//di defalut l'ultimo elemento deve puntare a NULL              
        //Riconosciamo il servizio dalla chiave
        int service = (int) (key % 10);	       //(1,2,3 o 9)     			
        if(service == SERVICE_PRINT) {      //serve per confrontare le costanti con il servizio
                                            // e di conseguenza eseguire i programmi di stampa, invia o salva
            args[0] = "stampa";				//avvia exec sostiuisce il processo e viene scartato tutto
            execvp("./stampa", args);       //execvp system call che serve ad eseguire il programma prende come argomento
                                            //il comando da eseguire e gli argomenti dell'array es. ./stampa Giulio 12345 ecc...            
        } else if(service == SERVICE_SEND) {
            args[0] = "invia";
            execvp("./invia", args);
        } else if(service == SERVICE_SAVE) {
            args[0] = "salva";
            execvp("./salva", args);
        } else {
            printf("Servizio inesistente. Errore con la chiave data...\n");
        }
        errExit("Errore nell'esecuzione del programma\n");
    } else {
        if(findMark == -1) {
            printf("La chiave richiesta è già stata utilizzata!\n");
        } else {
            printf("Coppia chiave, utente (%li, %s) inesistente.\n", key, userCode);
        }
    }
    
    return 0;
}
