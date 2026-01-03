#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netinet/in.h>

#define PORT 12352
#define BUFFER_SIZE 1024
#define MAX_SIZE_CLIENT 10
#define MAX_SIZE_CHANNEL 10

// Codes couleurs ANSI
#define RED     "\x1B[31m"
#define GREEN   "\x1B[32m"
#define YELLOW  "\x1B[33m"
#define BLUE    "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN    "\x1B[36m"
#define WHITE   "\x1B[37m"
#define RESET   "\x1B[0m"

// Structure représentant un client
typedef struct Client {
    char nom[25];
    int *socket;
    char ip[INET_ADDRSTRLEN];
    int port;
} Client;

// Structure représentant un channel
typedef struct Channel {
    Client* clientSockets[MAX_SIZE_CLIENT];
    int nombreClient;
    pthread_mutex_t mutex;
    char name[50]; // nom du channel
} Channel;

// Structure pour gérer la liste des channels
typedef struct {
    Channel *listeChannel[MAX_SIZE_CHANNEL];
    int tailleCourantListe;
} Channels;

Channels *channels;
FILE *desc = NULL;  // Fichier d'historique global

// on utilise le macro pour l'initialisation du mutex au lieu d'utiliser explicitement la fonction pthread_mutex_init().
pthread_mutex_t historique_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex pour synchroniser l'écriture dans l'historique

// Structure pour passer les paramètres au thread de discussion d'un client dans un channel
typedef struct {
    Client* client;
    Channel* channel;
} ParamClientForThread;

// Fonction de traitement pour un client dans un channel spécifique.
// Elle reçoit les messages, les diffuse aux autres clients du channel
// et écrit dans le fichier d'historique le nom, l'IP, le port, le channel et le message.
void* traitementClientByThread(void *arguments) {
    char buffer[BUFFER_SIZE];
    ParamClientForThread *param = (ParamClientForThread*)arguments;
    Client *client = param->client;
    Channel *channel = param->channel;
    free(param);

    while (1) {
        int receive_bytes = recv(*client->socket, buffer, BUFFER_SIZE, 0);
        if (receive_bytes <= 0) {
            if (receive_bytes == 0) {
                printf(RED "Client %s déconnecté.\n" RESET, client->nom);
            } else {
                perror(RED "Erreur lors de la reception" RESET);
            }
            // Suppression du client dans le channel si on detecte qu'il est deconnecté du channel.
            pthread_mutex_lock(&channel->mutex);
            for (int i = 0; i < channel->nombreClient; i++) {
                if (channel->clientSockets[i] == client) {
                    channel->clientSockets[i] = channel->clientSockets[channel->nombreClient - 1];
                    channel->clientSockets[channel->nombreClient - 1] = NULL;
                    channel->nombreClient--;
                    break;
                }
            }
            pthread_mutex_unlock(&channel->mutex);
            close(*client->socket);
            free(client->socket);
            free(client);
            pthread_exit(NULL);
        }
        buffer[receive_bytes] = '\0';
        printf(CYAN "\nMessage reçu de %s (%s:%d) [%s]: %s\n" RESET, client->nom, client->ip, client->port, channel->name, buffer);
        // Écriture dans le fichier d'historique
        pthread_mutex_lock(&historique_mutex);
        fprintf(desc, "[%s] (%s:%d) [Channel: %s] %s \n", client->nom, client->ip, client->port, channel->name, buffer);
        fflush(desc);
        pthread_mutex_unlock(&historique_mutex);
        // Diffusion du message aux autres clients du même channel
        pthread_mutex_lock(&channel->mutex);
        for (int i = 0; i < channel->nombreClient; i++) {
            if (channel->clientSockets[i] != NULL &&
                *(channel->clientSockets[i]->socket) != *client->socket) {
                if (send(*(channel->clientSockets[i]->socket), buffer, strlen(buffer), 0) == -1) {
                    perror(RED "Erreur lors de l'envoi" RESET);
                }
            }
        }
        pthread_mutex_unlock(&channel->mutex);
    }
    memset(buffer, 0, BUFFER_SIZE);
    return NULL;
}

