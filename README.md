# Projet 1 : Chat en Temps Réel avec Sockets en C
![Illustration du Chat en Temps Réel](chat_online_image.png)
Le projet consiste à développer une application de chat en temps réel en utilisant les sockets
réseaux en langage C. L'application permettra à plusieurs utilisateurs de se connecter à un serveur central
et d'échanger des messages en temps réel.

## 📁 Structure du projet

```
.
├── client.c       # Code source du client 
├── server.c      # Code source du serveur
├── README.md      # Le fichier README
```

## ⚙️ Prérequis

Avant de compiler, assurez-vous d’avoir importé ou installé:

- Un compilateur C (gcc)
- Les bibliothèques suivantes :
  - `pthread` 
  - `stdlib.h`
  - `string.h`
  - `unistd.h`
  - `arpa/inet.h`
  - `pthread.h`
  - `netinet/in.h`
  

## 🔧 Compilation

### Serveur

```bash
gcc server.c -o server
```

### Client

```bash
gcc client.c -o client
```

## ▶️ Exécution

### Lancer le serveur

```bash
./server
```

Par défaut, le serveur écoute sur le port 8080.

### Lancer un client

```bash
./client
```

Chaque client se connecte au serveur, entre un pseudo, et rejoint un canal.



## 📝 Remarques

- Le client et le serveur doivent être lancés sur la même machine ou sur des machines ayant une connexion réseau entre elles.
- Vous pouvez modifier l’adresse IP et le port dans les fichiers source si nécessaire.

## 📤 Auteurs

- Assan Kane
- Assadick annadif abderahim
- Youssouf ali rozi
