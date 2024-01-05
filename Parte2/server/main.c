#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "../common/constants.h"
#include "operations.h"
#include "eventlist.h"
#include <stdbool.h>


#define MAX_SESSIONS 10  // Exemplo de limite de sessões

void parseMsg(char buf[TAMMSG],char atribts[5][MAX_RESERVATION_SIZE]);
void arrayXY(char *xys, size_t array[]);

// Function to set up the named pipe and start the server
int setup_named_pipe(const char *pipe_path) {
    // Attempt to create the named pipe
    if (mkfifo(pipe_path, 0666) < 0) {
        perror("mkfifo");
        return -1;
    }

    return 0;
}

bool active_sessions[MAX_SESSIONS] = {false};  // Rastrear sessões ativas

// Função modificada para gerar session_id
int generate_session_id() {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!active_sessions[i]) {
            active_sessions[i] = true;  // Marca a sessão como ativa
            return i;
        }
    }
    return -1;  // Retorna -1 se todas as sessões estiverem ativas
}

// Função para liberar session_id
void release_session_id(int session_id) {
    if (session_id >= 0 && session_id < MAX_SESSIONS) {
        active_sessions[session_id] = false;  // Marca a sessão como inativa
    }
}

// ---------------------- EX2 ----------------------------
// Define the global flag and the signal handler
volatile sig_atomic_t sigusr1_flag = 0;

void sigusr1_handler(int signum) {
    sigusr1_flag = 1;
}

void showEvents() {
  // Assuming you have a function to get the first event in your list
  Event *event = getFirstEvent();
  while (event != NULL) {
    printf("Event ID: %d, Name: %s\n", event->id, event->name);
    // Assuming you have a function to print the state of seats for an event
    printSeatStates(event);
    event = event->next;  // Move to the next event in the list
  }
}

void printSeatStates(Event *event) {
  // Iterate through the seats of the event and print their states
  for (int i = 0; i < event->numSeats; ++i) {
    printf("Seat %d: %s\n", i, (event->seats[i].isReserved ? "Reserved" : "Available"));
  }
}
// -------------------------------------------------------

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  // ---------------------------- EX2 -------------------------------------
  // Registering the SIGUSR1 signal handler before creating any threads
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sigusr1_handler;
  sigaction(SIGUSR1, &sa, NULL);
  // ----------------------------------------------------------------------

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  //TODO: Intialize server, create worker threads
  // Set up the named pipe
  if (setup_named_pipe(argv[1]) < 0) {
    fprintf(stderr, "Failed to set up the named pipe\n");
    return 1;
  }

  int server_fd = open(argv[1], O_RDONLY);
  if (server_fd < 0) {
    perror("open");
    return 1;
  }

  while (1) {
    //TODO: Read from pipe
    //TODO: Write new client to the producer-consumer buffer

    if (sigusr1_flag) {
      showEvents();
      sigusr1_flag = 0;
    }

    // Se todas as sessões estiverem ativas, bloquear até que uma seja liberada
    while (generate_session_id() == -1) {
      // Pode adicionar um sleep aqui para evitar uso excessivo de CPU
    }

    char buf[TAMMSG];
    
    char client_pipes[2][MAX_PATH_SIZE];  // Array to hold client's request and response pipe names

    //session_message msg;
    // Read from the server's named pipe
    ssize_t num_read = read(server_fd, buf, TAMMSG);

    if (num_read > 0) {
      char atribts[5][MAX_RESERVATION_SIZE];
      parseMsg(buf,atribts);

    
      int session_id = generate_session_id();  // Generate new session ID

      strcpy(client_pipes[0],atribts[1]);strcpy(client_pipes[1],atribts[2]);

      store_session_details(session_id, client_pipes[0], client_pipes[1]); // Store session details
      // Open the client's response pipe and send the session ID
      int client_response_fd = open(client_pipes[1], O_WRONLY);
      if (client_response_fd < 0) {
        perror("open - client response pipe");
        continue;
      }
      // Send the session ID to the client
      if (write(client_response_fd, &session_id, sizeof(session_id)) < 0) {
        perror("write - session ID to client response pipe");
      }
       

      /*if (!process_message(&msg)) {
        continue;  // Se a sessão foi encerrada, continua para a próxima iteração
      }*/
      close(client_response_fd);  // Close the client's response pipe
    } /*else if (num_read == 0) {
      // End of file, no data read, pipe was closed
      release_session_id();
      break;
    }*/ else {
      // An error occurred
      perror("read");
      break;
    }
    SessionNode *temp=SessionList();
    while(temp!=NULL){
      int request_pipe=open(temp->request_pipe,O_RDONLY);
      int response_pipe=open(temp->response_pipe,O_WRONLY);
      char buffer[TAMMSG];
      read(request_pipe,buffer,TAMMSG);
      char buffer1[TAMMSG];
      strcpy(buffer1,buffer);
      char atribts[5][MAX_RESERVATION_SIZE];
      parseMsg(buffer,atribts);

      char ret_msg[TAMMSG];
      switch(buffer1[8]){
        case '2':
          release_session_id(temp->session_id);
          free_Session(temp->session_id);
          break;
        case '3':
          ems_create((unsigned int) atoi(atribts[1]),(size_t) atoi(atribts[2]),(size_t) atoi(atribts[3]));
          break;
        case '4':
          
          size_t arrayX[MAX_RESERVATION_SIZE];
          size_t arrayY[MAX_RESERVATION_SIZE];
          arrayXY(atribts[3],arrayX);
          arrayXY(atribts[4],arrayY);
          ems_reserve((unsigned int)atoi(atribts[1]),(size_t)atoi(atribts[2]),arrayX,arrayY);
          break;
        case '5':
          size_t rows;
          size_t cols;
          int event_id=atoi(atribts[1]);
          rows=getRows(event_id);
          cols=getCols(event_id);
          sprintf(ret_msg,"%d|%ld|%ld|",1,rows,cols);
          write(response_pipe,ret_msg,TAMMSG);
          ems_show(response_pipe,(unsigned int)event_id);
          break;
        case '6':
          size_t numEvents=getNumEvents();
          sprintf(ret_msg,"%d|%ld|",1,numEvents);
          write(response_pipe,ret_msg,TAMMSG);
          ems_list_events(response_pipe);
          break;
      }

      temp=temp->next;
    }
  }

  //TODO: Close Server
  // Clean up and close the server's named pipe
  close(server_fd);
  unlink(argv[1]);

  ems_terminate();
  return 0;
}

void parseMsg(char buf[TAMMSG],char atribts[5][MAX_RESERVATION_SIZE]){
    int i=0;
    // Extract the first token
    char * token = strtok(buf, "|");
    strcpy(atribts[i],token);
    i++;
  // loop through the string to extract all other tokens
    while( token != NULL ) {
         //printing each token
        token = strtok(NULL, "|");
        if(token != NULL)
            strcpy(atribts[i],token);
        i++;
    }
}

void arrayXY(char *xys, size_t array[]){

  int i=0;
  int t=0;
  char container[MAX_RESERVATION_SIZE]="";
  int len=(int)strlen(xys);
  while(i<len){
    if(xys[i]=='[' ||xys[i]==']' ||xys[i]==' '  ){
      if(i!=0){
        if(t<5){
        array[t]=(size_t)atoi(container);
        if(t<5)
        t++;
        strcpy(container,"\0");}
      }
    }
    else
      strcat(container,&xys[i]);
    i++;
  }
  return;
  
}