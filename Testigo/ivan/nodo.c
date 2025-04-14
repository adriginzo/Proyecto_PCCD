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

// Vectores compartidos: 
// Estos vectores serán utilizados para llevar un registro de las peticiones y de las solicitudes atendidas.
int vector_peticiones[N] = {0};
int vector_atendidas[N] = {0};
int msqid_nodos[N] = {0}; // Array para almacenar los IDs de las colas de mensajes de los nodos

// Variables compartidas:
int testigo = 0; // Variable que indica si el nodo tiene el testigo (0 significa que no lo tiene)
int dentro = 0; // Indica si el nodo está dentro de la sección crítica
int nodo = 0; // ID del nodo (será pasado como parámetro en la ejecución)

// Semáforos: Estos se utilizan para controlar el acceso a las variables compartidas y para la sincronización.
sem_t sem_vectores;
sem_t sem_vector_atendidas;
sem_t sem_testigo;
sem_t sem_dentro;

// Estructura que representa el mensaje que se enviará a través de la cola de mensajes
struct msgbuf {
    long mtype; // Tipo de mensaje
    int nodoID; // ID del nodo que envía el mensaje
    long vector[N]; // Vector de atendidos
};

// Función del receptor (hilo receptor)
void *receptor(void *arg) {
    int *msqid = (int *)arg; // ID de la cola de mensajes del nodo
    struct msgbuf solicitud; // Estructura que almacenará el mensaje recibido
    size_t buf_length = sizeof(struct msgbuf) - sizeof(long); // Calculamos el tamaño del mensaje a recibir (excluyendo 'mtype')

    while (1) {
        // Recibir el mensaje (debe ser tipo 2)
        if (msgrcv(*msqid, (void *)&solicitud, buf_length, 2, 0) < 0) {
            printf("Error con msgrcv: %s\n", strerror(errno)); // Si hay error al recibir el mensaje
            exit(1); // Termina la ejecución si hay error
        }
        printf("Solicitud %lo recibida del nodo %d\n", solicitud.vector[solicitud.nodoID - 1], solicitud.nodoID);
        fflush(stdout);

        // Actualizar el vector de peticiones con la nueva solicitud
        sem_wait(&sem_vectores); // Esperar para acceder al vector de peticiones
        vector_peticiones[solicitud.nodoID - 1] = solicitud.vector[solicitud.nodoID - 1];
        sem_post(&sem_vectores); // Liberar el acceso al vector

        // Verificar si el nodo tiene el testigo y si no está en la sección crítica
        sem_wait(&sem_dentro);
        if (testigo && (!dentro) && vector_atendidas[solicitud.nodoID - 1] < vector_peticiones[solicitud.nodoID - 1]) {
            solicitud.mtype = 1; // Cambiar el tipo de mensaje para indicar que el nodo debe recibir el testigo
            if (msgsnd(msqid_nodos[solicitud.nodoID - 1], &solicitud, buf_length, IPC_NOWAIT) < 0) {
                printf("Error con msgsnd: %s\n", strerror(errno)); // Si ocurre un error al enviar el mensaje
                exit(1); // Termina la ejecución si hay error
            }
            sem_wait(&sem_testigo); // Esperar para modificar la variable 'testigo'
            testigo = 0; // El testigo es entregado
            sem_post(&sem_testigo);

            printf("Token enviado al nodo %d\n", solicitud.nodoID);
            fflush(stdout);
        }
        sem_post(&sem_dentro); // Liberar el acceso a la variable 'dentro'
    }

    return NULL; // Termina el hilo receptor
}

