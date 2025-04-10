#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <errno.h>
#include <string.h>
// gcc nodo.c -lpthread -o nodo

#define N 3 // Número de nodos

// Vectores compartidos
int vector_peticiones[N] = {0};
int vector_atendidas[N] = {0};
int msqid_nodos[N] = {0};

// Variables compartidas
int testigo = 0; // 0 para indicar que no hay testigo
int dentro = 0; // 0 para indicar que estas en la SC
int nodo = 0; // 'id' del nodo

// Semáforos
sem_t sem_vectores;
sem_t sem_vector_atendidas;
sem_t sem_testigo;
sem_t sem_dentro;

struct msgbuf {
    long mtype;
    int nodoID;
    long vector[N]; // Vector de atendidos
};

void *receptor(void *arg) {
    int *msqid = (int *)arg;
    struct msgbuf solicitud;
    size_t buf_length = sizeof(struct msgbuf) - sizeof(long); //tamaño de (mtype + vector + nodoID) - mtype

    while (1) {
        if(msgrcv(*msqid, (void *)&solicitud, buf_length, 2, 0) < 0){
            printf("Error con msgrcv: %s\n", strerror(errno));
            exit(1);
        }
        printf("Solicitud %lo recibida del nodo %d\n",solicitud.vector[solicitud.nodoID-1],solicitud.nodoID);
        fflush(stdout);

        sem_wait(&sem_vectores);
        vector_peticiones[solicitud.nodoID-1]=solicitud.vector[solicitud.nodoID-1];
        sem_post(&sem_vectores);
        
        sem_wait(&sem_dentro);
        if(testigo && (!dentro) && vector_atendidas[solicitud.nodoID-1]<vector_peticiones[solicitud.nodoID-1]){
            solicitud.mtype = 1;
            if(msgsnd(msqid_nodos[solicitud.nodoID-1], &solicitud, buf_length, IPC_NOWAIT) < 0){
                printf("Error con msgsnd: %s\n", strerror(errno));
                exit(1);
            }
            sem_wait(&sem_testigo);
            testigo=0;
            sem_post(&sem_testigo);

            printf("Token enviado al nodo %d\n",solicitud.nodoID);
            fflush(stdout);
        }
        sem_post(&sem_dentro);
    }

    return NULL;
}

int main(int argc, char *argv[]){
    if(argc != 2){
		printf("Uso: %s <Número de nodo>\n", argv[0]);
        return 1;
	}
    // Inicializar semáforos
    sem_init(&sem_vectores, 0, 1);
    sem_init(&sem_vector_atendidas, 0, 1);
    sem_init(&sem_testigo, 0, 1);
    sem_init(&sem_dentro, 0, 1);

    pthread_t hilo_receptor;
    struct msgbuf mensaje;
    size_t msgsz = sizeof(struct msgbuf) - sizeof(long); //tamaño de (mtype + vector + nodoID) - mtype
    nodo = atoi(argv[1]);
    long mi_peticion = 0;

    // Intentar obtener el token
    int msqid_token = msgget(1069, 0666); // Cola de mensajes del token
    if (msgrcv(msqid_token, &mensaje, msgsz, 1, IPC_NOWAIT) < 0) {
        if (errno == ENOMSG) {
            printf("No hay token.\n");
            testigo = 0;
        } else {
            perror("msgrcv");
            return -1;
        }
    }else{
        printf("Tengo el token.\n");
        testigo = 1;
    }

    // Almacena los ids
    for(int i=0;i<N;i++) msqid_nodos[i] = msgget(1069+i+1, 0666); // Colas de mensajes de los nodos

    // Lanzar el hilo del recibidor de solicitudes
    int hilo_recibidor;
    if ((hilo_recibidor=pthread_create(&hilo_receptor, NULL, receptor, &msqid_nodos[nodo-1])) < 0) printf("Error con pthread_create: %s\n", strerror(errno));

    // Bucle del nodo
    while(1){
        printf("[NODO %d] Pulsa ENTER para intentar entrar a la sección critica\n", nodo);
        fflush(stdout);
        while(!getchar());

        printf("[NODO %d] Intentando entrar a la SC...\n", nodo);
        fflush(stdout);
        if(!testigo){
            mensaje.mtype = 2;
            mensaje.vector[nodo-1] = ++mi_peticion;
            mensaje.nodoID = nodo;
            for(int i=0;i<N;i++){
                if(i+1 == nodo) continue;
                if (msgsnd(msqid_nodos[i], &mensaje, msgsz, IPC_NOWAIT) < 0){
                    printf("Error con msgsnd: %s\n", strerror(errno));
                    return -1;
                }
                else printf("Solicitud enviada al nodo %d.\n",i+1);
            }
            printf("[NODO %d] Esperando recibir el testigo..\n", nodo);
            if (msgrcv(msqid_nodos[nodo-1], &mensaje, msgsz, 1, 0) < 0){
                printf("Error con msgrcv: %s\n", strerror(errno));
                return -1;
            }
            sem_wait(&sem_testigo);
            testigo=1;
            sem_post(&sem_testigo);
        }
        sem_wait(&sem_dentro);
        dentro = 1;
        sem_post(&sem_dentro);

        printf("[NODO %d] En la seccion critica, pulsa ENTER para salir\n", nodo);
        fflush(stdout);
        while(!getchar());
        mensaje.vector[nodo]=mi_peticion;

        sem_wait(&sem_dentro);
        dentro = 0;
        sem_post(&sem_dentro);

        // Generar un índice aleatorio
        srand(time(0)); // Inicializar el generador de números aleatorios
        int k = rand() % N; // Generar un índice aleatorio entre 0 y nro de nodos
        int enviado = 1; 

        sem_wait(&sem_vectores);
        for(int i=k;i<N && enviado;i++){
            if(vector_atendidas[i]<vector_peticiones[i]){
                mensaje.mtype = 1;
                if (msgsnd(msqid_nodos[i], &mensaje, msgsz, IPC_NOWAIT) < 0)
                    printf("Error con msgsnd: %s\n", strerror(errno));
                sem_wait(&sem_testigo);
                testigo=0;
                sem_post(&sem_testigo);
                enviado = 0;
            }
        }
        for(int i=0;i<k && enviado;i++){
            if(vector_atendidas[i]<vector_peticiones[i]){
                mensaje.mtype = 1;
                if (msgsnd(msqid_nodos[i], &mensaje, msgsz, IPC_NOWAIT) < 0)
                    printf("Error con msgsnd: %s\n", strerror(errno));
                sem_wait(&sem_testigo);
                testigo=0;
                sem_post(&sem_testigo);
                enviado = 0;  
            }
        }
        sem_post(&sem_vectores);

        printf("[NODO %d] Fuera de la sección crítica.\n", nodo);
        fflush(stdout);
    }
    if(pthread_join(hilo_recibidor,NULL) != 0) perror("Error al unirse el thread");
    return 0;
}
