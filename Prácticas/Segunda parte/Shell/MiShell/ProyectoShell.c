/*------------------------------------------------------------------------------
Pablo Julián Campoy Fernández
CURSO 2022-2023
Proyecto Shell de UNIX. Sistemas Operativos
Doble grado  en Ingeniería Informatica y Matemáticas


Algunas secciones estan inspiradas en ejercicios publicados en el libro
"Fundamentos de Sistemas Operativos", Silberschatz et al.

Para compilar este programa: gcc ProyectoShell.c ApoyoTareas.c -o MiShell
Para ejecutar este programa: ./MiShell
Para salir del programa en ejecucion, pulsar Control+D
------------------------------------------------------------------------------*/

#include "ApoyoTareas.h" // Cabecera del modulo de apoyo ApoyoTareas.c
 
#define MAX_LINE 256 // 256 caracteres por linea para cada comando es suficiente
#include <string.h>  // Para comparar cadenas de cars. (a partir de la tarea 2)

// --------------------------------------------
//                     MAIN          
// --------------------------------------------

job *tareas; // Lista de tareas


// MANEJADOR  DE SENALES ------------------------------------------------------------------------------------------
void manejador(int signal){ //Signal será siempre 17, pues corresponderá a SIGCHLD (podemos comprobarlo usando kill -l)
  block_SIGCHLD(); // Evitamos las posibles modificaciones sobre la lista cuando llamemos al manejador
  job *item;
  int status;
  int info;
  int pid_wait = 0;
  enum status status_res;

  //Recorremos la lista de tareas
  for(int i = 1; i <= list_size(tareas); i++){ // La lista debe empezar en 1
    item = get_item_bypos(tareas, i); // Sacamos cada item de la lista de tareas
    pid_wait = waitpid(item->pgid, &status, WUNTRACED | WNOHANG | WCONTINUED); //Esperamos a cada uno de los comandos de la lista
    //item es una estructura de tipo job (observar cómo está implementada la estructura)
    
    if(pid_wait == item->pgid){ // Si waitpid recoge un proceso:
      status_res = analyze_status(status, &info);

      if(status_res == SUSPENDIDO){
        printf("Comando %s ejecutado en segundo plano con PID %d ha suspendido su ejecucion.\n", item->command, item->pgid);
        item->ground = DETENIDO;  // Actualizamos el estado del elemento en la lista
      }else if((status_res == FINALIZADO) || (status_res == SENALIZADO)){ // SENALIZADO HACE LO MISMO
        printf("Comando %s ejecutado en segundo plano con PID %d ha concluido.\n", item->command, item->pgid);
        delete_job(tareas,item); // Eliminamos la tarea finalizada de la lista
      }else if(status_res == REANUDADO){
        item->ground = SEGUNDOPLANO;
      }
    }
  }
  unblock_SIGCHLD();
}