// Fonction qui envoie le menu au client : liste des channels existants + option de création.
void sendMenuToClient(int client_sock) {
    char menu[BUFFER_SIZE];
    memset(menu, 0, BUFFER_SIZE);
    // On utilise strcat pour concaténer des chaînes dans le buffer menu.
    strcat(menu, YELLOW "Menu:\n" RESET);
    strcat(menu, YELLOW "Liste des channels existants:\n" RESET);
    if(channels->tailleCourantListe == 0) {
        strcat(menu, RED "Aucun channel existant.\n" RESET);
    } else {
        for(int i = 0; i < channels->tailleCourantListe; i++) {
            char line[100];
            sprintf(line, GREEN "%d : %s\n" RESET, i+1, channels->listeChannel[i]->name);
            strcat(menu, line);
        }
    }
    strcat(menu, MAGENTA "0 : Créer un nouveau channel\n" RESET);
    strcat(menu, YELLOW "Votre choix : " RESET);
    send(client_sock, menu, strlen(menu), 0);
}

// Création d'un nouveau channel et ajout du client initiateur.
Channel* createChannel(const char *channelName, Client *client) {
    Channel* newChannel = (Channel*)malloc(sizeof(Channel));
    if(newChannel == NULL) {
        perror(RED "malloc" RESET);
        exit(EXIT_FAILURE);
    }
    strncpy(newChannel->name, channelName, 50);
    newChannel->nombreClient = 0;
    pthread_mutex_init(&newChannel->mutex, NULL);
    newChannel->clientSockets[newChannel->nombreClient++] = client;
    return newChannel;
}

// Ajout d'un client dans un channel existant.
void rejoindreChannel(Client *client, int index) {
    if (index < 0 || index >= channels->tailleCourantListe) {
        printf(RED "Index de channel invalide.\n" RESET);
        return;
    }
    Channel *ch = channels->listeChannel[index];
    pthread_mutex_lock(&ch->mutex);
    if (ch->nombreClient < MAX_SIZE_CLIENT) {
        ch->clientSockets[ch->nombreClient++] = client;
        printf(GREEN "Client %s rejoint le channel %s\n" RESET, client->nom, ch->name);
    }
    pthread_mutex_unlock(&ch->mutex);
}

// Fonction de gestion d'un client.
// Le paramètre passé est un pointeur sur l'entier contenant le socket.
// Dans clientHandler, nous utilisons getpeername() pour récupérer l'adresse IP et le port.
// Elle s'occupe de récupérer l'adresse IP et le port du client,
// de recevoir son nom, d'envoyer le menu, et de gérer son choix (créer ou rejoindre un channel).
void* clientHandler(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    struct sockaddr_in addresseClient;
    socklen_t addr_len = sizeof(addresseClient);
    if(getpeername(client_sock, (struct sockaddr *)&addresseClient, &addr_len) == -1) {
        perror(RED "getpeername" RESET);
    }
    // On crée un tableau pour stocker l'adresse ip du client
    char ip_str[INET_ADDRSTRLEN];
    // inet_ntop() convertit l’adresse IP (stockée en binaire dans peer_addr.sin_addr) en une chaîne lisible.
    inet_ntop(AF_INET, &addresseClient.sin_addr, ip_str, INET_ADDRSTRLEN);
    // ntohs permet de convertir le port du format réseau en format hôte
    int port = ntohs(addresseClient.sin_port);
    // Allocation de la structure Client et stockage des informations de connexion
    Client *client = malloc(sizeof(Client));
    client->socket = malloc(sizeof(int));
    *(client->socket) = client_sock;
    strncpy(client->ip, ip_str, INET_ADDRSTRLEN);
    client->port = port;
    // Récupération du nom envoyé par le client lors de la connexion
    memset(buffer, 0, BUFFER_SIZE);
    int bytes = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
    if(bytes <= 0) {
        close(client_sock);
        free(client->socket);
        free(client);
        pthread_exit(NULL);
    }
    buffer[bytes] = '\0';
    strncpy(client->nom, buffer, sizeof(client->nom));
    printf(GREEN "\nClient connecté : %s (%s:%d)\n" RESET, client->nom, client->ip, client->port);
    // Envoi du menu au client avec la fonction sendMenuToClient
    sendMenuToClient(client_sock);
    // Réception du choix du client
    memset(buffer, 0, BUFFER_SIZE);
    bytes = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
    if(bytes <= 0) {
        close(client_sock);
        free(client->socket);
        free(client);
        pthread_exit(NULL);
    }
    buffer[bytes] = '\0';
    // atoi permet de convertir une chaîne en entier
    int choix = atoi(buffer);
    Channel *channel = NULL;

    // Le client souhaite créer un nouveau channel
    if(choix == 0) {    
        char messsage[] = MAGENTA "Entrez le nom du nouveau channel : " RESET;
        // On lui demande de saisir le nom du channel qu'il veut créer
        send(client_sock, messsage, strlen(messsage), 0);
        memset(buffer, 0, BUFFER_SIZE);
        bytes = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        if(bytes <= 0) {
            close(client_sock);
            free(client->socket);
            free(client);
            pthread_exit(NULL);
        }
        buffer[bytes] = '\0';
        // on crée le channel avec la fonction createChannel
        channel = createChannel(buffer, client);
        // On ajoute le channel dans la liste de channels
        channels->listeChannel[channels->tailleCourantListe++] = channel;
        printf(BLUE "Nouveau channel créé: %s par %s\n" RESET, channel->name, client->nom);
    } else {
        // Le client souhaite rejoindre un channel existant
        int index = choix - 1;
        if(index < 0 || index >= channels->tailleCourantListe) {
            char errMsg[] = RED "Choix invalide.\n" RESET;
            send(client_sock, errMsg, strlen(errMsg), 0);
            close(client_sock);
            free(client->socket);
            free(client);
            pthread_exit(NULL);
        }
        // On récupere le channel indexé par le client pour pouvoir ensuite lancer la fonction traitementClientByThread avec ce client
        channel = channels->listeChannel[index];
        rejoindreChannel(client, index);
    }

    // Préparation des paramètres pour le thread qui gére le client
    ParamClientForThread *param = malloc(sizeof(ParamClientForThread));
    param->client = client;
    param->channel = channel;
    pthread_t thread;
    // Lancement du thread pour gérer la discussion dans le channel
    pthread_create(&thread, NULL, traitementClientByThread, param);
    pthread_detach(thread);
    return NULL;
}

