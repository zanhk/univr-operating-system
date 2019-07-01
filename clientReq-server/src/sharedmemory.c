#include <sys/shm.h>
#include <sys/stat.h>

#include "../inc/errExit.h"
#include "../inc/sharedmemory.h"

//Creazione della memoria condivisa
int createSharedMemory(key_t key, size_t size) { //Crea la memoria condivisa che serve per condividere file tra più processi (client ,server e keymanager in questo caso)			
    int shmid = shmget(key, size, IPC_CREAT |  S_IWGRP | S_IRGRP | S_IRUSR | S_IWUSR); //a differenza di createSharedMemoryFromSystem gli passiamo noi la chiave e non è random
    if(shmid == -1) {
        errExit("Creation memory failed");
    }
    return shmid;
}

int createSharedMemoryFromSystem(size_t size) {	 
    int shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | S_IWGRP | S_IRGRP | S_IRUSR | S_IWUSR); //system call che server per creare un memoria condivisa (shared memory get)
                        //Flag della system call che delega il il sistema operativo di trovare una chiave adatta alla memoria condivisa
                                    //size è la dimensione della memoria
                                            //IPC_CREAT Se non cè questo flag restituisce la chiave già presente altrimenti la crea
    if(shmid == -1) {
        errExit("Creation memory failed");
    }
    return shmid;
}

//Ottieniamo la memoria condivisa in caso sià già presente, return -1
int getSharedMemory(key_t key, size_t size) {
    int shmid = shmget(key, size, S_IWGRP | S_IRGRP | S_IRUSR | S_IWUSR);
    if(shmid == -1) {
        errExit("Creation memory failed");
    }
    return shmid;
}

//attach della memoria condivisa
void *attachSharedMemory(int shmid, int shmflg) {  //è un puntatore perché punta all'indirizzo della memoria
    void *pointer = shmat(shmid, NULL, shmflg); //system call (shared memory attach) che serve a collegare la memoria
    if (pointer == (void *)-1)					
        errExit("Attach shared memory failed");
    return pointer;
}

//detach della memoria condivisa
void freeSharedMemory(void *ptrSharedMemory) {
    if(shmdt(ptrSharedMemory) == -1)
        errExit("Dettach shared memory failed");
}

//remove della memoria condivisa
void removeSharedMemory(int shimd) {
    if(shmctl(shimd, IPC_RMID, NULL) == -1)
        errExit("Remove shared memory failed");
}
