#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ipc.h>

#include "../inc/errExit.h"
#include "../inc/keymanager.h"
#include "../inc/request_response.h"
#include "../inc/keygenerator.h"
#include "../inc/sharedmemory.h"
#include "../inc/semaphore.h"

//costanti utilizzate per memorizzare il servizio (Stampa, salva e invia) in un intero
const int SERVICE_PRINT = 1;	//dato che bisogna accodare il servizio alla chiave viene convertito il servizio 		
const int SERVICE_SAVE = 2;		//da char ad un int e viene inserito in fondo alla chiave per poi essere recuperato
const int SERVICE_SEND = 3;     //calcolando il resto di 10 10 (key % 10)
const int NO_SERVICE = 9;

const char *IPC_SHD_MEM_KEY_PATH = "../IPC_KEYS/ipc_key_mem.conf"; //Sono dei file per definire le chiave della memoria condivisa 
const char *IPC_SEM_KEY_PATH = "../IPC_KEYS/ipc_key_sem.conf";     // e del semaforo globalmente

const char *serverFifoPath = "/tmp/server_fifo"; 
const char *baseClientFifoPath = "/tmp/client_fifo";

sigset_t signalset; //serve per inizializzare un set di segnali (come sig_term, sig_kill, sig_stop)

pid_t childpid;  //pid_t dichiare un processo -in questo caso figlio- (process identification)

int serverFifoFD;
int serverFifoExtraFD;
long requestNumber = 1;					

//
int shmid; //dichiaro una memoria condivisa 

int indexPosShmid; //indice posizione memoria condivisa
struct Memoryrow *mempointer;
int *maxRowUsed;		//variabile utilizzata per tenere traccia delle righe utilizzate nella shared memory

int semid;

//---------------------------------------------------------------------------------------------------------------------------------
//Funzione utilizzate per chiudere tutte le risorse utilizzate: fifo, semafori e memoria condivisa
void quit() {						
    if (serverFifoFD != 0 && close(serverFifoFD) == -1)
        errExit("close failed");

    if (serverFifoExtraFD != 0 && close(serverFifoExtraFD) == -1)
        errExit("close failed");

    if (unlink(serverFifoPath) != 0)
        errExit("unlink failed");
    
    freeSharedMemory(mempointer);
    removeSharedMemory(shmid);

    freeSharedMemory(maxRowUsed);
    removeSharedMemory(indexPosShmid);
    

    removeSemaphore(semid);
    
    _exit(0);
}

//--------------------------------------------------------------------------------------------------------------------------------------
//handler per gestire la SIGTERM nel il mio set di segnali
void sigHandler(int sig) {		//gestice il segnale SIGTERM quando viene terminato il processo termina anche il processo figlio		
    printf("Sigterm called %d\n", getpid());       	
    if(sig == SIGTERM) {  //se sig == SIGTERM uccide anche il processo figlio
        printf("Killing process %d\n", childpid);
        if(kill(childpid, SIGTERM) == -1)  //kill è la system call che invia il segnale di terminazione che è diverso dal comando da terminale
            printf("Killing child task failed!\n"); //per terminare il processo occore SIGTERM
                                                    //SIGKILL invece termina brutalmente il processo
        wait(NULL);				//system call wait che aspetta uccisione figlio fino a che non termina ciò che sta facendo
        quit();                 //funzione dichiarta prima per chiudere il server fifo
    }
}

//funzione che mi ritorna il servizio corretto
const int getService(char serviceInput[]) {		
    if(strcmp(serviceInput, "Stampa") == 0) {
        return SERVICE_PRINT;
    } else if(strcmp(serviceInput, "Invia") == 0) {
        return SERVICE_SEND;
    } else if(strcmp(serviceInput, "Salva") == 0) {
        return SERVICE_SAVE;
    }
    return NO_SERVICE;
}

