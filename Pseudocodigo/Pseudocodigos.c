
//MAIN
main(){

    for(int i=0; i<Nprocesos; i++){
        crearBuzon();
        crearReceiver();
        crearProceso();
    }

}




//RECEIVER
receiver(){

    recibirMensaje();

    if(es_confirmacion){

        confirmacion++;

        if(confirmacion==num_total){

            despertarFuncionPrincipal(); //para que entre a la SC

        }

    }else if(es_quieroSC){

        almacenarMensajes(&peticiones);

        if(mas_de_una_peticion){

            for(int i=0; i<NumPeticiones; i++){

                prioridad_aux[i] = peticiones[i].prioridad;
    
            }
            
            ordenarPrioridades(&prioridad_aux, &peticiones); //ordena las peticiones
                                                            //DENTRO DE LA FUNCION
        }

    }else{
        enviarConfirmacion();
    }
}