#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

#include "../inc/keymanager.h"
#include "../inc/errExit.h"
#include "../inc/semaphore.h"

int shmid; //Shared memory id
int semid; //Semaphore id
struct Memoryrow *mempointer;   //puntattore alla memoria condivisa in sharedmemory.h
int *maxRowUsed;
const int FIXED_TIME = 60 * 5;  //variabile utilizzata per fissare 5 minuti di tempo

void alarmHandler(int sig) {    //
    if(sig == SIGALRM) { 
        printf("<KeyManager: %d> Alarm scattato!\n", getpid());
	//Rimozione della chiave        
	enterInCriticalSection(semid);
        deleteInvalidKeys();
        exitFromCriticalSection(semid);
    }
}

int keymanager(int memoryId, int semaphoreId, struct Memoryrow *pointer) {
    shmid = memoryId;
    semid = semaphoreId;
    mempointer = pointer;

    if(signal(SIGALRM, alarmHandler) == SIG_ERR)    //system call signal passa la gestione del segnale SIGALARM (FLAG della system call) all'alarHandler(funzione)
        errExit("Alarm handler failed");

    while(1) {
        alarm(30);  //system call alarm invia un segnale SIGALRM ogni tot di tempo (30 s in questo caso)
        pause();    //In pratica ogni 30s viene eseguita la funzione alarmHandler
    }
    
    return 0;
}

//Funzione chiamata solamente dal padre (server)----------------------------------------------------------------------------------
int insertKey(long key, char userCode[]) {  //funzione chiamata dal server per inrerie la chiave nella memoria
    enterInCriticalSection(semid);          //la funzione cerca una righa libera per inserire 
    //Troviamo il primo slot libero disponibile
    int foundSlot = 0; //
    for(int i=0; i<LENGTH_SHARED_MEM && foundSlot == 0; i++) {  //cicle che serve per trovare il primo spazio disponibile nella memoria condivisa
        struct Memoryrow *row = &(mempointer[i]);
        if(row->key == 0) { //significa che ho trovato una righa libera della memoria condivisa
            strcpy(row->userCode, userCode);
            row->timestamp = time(NULL);        //imposta il timestam al tempo in cui inerisco la chiave
            row->key = key;                     //inerisco la chiave nella memoria ocndivisa
            printf("<Server> Dato inserito in memoria a posizione %i\n", i);    //inserito la chiave nella posizione i del ciclo
            foundSlot = 1;  //valore da ritornare in caso in cui trovi lo slot libero
            if((*maxRowUsed) <= i) {			//tengo traccia delle righe utilizzate
                (*maxRowUsed)++;						
            }
        }
    }
    exitFromCriticalSection(semid);
    return foundSlot;
}

//Funzione chiamata solo dal processo padre(server)
//Eliminazione delle rige con usercode vuoto e chiave = 0
//Le righe con usercode vuoto e chiave con valore > 0 sono chiavi già utilizzate
void deleteInvalidKeys() {
    time_t timestamp = time(NULL);  //time(NULL) ---> tempo attuale del sistema  (timestamp del sistema attuale)		
    for(int i=0; i <(*maxRowUsed) && i< LENGTH_SHARED_MEM; i++) {	//maxrowUsed per non iterare tuteele 50 righe ma solo quele utilizzate	        
        struct Memoryrow *row = &(mempointer[i]); //Ciclo che scorre tutte le righe della memoria condivisa
        //Individuo tutti le chiavi che non sono stati eliminati o sono vecchi
        int interval = (int) (timestamp - row->timestamp);			      
        if(row->key != 0 && interval > (int) (FIXED_TIME)) {    //se c'è una chiave diversa da 0 ed è lì da più di 5 minuti viene eliminata
            printf("<KeyMananer> Dato scaduto(Usercode: %s key: %li) cancellato alla posizione %i\n", row->userCode, row->key, i);
            row->key=0;
            strcpy(row->userCode, ""); //string copy inserisco l'usercode vuoto
        }
    }
}