//---------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------MAIN--------------------------------------------------------------------------------
int main (int argc, char *argv[]) {

    printf("Processo server partito con pid: %d!\n", getpid());	 //system call per ottenere l'id del processo
    //-----SEGNALI-----//
    //Creazione e inizializzazione dell'handler
    if(signal(SIGTERM, sigHandler) == SIG_ERR) {  //con la system call signal passa la gestione del SIGTERM al sighandler(gestore segnali)
        errExit("Error creation signal handler");   //fa parte del 10% di system call che non ritornano -1 per l'errore
    }

    if(sigfillset(&signalset) == -1)   //system call sigfillset che inizializza il nostro set di segnali riempiendolo con tutti i segnali presenti nel sistema
        errExit("Error filling signal set");  //signalset --> set di segnali 
    if(sigdelset(&signalset, SIGTERM) == -1) //system call sigdelset serve per eliminare il segnale sigter dal set di segnali (&signalset)
        errExit("Error removing signal from mask set");
    if(sigprocmask(SIG_SETMASK, &signalset, NULL) == -1) //system call signal process mask che aggiorna la machera del segnale (aggiorna il file dopo l'eliminazione)
        errExit("Error setting mask"); //SIG_SETMASK è flag della system call sigpromask che serve a settare la maschera

    //SHARED MEMORY
    indexPosShmid = createSharedMemoryFromSystem(sizeof(int));	//funzione dichiarata in sharedmemory.c
    maxRowUsed = attachSharedMemory(indexPosShmid, 0); //collega il segmento di memoria condiisa allo spazio di indirizzamento logico del processo chiamante
                                                        //inclusa in sharedmemory.c
                                                        //0 sta per lettura e scrittura
    *maxRowUsed = 0;	//imposta il puntatore alla memoria condivisa alla prima riga					

    key_t keySharedMem = ftok(IPC_SHD_MEM_KEY_PATH, 'a'); // system call ftok (file to key) serve agenerare una chiave IPC random prendendo come anrgomento il percorso
    if(keySharedMem == -1)                                  //della chiave (IPC_KEY_MEM.conf e altri)
        errExit("Ftok for shdmem failed!");
    const int lengthMemory = LENGTH_SHARED_MEM;  //Server per definire la lunghezza della memoria
    int totalLength = sizeof(struct Memoryrow) * lengthMemory; //crea tante righe quante quelle della memoria condivisa (50 righe)
    printf("Total length of memory is %d: \n", totalLength); 
    shmid = createSharedMemory(keySharedMem, totalLength); //funzione 
    mempointer = (struct Memoryrow*) attachSharedMemory(shmid, 0);
    printf("This is the shared mem id: %d\n", shmid);

    //SEMAFORO: CREAZIONE E INIZIALIZZIONE -----------------------------------------------------------------------------------------------------
    key_t keySem = ftok(IPC_SEM_KEY_PATH, 'a'); //Dichiarazione speciale delle system call che serve a dichiarare una variabile di tipo chiave
    if(keySem == -1)
        errExit("Ftok for semaphore failed!");
    semid = createSemaphore(keySem, 1); //funzione dichiarata all'interno di semphore.c (1=numero di semafori che vogliamo creare)	
    printf("This is the semaphore id: %d\n", semid);
    union semun sem; //la union è come una struttura ma si può accedere ad un solo campo alla volta
    sem.val = 1;  //inizializza il semaforo a 1
    semctl(semid, 0, SETVAL, sem);		//system call (semaphore control) che serve per compiere operazione di controllo sui semafori
                                        //semid=id semaforo, 0 inizio indice del semaforo, SETVAL=flag che setta il valore che do in sem)
                                        //array[0] = sem.val = 1
                                        //tutta sta roba serve ad inizializzare il semaforo ad 1 perché i semafori vanno sempre inizilizzati
    
    //Creazione processo figlio (keymanager)
    pid_t pid = fork(); //system call che serve a creare processo figlio
    if(pid == 0) {      //quando cè una fork il figlio ritorna 0
        
        
        //Codice del figlio--------------------------------------------------------------------------------------------------------------

        //RESET SIGNAL SIGTERM
        if (signal(SIGTERM, SIG_DFL) == SIG_ERR ) //Il figlio eredita tutte le cosa dal padre inclusa la machera
            errExit("Error resetting sigterm for child process");   
            
        //Rimozione di SIGALARM dalla maschera dei segnali    
        if(sigdelset(&signalset, SIGALRM) == -1)       
            errExit("Error removing alarm from mask set");
        if(sigprocmask(SIG_SETMASK, &signalset, NULL) == -1)
            errExit("Error setting mask for ALRM");
        
	//start keymanager
        keymanager(shmid, semid, mempointer); //Ogni 5 minuti il keymanager rimuove le chiavi dal sistema

    } else {	//-------------------------------------------------------------------------------------------------------------------------					
        
        
        //Codice del padre(SERVER)-------------------------------------------------------------------------------------------------------
       
       
        childpid = pid;

	//FIFO SERVER
    //Opposto alle funzioni del ClientReq

        if(mkfifo(serverFifoPath, S_IRUSR | S_IWUSR | S_IRGRP) == -1) {
            errExit("Error creation Server FIFO");
        }
        printf("<Server> Attesa di un client\n");

        serverFifoFD = open(serverFifoPath, O_RDONLY);
        if(serverFifoFD == -1)
            errExit("Reading server fifo Failed");
        
        //Apertura della extra fifo in write per l'EOF 
        serverFifoExtraFD = open(serverFifoPath, O_WRONLY);	  //apre una 'ulteriore fifo per vedere quando il client finisce di scrivere		
        if(serverFifoExtraFD == -1)         //quando il client scrive la server fifo deve rimanerer aperta per prevenire l'EOF
            errExit("Writing server fifo Failed");
        
        //Gestione delle richieste del client e inserimento nella memoria condivisa
        int bufferRead = -1;
        struct Request request;
        do {										
            bufferRead = read(serverFifoFD, &request, sizeof(struct Request)); //uguale alla write ma per la lettura
            if(bufferRead != sizeof(struct Request)) {
                printf("Request read failed, incompatible or size differents\n");
            } else {
                struct Response response;
                response.key = -1;							
                const int service = getService(request.service);
                if(service == NO_SERVICE)
                    printf("<Server> Richiesta ricevuta da client <%s PID: %d>, servizio richiesto inesistente \n", request.user_code, request.pid);
                else    
                    printf("<Server> Richiesta ricevuta da client <%s PID: %d>, servizio richiesto: %i \n", request.user_code, request.pid, service);
                if(service != NO_SERVICE) {
                    //Generazione della chiave e inserimento nella memoria condivisa
                    long key = generateKey(service, request.user_code);	
                    response.key = key;
                    
                    if(insertKey(key, request.user_code) == 0) { //funzione chiamata in keygenerator.c
                        printf("Memoria piena!! Chiave non inserita\n");
                        response.key = 0; 
                    }
                    requestNumber++;
                } else {
                    printf("Servizio richiesto non disponibile!\n");
                    response.key = -1;
                }
                
		//Apertura di FIFO CLIENT e invio della response
                char clientFifoPath[100];
                sprintf(clientFifoPath, "%s_%d", baseClientFifoPath, request.pid);
                int clientFifoPathFD = open(clientFifoPath, O_WRONLY);
                if(clientFifoPathFD == -1) {
                    printf("Client FIFO open error!\n");
                } else {
                    if(write(clientFifoPathFD, &response, sizeof(struct Response)) !=
                        sizeof(struct Response)) {
                        printf("Write to client FIFO has failed\n");
                    } else {
                        printf("<Server> Risposta inviata a client <%s PID: %d>, chiave generata %li\n", request.user_code, request.pid, response.key);
                    }
                }
                if(close(clientFifoPathFD) == -1)
                    printf("Error closing FIFO client\n");
            }
            printf("<Server> Attesa di un client\n");
        } while(bufferRead != -1);
    }

    //Uscita e chiusura di tutte le risorse(solo in caso di rottura della fifo)
    quit();
}
