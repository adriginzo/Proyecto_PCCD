//gcc ricart_agrawala.c -o t -lpthread -lm

//INCLUDES
//==============================================================================================
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <time.h>

#include <math.h>

#include <sys/msg.h>    // Msgget
#include <sys/ipc.h>    // Msgget
#include <pthread.h>    // Hilos
#include <semaphore.h>  // Semaforos
//==============================================================================================



#define nUsuarios 3

typedef struct{     
    int mtype;
    int idO;
    float ticket;
}msj;


sem_t entradaSC;
key_t clave; //Se utilizara en el ftok
float ticket, minTicket = 0; //Explico en clase algo del minimo del ticket
int pend = 0, entrarSC = 0, colaN[nUsuarios-1];
int miID;

float quienEsMas(float, float);
void * recibir();
void  oficinaCorreos();


//MAIN
//=========================================================================================================
int main(int argc, char* argv[]){

    msj msjaEnviar;
    int idBuzonNodos[nUsuarios -1];
    pthread_t idHilo;
    

    clave = ftok("/tmp",atoi(argv[1])); //crear la clave del buzon

    oficinaCorreos(); //Aqui xa temos o semaforo mais o buzon 

    for (int i = 0; i < (nUsuarios -1); i++){
        printf("Pon el id del BUZON del los otros procesos: ");  // comunicacion cos outros procesos
        scanf("%i", &idBuzonNodos[i]);
    }


    //Creoamos un hilo que sexa pa recibir as mensaxes
    if(pthread_create(&idHilo, NULL, recibir, NULL)==-1){

        printf("Error en creacion de hilo\n");
        return EXIT_FAILURE;
    }

    printf("Hilo creado perfectamente\n");



    while(1){
        sleep(3);
        fflush (stdin);

        printf("Si quieres entrar a la seccion critica pulsa <e>: ");

        while (getchar()!= 'e');


        //===========================MAGIA DE ALEATORIEDAD=============================
        srand((unsigned)time(NULL)); //Inicializmos a semilla da funcion rand co tempo actual 
        ticket = minTicket + (float)((rand()%1000)/1000.0f);//Le suma numeros decimales aleatorios a minTicket
        //=============================================================================


        //Quiere entrar y definimos en mensaje a enviar al resto de los nodos
        entrarSC  = 1;
        msjaEnviar.idO = miID;
        msjaEnviar.mtype = 1;
        msjaEnviar.ticket = ticket;



        //Mandamos os mensaxes aos outros nodos para poder entrar a SC

        for(int  i = 0; i <(nUsuarios - 1); i++){//dW?

            if(msgsnd(idBuzonNodos[i], &msjaEnviar, sizeof(msjaEnviar),0)==-1){
                printf("error en el envio\n");
            }

        }
        


        printf("Msj enviados\n ");

        //Hacemos wait para que cuano llegue aqui se mima esperando por los msj de los otros nodos
        sem_wait(&entradaSC);
        //Asi que se reciben todas las respues la funcion recibir despierta a este hilo y entra en la SC

        printf("Se han recibido todas las respuestas\n Entramos en SC\n");

        printf("Para salir de la seccion critica escribe <s>: ");
        while(getchar() != 's');

        printf("Hemosalido\n\n");

        msjaEnviar.mtype=2;
        msjaEnviar.idO = miID;

        entrarSC = 0;

        //Como mando o algorimo ahora ocuparemonos das peticions que chegaron cando estabamos na SC
        printf("Fancendo os Deberes\n");


        for(int i = 0; i<pend; i++){//dW?

            if(msgsnd(idBuzonNodos[i], &msjaEnviar, sizeof(msjaEnviar),0)==-1){
                printf("error en el envio\n");
            }

        }

        printf("Deberes pendientes acabados\n");

        
    }



    return 0;
}
//=========================================================================================================





//OFICINACORREOS
//=========================================================================================================
void  oficinaCorreos(){

    miID = msgget(clave, IPC_CREAT|0777); // crear el buzon
    
    if(miID == -1){
        printf("Error en msgget\n");
        exit(EXIT_FAILURE);
    }

    printf("BUZON CON ID: %i\n", miID);

    if(sem_init(&entradaSC, 0 ,0)){
        printf("Error en senminit\n");
        exit(EXIT_FAILURE);
    }



    return;
}
//=========================================================================================================

//RECIBIR
//=========================================================================================================
void * recibir(){

    int nConf=0;
    msj out;
    msj in;


    while(1){


        if(msgrcv(miID,&in,sizeof(in),0 ,0)==-1){
            printf("Error al recibir msj\n");
        }else if(in.mtype==2){

            printf("Confirmacion de NODO recibida\n");
            nConf++;
            printf("%i\n", nConf);
            if ( nConf == (nUsuarios -1)){

                //Cuando recibimos todos los mensajes de tipo 2 quiere decir que nadie mas esta interesado y podemos entrar en SC
                sem_post(&entradaSC);
                nConf = 0;
            }

        }else if(entrarSC==1){
            
            //Cuando esta con entrar SC 1 queremos guardar todos los mensajes que se reciban por si otro nodo quiere entrar a la SC
            //Y posteriormente poder responderlos
            colaN[pend] = in.idO;
            pend ++;
            
        }else if((entrarSC == 0)||((ticket > in.ticket)||(ticket == in.ticket && miID > in.idO))){
            //Si NO quiero entrar a las seccion critica 
            //O
            //Si mi tiquet es mayor del que reccibi O si mi tiquet es eigual al que recibi pero mi ID es mayor

            //Escribo msj de que no voy a entrar a la seccion critica y dejo pasar a la otra

            minTicket = quienEsMas(minTicket, in.ticket);

            out.idO = miID;
            out.mtype = 2; //deixo pasar
            out.ticket = 0;


            if(msgsnd(in.idO, &out, sizeof(out),0)==-1){
                printf("error en el envio\n");
            }


        }


    }

}
//=========================================================================================================


//QUIENESMAS
//=========================================================================================================
float quienEsMas(float n1, float n2){
    if(n1>n2){
        return n1;   
    }else{
        return n2;
    }
}
//=========================================================================================================