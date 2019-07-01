#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <unistd.h>

#include "../inc/constant.h"
#include "../inc/errExit.h"
#include "../inc/request_response.h"

char *baseClientFifoPath = "/tmp/client_fifo";
char *serverFifoPath = "/tmp/server_fifo";

int main (int argc, char *argv[]) {

    char clientFifoPath [100];
    
    struct Request request; //Struct presa dall'header request_response.h che serve a memorizzare il codice dell'utente (nome)
                            //E servizio (Stampa, Salva o invia)
                            //E pid che è il id del processo per poter terminarlo

    printf("Benvenuto a ClientReq!\n");
    printf("Offro i seguenti servizi: Stampa, Salva e Invia!\n");
    printf("Inserire il codice identificativo: ");
    scanf("%s", request.user_code);
    
    printf("Inserire il servizio richiesto: ");
    scanf("%s", request.service);

    request.pid = getpid(); // libreria system call

    //Creazione FIFO Client -----------------------------------------------------------------------------------------------------------------
    sprintf(clientFifoPath, "%s_%d", baseClientFifoPath, request.pid); 	//string printf (memorizza nel primo argomento il secondo argomento (%s_%d)
                                                                        // che in questo caso richiama il terzo e quarto elemento
                                                                        //- clientFifoPath mette baseClient --> associato al pid
    									                                //piu richieste del cient ognugna pid diverso
    
    if(mkfifo(clientFifoPath, S_IRUSR | S_IWUSR | S_IWGRP) == -1)       //mkfifo --> crea una fifo 
                                                                        //Dove il primo argomento è dove devo crearla e i permessi nel secondo argomento
        
        errExit("Creation fifo client failed");                         //System call che se restituisce ritorna un messaggio di errore
    
    
    //Connessione alla FIFO Server ------------------------------------------------------------------------------------------------------------
    int serverFifoFD = open(serverFifoPath, O_WRONLY);                  //open -> apre il file
                                                                        //Apriamo il file serverFifoPath per scriverci la richiesta (Invia, stampa o salva)
    if(serverFifoFD == -1) {
        errExit("Connecting to server FIFO has failed");                //In caso di errore stampa -1 (90% system call ritorna -1 in caso di errore)
    }
    //Invio dei dati al Server attraverso FIFO SERVER -----------------------------------------------------------------------------------------
    if(write(serverFifoFD, &request, sizeof(struct Request)) 		    //la sytem call Write scrive la richiesta nel file serverFifoFD
        != sizeof(struct Request)) {			                        //&request = indirizzo di memoria
        errExit("Write Request in FIFO Server has failed");             //per la write bisogna sempre passare la dimensione del file che dobbiamo passsare
    }                                                                   // se la dimensione della struttura è diversa ritorna errore

    //Aspetto e leggo la risposta in FIFO CLIENT ----------------------------------------------------------------------------------------------
    printf("Messaggio inviato, attesa di risposta... \n");
    int clientFifoPathFD = open(clientFifoPath, O_RDONLY);              //variabile per leggere la rispoista inviata dal server (per questo aperta in sola lettura)
    if(clientFifoPathFD == -1) {
        errExit("Read client FIFO has failed");
    }

    struct Response response;           //contenuta in request_response che ha come variabili solo la chiave che verrà generata dopo
    int bufferRead = -1;		        //-1 perchè non restistuisce nulla, non inizializzati 	magari la sizof potrebbe essere 0 (meglio metterla a -1 per evitare errori)				
    bufferRead = read(clientFifoPathFD, &response, sizeof(struct Response));  //sistem call read molto simile alla write
    if(bufferRead != sizeof(struct Response)) {
        errExit("Response Fifo Client read failed");
    }
    //RESPONSE -------------------------------------------------------------------------------------------------------------------------------
    if(response.key == -1) {            //campo della struttura response
        printf("Servizio richiesto al server non disponibile\n");
    } else if(response.key == 0) {
        printf("Chiave non generata. La memoria e` piena.\n");
    } else {
        printf("Chiave rilasciata del server: %li \n", response.key);
    }
    //Chiusura e unlink del FD e del file ------------------------------------------------------------------------------------------------------
    if(close(serverFifoFD) || close(clientFifoPathFD) == -1)  //sytem call close --> server per chiudere i file
        errExit("Error closing FIFOs");                       //basta passargli la variabile dove è stato aperto il file
    
    if(unlink(clientFifoPath) != 0){                            //system call unlink --> rimuove il file della fifo
        errExit("Unlink client fifo failed");
    }
    
    return 0;
}
