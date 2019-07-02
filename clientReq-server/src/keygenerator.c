#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "../inc/keygenerator.h"

struct Node *head = NULL;

//Generazione della chiave, l'utlima cifra identifica il servizio
long generateKey(int service, char *user_code) {
    //Generazione dell'hash
    int hash = 0;   //contiene i numeri generati randomicamente che verrano poi aggoingi alla chiave (long key)
    int randnum;    //
    srand(time(NULL));
    randnum = rand() % 100 + 1; //genero un num rand da 1 a 100
    for(int i = 0; user_code[i] != '\0'; i++){  //for che server per generare un numero random a partire
        hash += (int)user_code[i];              //da l'user code inserito convertito in un intero
    }
    hash *= randnum;    //hash che vine moltiplicata per il numero rando mgenerato
    long key = hash *10 + service;	//memorizza nel long key l'hash per 10 per aggiungere all'ultima cifra il numero del service
    //service è l'offset aggiunto per identificare il servizio
    
    while(containsKey(key) == 1) {			//se la chiave è già presente ne genero un'altra
        printf("The generation of key has created a replica: %li.\n", key);
        randnum = rand() % 100 + 1;
        for(int i = 0; user_code[i] != '\0'; i++){  //
            hash += (int)user_code[i];              
        }
        hash *= randnum;
        key = hash *10 + service;
    }
    addNode(key); //funzione che server per memorizzare la chiave in un nodo di una lista 
    return key;
}


int containsKey(long key) {
    struct Node *node = head;
    while(node != NULL) {
        if(node->value == key) {
            printf("Found same key\n");
            return 1;
        } else {
            node = node->next;
        }
    }
    return 0;
}

int addNode(long key) {
    struct Node *new_node = malloc(sizeof(struct Node*));
    new_node->value = key;
    new_node->next = NULL;

    if(head == NULL) {
        head = new_node;
        return 1;
    }
    struct Node *last = head;
    while(last->next != NULL) {
        last = last->next;
    }
    last->next = new_node;
    return 1;
}