int main(int argc, char *argv[]) {
    // Verificar que el número de nodos ha sido proporcionado como argumento
    if (argc != 2) {
        printf("Uso: %s <Número de nodo>\n", argv[0]);
        return 1; // Salir si el número de argumentos es incorrecto
    }

    // Inicializar semáforos
    sem_init(&sem_vectores, 0, 1); // Inicializar el semáforo de acceso a 'vector_peticiones'
    sem_init(&sem_vector_atendidas, 0, 1); // Inicializar el semáforo de acceso a 'vector_atendidas'
    sem_init(&sem_testigo, 0, 1); // Inicializar el semáforo de acceso a 'testigo'
    sem_init(&sem_dentro, 0, 1); // Inicializar el semáforo de acceso a 'dentro'

    pthread_t hilo_receptor; // Variable para el hilo receptor
    struct msgbuf mensaje; // Estructura que se usará para enviar mensajes
    size_t msgsz = sizeof(struct msgbuf) - sizeof(long); // Tamaño del mensaje
    nodo = atoi(argv[1]); // Obtener el ID del nodo desde los argumentos de la línea de comandos
    long mi_peticion = 0; // Inicializar el número de la petición del nodo

    // Intentar obtener el testigo de la cola de mensajes
    int msqid_token = msgget(1069, 0666 | IPC_CREAT); // Crear o abrir la cola de mensajes para el token
    if (msgrcv(msqid_token, &mensaje, msgsz, 1, IPC_NOWAIT) < 0) {
        if (errno == ENOMSG) {
            printf("No hay token.\n");
            testigo = 0; // Si no hay mensaje, no se tiene el testigo
        } else {
            perror("msgrcv");
            return -1; // Terminar si hay error al recibir el mensaje
        }
    } else {
        printf("Tengo el token.\n");
        testigo = 1; // Si se recibe el token, el nodo tiene el testigo
    }

    // Crear las colas de mensajes para cada nodo
    for (int i = 0; i < N; i++) {
        msqid_nodos[i] = msgget(1069 + i + 1, 0666 | IPC_CREAT); // Crear una cola de mensajes por nodo
        if (msqid_nodos[i] == -1) {
            perror("Error al crear cola de mensajes");
            exit(1); // Si hay un error al crear la cola de mensajes, terminar
        }
    }

    // Lanzar el hilo receptor
    int hilo_recibidor;
    if ((hilo_recibidor = pthread_create(&hilo_receptor, NULL, receptor, &msqid_nodos[nodo - 1])) < 0) {
        printf("Error con pthread_create: %s\n", strerror(errno)); // Si hay un error al crear el hilo, terminar
        return -1;
    }

    // Bucle principal del nodo
    while (1) {
        // Solicitar al usuario que presione Enter para intentar entrar en la sección crítica
        printf("[NODO %d] Pulsa ENTER para intentar entrar a la sección crítica\n", nodo);
        fflush(stdout);
        while (!getchar());

        printf("[NODO %d] Intentando entrar a la SC...\n", nodo);
        fflush(stdout);

        // Si no se tiene el testigo, solicitarlo enviando un mensaje a los demás nodos
        if (!testigo) {
            mensaje.mtype = 2; // Tipo de mensaje para solicitud de testigo
            mensaje.vector[nodo - 1] = ++mi_peticion; // Incrementar el número de la petición del nodo
            mensaje.nodoID = nodo; // Establecer el ID del nodo en el mensaje

            // Enviar solicitud a todos los demás nodos
            for (int i = 0; i < N; i++) {
                if (i + 1 == nodo) continue; // No enviar al propio nodo
                if (msgsnd(msqid_nodos[i], &mensaje, msgsz, IPC_NOWAIT) < 0) {
                    printf("Error con msgsnd: %s\n", strerror(errno)); // Si hay error al enviar el mensaje
                    return -1; // Terminar si hay error
                } else {
                    printf("Solicitud enviada al nodo %d.\n", i + 1); // Confirmar envío de solicitud
                }
            }
            printf("[NODO %d] Esperando recibir el testigo..\n", nodo);
            if (msgrcv(msqid_nodos[nodo - 1], &mensaje, msgsz, 1, 0) < 0) {
                printf("Error con msgrcv: %s\n", strerror(errno)); // Si hay error al recibir el testigo
                return -1;
            }
            sem_wait(&sem_testigo); // Esperar para modificar la variable 'testigo'
            testigo = 1; // Obtener el testigo
            sem_post(&sem_testigo);
        }

        // Indicar que el nodo entra en la sección crítica
        sem_wait(&sem_dentro);
        dentro = 1; // Nodo entra a la sección crítica
        sem_post(&sem_dentro);

        printf("[NODO %d] En la sección crítica, pulsa ENTER para salir\n", nodo);
        fflush(stdout);
        while (!getchar()); // Esperar que el usuario presione Enter para salir

        mensaje.vector[nodo] = mi_peticion; // Actualizar el vector de atendidos

        sem_wait(&sem_dentro);
        dentro = 0; // Nodo sale de la sección crítica
        sem_post(&sem_dentro);

        // Enviar mensajes a los nodos correspondientes
        srand(time(0)); // Inicializar el generador de números aleatorios
        int k = rand() % N; // Elegir un nodo aleatorio
        int enviado = 1;

        sem_wait(&sem_vectores); // Esperar para modificar los vectores
        // Recorrer los nodos desde el índice aleatorio
        for (int i = k; i < N && enviado; i++) {
            if (vector_atendidas[i] < vector_peticiones[i]) {
                mensaje.mtype = 1; // Cambiar el tipo de mensaje a solicitud de testigo
                if (msgsnd(msqid_nodos[i], &mensaje, msgsz, IPC_NOWAIT) < 0) {
                    printf("Error con msgsnd: %s\n", strerror(errno)); // Si hay error al enviar el mensaje
                }
                sem_wait(&sem_testigo);
                testigo = 0; // El testigo se entrega
                sem_post(&sem_testigo);
                enviado = 0; // Indicar que ya se ha enviado el testigo
            }
        }
        // Recorrer desde el inicio si es necesario
        for (int i = 0; i < k && enviado; i++) {
            if (vector_atendidas[i] < vector_peticiones[i]) {
                mensaje.mtype = 1; // Cambiar el tipo de mensaje a solicitud de testigo
                if (msgsnd(msqid_nodos[i], &mensaje, msgsz, IPC_NOWAIT) < 0) {
                    printf("Error con msgsnd: %s\n", strerror(errno)); // Si hay error al enviar el mensaje
                }
                sem_wait(&sem_testigo);
                testigo = 0; // El testigo se entrega
                sem_post(&sem_testigo);
                enviado = 0; // Indicar que ya se ha enviado el testigo
            }
        }
        sem_post(&sem_vectores); // Liberar el acceso a los vectores

        printf("[NODO %d] Fuera de la sección crítica.\n", nodo); // Indicar que el nodo ha salido de la sección crítica
        fflush(stdout);
    }

    // Esperar a que termine el hilo receptor antes de finalizar
    if (pthread_join(hilo_recibidor, NULL) != 0) {
        perror("Error al unirse el thread"); // Si hay error al esperar al hilo
    }
    return 0; // Terminar el programa
}