int main() {
    int server_socket;
    struct sockaddr_in server_addr, client_addr;

    // On stocke la taille de l'adresse du client
    socklen_t client_addr_size = sizeof(client_addr);
    // Ouverture du fichier d'historique en mode append
    desc = fopen("historique.txt", "a");
    if(desc == NULL) {
        perror(RED "Erreur lors de l'ouverture du fichier de log" RESET);
        exit(EXIT_FAILURE);
    }
    // Initialisation de la structure des channels
    channels = malloc(sizeof(Channels));
    channels->tailleCourantListe = 0;

    // Création du socket du server
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket == -1) {
        perror(RED "socket" RESET);
        exit(EXIT_FAILURE);
    }


   // Configuration de l'adresse du serveur
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

     // Association de la socket avec l'adresse du serveur
    if(bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror(RED "bind" RESET);
        exit(EXIT_FAILURE);
    }

    // Mettre le server en écoute des clients
    if(listen(server_socket, 5) < 0) {
        perror(RED "listen" RESET);
        exit(EXIT_FAILURE);
    }
    printf(BLUE "\n=== Serveur démarré sur le port %d ===\n" RESET, PORT);
    printf(YELLOW "En attente de connexions...\n" RESET);

    while(1) {
        // Fonction qui permet d'accepter un client à tout moment
        int client_sock = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_size);
        if(client_sock < 0) {
            perror(RED "accept" RESET);
            continue;
        }

        printf(GREEN "\nNouveau client connecté.\n" RESET);
        // On alloue une place en mémoire pour le socket du client
        int *pclient = malloc(sizeof(int));
        *pclient = client_sock;
        pthread_t thread_id;
        // On crée un thread pour gérer le client, comme ça le server peut traiter d'autres clients aussi
        if(pthread_create(&thread_id, NULL, clientHandler, pclient) != 0) {
            perror(RED "pthread_create" RESET);
            close(client_sock);
            free(pclient);
        }

        // Cette ligne de code permet de detacher les resources allouer au thread une fois fini
        pthread_detach(thread_id);
    }

    // On ferme le fichier d'historique
    fclose(desc);
    // on clode le socket du server à la fin du programme
    close(server_socket);
    return 0;
}