/* gcc ricart_agrawala.c -o ricart_agrawala -lpthread -lm

./ricart_agrawala 1234
./ricart_agrawala 5678
./ricart_agrawala 91011
 */


 #include <stdio.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <time.h>
 #include <math.h>
 #include <sys/msg.h>
 #include <sys/ipc.h>
 #include <pthread.h>
 #include <semaphore.h>
 
 #define N 3  // Número de procesos
 
 typedef struct {
     long mtype;
     int idOrigen;
     int ticket;
 } msg;
 
 key_t clave;
 int miID, idNodos[N - 1];
 int ticket = 0, minTicket = 0, quieroEntrar = 0, confirmaciones = 0;
 int colaPendientes[N - 1], pendientes = 0;
 sem_t entradaSC;
 pthread_t hilo;
 
 // ----- Funciones que vamos a usar -----
 void* receive();
 void inicializarBuzonSem();
 void sendSol();
 void sendConfirm(int destinatario);
 void enterSC();
 void exitSC();
 
 int main(int argc, char* argv[]) {
     if (argc < 2) {
         printf("Uso: %s <clave_buzon>\n", argv[0]);
         return -1;
     }
 
     clave = atoi(argv[1]);

     // -- Inicializamos el buzón y el semáforo de paso --
     inicializarBuzonSem();
 

     // -- Recogemos los ID de buzones de los otros procesos --
     printf("Ingrese los ID de los buzones de los otros procesos:\n");
     for (int i = 0; i < N - 1; i++) {
         printf("ID nodo %d: ", i);
         scanf("%d", &idNodos[i]);
     }
     
     // -- Creamos el hilo para recibir mensajes --
     pthread_create(&hilo, NULL, receive, NULL);
     printf("Hilo creado correctamente.\n");


     printf("Nodo inicializado. Presione 'n' para solicitar acceso a SC.\n");
 
     while (1) {
         while (getchar() != 'n');  // Espera activación
 
         sendSol();
         sem_wait(&entradaSC); // Espera a entrar en la sección crítica
         enterSC(); // Entro en la sección critica
         while (getchar() != 's');  // Espera salida
         exitSC(); 
     }
 
     return 0;
 }
 
 
 void inicializarBuzonSem() {
     miID = msgget(clave, IPC_CREAT | 0777);
     if (miID == -1) {
         perror("Error creando el buzón");
         exit(EXIT_FAILURE);
     }
     printf("Buzón creado con ID %d\n", miID);
     sem_init(&entradaSC, 0, 0);
 }
 
 void sendSol() {
     msg mensaje; 
     srand(time(NULL)); 
     ticket = minTicket + (rand() % 100 + 1);  // Generar ticket único aleatorio entre minTicket y (minTicket + 100)
     quieroEntrar = 1; // Indicar que quiero entrar en la sección crítica
     confirmaciones = 0; // Reiniciar confirmaciones
 
     mensaje.mtype = 1; // Solicitud de entrada
     mensaje.idOrigen = miID; // ID del nodo que envía la solicitud
     mensaje.ticket = ticket; // Ticket del nodo
 
    // -- Enviar solicitud a todos los nodos --
     for (int i = 0; i < N - 1; i++) {
         if (msgsnd(idNodos[i], &mensaje, sizeof(msg) - sizeof(long), 0) == -1) {
             perror("Error enviando solicitud");
         }
     }
     printf("Solicitudes enviadas con ticket %d.\n", ticket);
 }
 

 void sendConfirm(int destinatario) {
     msg mensaje;
     mensaje.mtype = 2; // Confirmación
     mensaje.idOrigen = miID; // ID del nodo que envía la confirmación
     mensaje.ticket = 0; // No se necesita el ticket en la confirmación
 
    // -- Enviar confirmación al nodo destinatario --
     if (msgsnd(destinatario, &mensaje, sizeof(msg) - sizeof(long), 0) == -1) {
         perror("Error enviando confirmación");
     } else {
         printf("Confirmación enviada a %d.\n", destinatario);
     }
 }
 
 void* receive() {
     msg mensaje;
     while (1) {
         if (msgrcv(miID, &mensaje, sizeof(msg) - sizeof(long), 0, 0) == -1) {
             perror("Error recibiendo mensaje");
             continue;
         }
 
         if (mensaje.mtype == 1) {  // Solicitud de entrada
             printf("Solicitud recibida de %d con ticket %d.\n", mensaje.idOrigen, mensaje.ticket);
             minTicket = fmax(minTicket, mensaje.ticket);
 
             if (!quieroEntrar || mensaje.ticket < ticket || (mensaje.ticket == ticket && mensaje.idOrigen < miID)) {
                 sendConfirm(mensaje.idOrigen);
             } else {
                 colaPendientes[pendientes++] = mensaje.idOrigen;
             }
         } else if (mensaje.mtype == 2) {  // Confirmación
             printf("Confirmación recibida de %d.\n", mensaje.idOrigen);
             confirmaciones++;
             if (confirmaciones == N - 1) {
                 sem_post(&entradaSC);
             }
         }
     }
 }
 
 // -- Entrar en la sección crítica (La sección critica)--
 void enterSC() {
     printf("Entrando en Sección Crítica...\n");
     sleep(2);
 }
 
 // -- Salir de la sección crítica --
 void exitSC() {
     printf("Saliendo de Sección Crítica...\n");
     quieroEntrar = 0; // Reiniciar la variable
 
     msg mensaje;
     mensaje.mtype = 2; // Confirmación
     mensaje.idOrigen = miID; // ID del nodo que envía la confirmación
     mensaje.ticket = 0;
 
     // -- Enviar confirmaciones pendientes --
     for (int i = 0; i < pendientes; i++) {
         sendConfirm(colaPendientes[i]);
     }
     pendientes = 0;
 }