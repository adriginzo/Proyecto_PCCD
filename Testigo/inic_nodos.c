#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <errno.h>
#include <string.h>
// gcc inic_nodos.c -lpthread -o inic_nodos

#define N 3 // Número de nodos

struct msgbuf {
    long mtype;
    int nodoID;
    long vector[N]; // Vector de atendidos
};

int main() {
    int msqid[N+1];
    int msgflg = IPC_CREAT | IPC_EXCL;
    struct msgbuf sbuf;
    size_t msgsz = sizeof(struct msgbuf) - sizeof(long); //tamaño de (mtype + vector + nodoID) - mtype

    //Inicializar la cola de mensajes del token
    if ((msqid[0] = msgget(1069, msgflg | 0666)) < 0)
        printf("Error con msgget: %s\n", strerror(errno));

    // El 'token' es un número entero y el vector de solicitudes atendidas es de tipo long
    sbuf.mtype = 1; // Tipo de mensaje del token
    for(int i=0;i<N;i++)
        sbuf.vector[0]=0;    //inicializar las solicitudes a 0
    
    // Enviar el token
    if (msgsnd(msqid[0], &sbuf, msgsz, IPC_NOWAIT) < 0)
                    printf("Error con msgsnd: %s\n", strerror(errno));
    printf("Token enviado\n");
    
    // Inicializar la cola de mensajes de los nodos
    for (int i=1; i<N+1; i++)
        if ((msqid[i] = msgget(1069+i, msgflg | 0666)) < 0) printf("Error con msgget: %s\n", strerror(errno));

    printf("Pulsa enter para terminar las colas de mensaje\n");
	while(!getchar());

    // Apagar las colas de mensajes del token y los nodos
    for (int i = 0; i < N+1; i++)
        if (msgctl(msqid[i], IPC_RMID, NULL) == -1) printf("Error con msgctl: %s\n", strerror(errno));

    printf("Colas de mensajes apagadas.\n");
    return 0;
}