int main(void)
{
      char inputBuffer[MAX_LINE]; // Búfer que alberga el comando introducido
                                  // Se trata de un array de caracteres que puede albergar hasta un máximo de 256
     
      int background;         // Vale 1 si el comando introducido finaliza con '&' --> está en segundo plano
     
      char *args[MAX_LINE/2]; // La linea de comandos (de 256 cars.) tiene 128 argumentos como max
                              // Array de punteros a caracteres -> punteros al inputBuffer donde comienza cada uno
                              // Variables de utilidad:
      int pid_fork, pid_wait; // pid para el proceso creado y esperado
      int status;             // Estado que devuelve la funcion wait
      enum status status_res; // Estado procesado por analyze_status()
      int info;               // Informacion procesada por analyze_status()
     
     
     
      job *item;  
     

      ignore_terminal_signals(); // Para ignorar señales del terminal.
      signal(SIGCHLD, manejador);
      tareas = new_list("tareas");
     

      while (1) // El programa termina cuando se pulsa Control+D dentro de get_command()
      {  
        printf("COMANDO-> "); 
        fflush(stdout); 
       
        get_command(inputBuffer, MAX_LINE, args, &background); // Obtener el proximo comando
        
        /*
        args[0] => va a ser siempre el comando
        args[1] => se guarda lo siguiente
       
        &background se va a modificar dependiendo si es primer plano o segundo plano
        */

        if (args[0]==NULL) continue; // Si se introduce un comando vacio, no hacemos nada
   

        if(!strcmp(args[0],"cd")){ 
          int path = chdir(args[1]);

          if(path == -1){
            printf("\nLa ruta %s no es valida.\n", args[1]);
          }

          continue;
        }

        if(!strcmp(args[0],"logout")){ 
          exit(0);
        }

        if(!strcmp(args[0],"jobs")){ 
          block_SIGCHLD();
          print_job_list(tareas);
          unblock_SIGCHLD();
          continue;
        }
        if(!strcmp(args[0], "fg")){
          block_SIGCHLD();
          int posicion = 1;
          if (args[1] != NULL){
            posicion = atoi(args[1]);
          }
          item = get_item_bypos(tareas, posicion);    
          if(item != NULL){
            if(item->ground!=PRIMERPLANO){
              item->ground = PRIMERPLANO;
              set_terminal(item->pgid);
              killpg(item->pgid, SIGCONT);
              waitpid(item->pgid,&status,WUNTRACED);
     
              set_terminal(getpid());
              status_res=analyze_status(status,&info);

              if(status_res==SUSPENDIDO){
                item->ground=DETENIDO;
              }else{
                delete_job(tareas,item);
              }
              printf("Comando %s ejecutado en primer plano con PID %d. Estado suspendido. Info: %d. \n", args[0], pid_fork, info);
            }else{
              printf("Comando %s con PID %d no estaba suspendido ni en segundo plano. \n",item->command, item->pgid);
            }
          }else{
            printf("Error en la posición introducida. \n");
          }
          unblock_SIGCHLD();
        }
   
        if(!strcmp(args[0],"bg")){ 
          block_SIGCHLD();
          int posicion = 1; 

          if(args[1] != NULL){
            posicion = atoi(args[1]); // Esta función se encarga de pasar de String a Int
          }
   
          item = get_item_bypos(tareas, posicion);
   
          if(item == NULL ){
            printf("La posición introducida no es correcta. \n");
          }else if((item != NULL) && (item->ground == DETENIDO)){
            item->ground = SEGUNDOPLANO;
            killpg(item->pgid, SIGCONT); // SIGCONT = 19
          }
   
          unblock_SIGCHLD();
          continue;
        }
     /* Los pasos a seguir a partir de aqu’, son:
       (1) Genera un proceso hijo con fork()
       (2) El proceso hijo invocar‡ a execvp()
       (3) El proceso padre esperar‡ si background es 0; de lo contrario, "continue" 
       (4) El Shell muestra el mensaje de estado del comando procesado 
       (5) El bucle regresa a la funci—n get_command()
    */

        pid_fork = fork();
   
        if(pid_fork > 0) { // Padre
          if(background == 0){ // Primer plano
            pid_wait = waitpid(pid_fork, &status, WUNTRACED);  // status lo pasamos por referencia al tratarse de un puntero y que lo vamos a cambiar
            set_terminal(getpid()); // Recuperamos el terminal con el PID del proceso actual (padre), ya que es en primer plano
            status_res = analyze_status(status, &info); // Aquí solo va a leer status, mientras que info es un puntero y puede modificarlo
   
            if(status_res == SUSPENDIDO){
              block_SIGCHLD();
              item = new_job(pid_fork, args[0], DETENIDO);
              add_job(tareas, item);
              printf("Comando %s ejecutado en primer plano con PID %d. Estado %s. Info: %d. \n", args[0], pid_fork, status_strings[status_res], info);
              unblock_SIGCHLD();
            }else if(status_res == FINALIZADO){
              if(info!=255){ // Para realizar el control de errores usamos que, siempre que se produzca uno, info vale 255. Esto lo hacemos porque no queremos imprimir que un comando erróneo ha terminado su ejecución.
                printf("Comando %s ejecutado en primer plano con PID %d. Estado %s. Info: %d. \n", args[0], pid_fork, status_strings[status_res], info);
              }
            }
          }else{ // Segundo plano
            block_SIGCHLD();
            item = new_job(pid_fork, args[0], SEGUNDOPLANO);
            add_job(tareas, item);
            printf("Comando %s ejecutado en segundo plano con PID %d. \n", args[0], pid_fork);
            unblock_SIGCHLD();
          }
        }else if(pid_fork == 0){ // Proceso hijo (es donde se lanzan los comandos)
          new_process_group(getpid()); // Se hace para que el hijo sea independiente al padre. Si no lo hicieramos, al hacer Ctrl+Z al hijo, afectaria también al Shell
          if(background == 0){
            set_terminal(getpid()); // El hijo toma el terminal si se encuentra en primer plano
          }
          restore_terminal_signals();  
          execvp(args[0], args); // args[0] indica el comando que le queremos pasar, mientras que args lo emplea 
          printf("Error. Comando %s no encontrado \n", args[0]);
          exit(-1); // Si execvp no se ejecuta, pasa a este comando, que devuelve -1, ya que se trataría de un error
        }else{ // Error
          printf("Error de fork.\n");
          fflush(stdout); // siempre que se hace printf se tiene que hacer
          exit(EXIT_FAILURE);
        }
      }// end while
} 

