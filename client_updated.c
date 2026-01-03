#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>


#define SERVER_IP "127.0.0.1"
#define PORT 12352
#define BUFFER_SIZE 1024

int client_socket;
char buffer[BUFFER_SIZE];


// Fonction qui permet de faire chiffrer 
char *chiffrerCesar(char* texte, int decalage) {
    for (int i = 0; texte[i] != '\0'; i++) {
        char c = texte[i];

        // Lettre majuscule
        if (isupper(c)) {
            texte[i] = ((c - 'A' + decalage) % 26) + 'A';
        }
        // Lettre minuscule
        else if (islower(c)) {
            texte[i] = ((c - 'a' + decalage) % 26) + 'a';
        }
        // Sinon : caractère non alphabétique → inchangé
    }
    return texte;
}


// Fonction permettant de dechiffrer 
char *dechiffrerCesar(char* texte, int decalage) {
    return chiffrerCesar(texte, 26 - (decalage % 26)); // Inverser le décalage
}

// Thread d'envoi des messages
void *sendThread(void *arg) {
    while(1) {
        printf("Votre message : ");
        fflush(stdout); // on force l’affichage de la fonction printf
        if(!fgets(buffer, BUFFER_SIZE, stdin)) {
            perror("fgets");
            exit(EXIT_FAILURE);
        }
        buffer[strcspn(buffer, "\n")] = '\0';
        if(send(client_socket, chiffrerCesar(buffer,2), strlen(buffer), 0) < 0) {
            perror("send");
            exit(EXIT_FAILURE);
        }
        memset(buffer, 0, BUFFER_SIZE);
    }
    return NULL;
}

// Thread de réception des messages
void *recvThread(void *arg) {
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int rec = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if(rec <= 0) {
            printf("Déconnexion du serveur.\n");
            close(client_socket);
            exit(EXIT_FAILURE);
        }
        buffer[rec] = '\0';
        printf("%s",dechiffrerCesar(buffer,2));
        fflush(stdout);
    }
    return NULL;
}

int main() {
    struct sockaddr_in server_addr;
    pthread_t thread_send, thread_recv;
    
    // Demander au client son nom avant de se connecter
    char name[50];
    printf("Entrez votre nom : ");
    if(!fgets(name, sizeof(name), stdin)) {
        perror("fgets");
        exit(EXIT_FAILURE);
    }
    name[strcspn(name, "\n")] = '\0';

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(client_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    

    // Configuration de l'adresse du serveur
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    
    if(connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    
    // Dès la connexion, envoyer le nom du client au serveur
    send(client_socket, name, strlen(name), 0);
    
    // Réception du menu envoyé par le serveur
    memset(buffer, 0, BUFFER_SIZE);
    int bytes = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if(bytes <= 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    buffer[bytes] = '\0';
    printf("%s", buffer);
    
    // Saisie du choix de l'utilisateur (numéro du channel ou 0 pour créer)
    char choix[10];
    fgets(choix, sizeof(choix), stdin);
    choix[strcspn(choix, "\n")] = '\0';
    send(client_socket, choix, strlen(choix), 0);
    
    if(strcmp(choix, "0") == 0) {
        // Si le client veut créer un nouveau channel,
        // il attend la réponse du serveur, saisit le nom et l'envoie
        memset(buffer, 0, BUFFER_SIZE);
        int bytes2 = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if(bytes2 <= 0) {
            perror("recv");
            exit(EXIT_FAILURE);
        }
        buffer[bytes2] = '\0';
        printf("%s", buffer);
        
        char channelName[50];
        fgets(channelName, sizeof(channelName), stdin);
        channelName[strcspn(channelName, "\n")] = '\0';
        send(client_socket, channelName, strlen(channelName), 0);
    }
    
    // Lancement des threads pour la discussion
    // Envoie non bloquant
    pthread_create(&thread_send, NULL, sendThread, NULL);
    // Réception non bloquant
    pthread_create(&thread_recv, NULL, recvThread, NULL);
    
    // Liberation des ressources
    pthread_join(thread_send, NULL);
    pthread_join(thread_recv, NULL);
    
    close(client_socket);
    return 0;
}