#include "api.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "common/constants.h"

int session_id;
int req_pipe;
int resp_pipe;
int server_pipe;
char reqst_pipe_path[MAX_PATH_SIZE];
char respn_pipe_path[MAX_PATH_SIZE];


void fillString(char string[], size_t size){
  size_t len=strlen(string);
  for (size_t i=len;i<size;i++){
    string[i]='\0';
  }
}

char *arrayXY(size_t *xys, size_t num_seats){
  char lista[200]= "[";
  char temp[3];
  size_t i;
  for(i=0;i< num_seats-1; i++){
    sprintf(temp,"%ld ",xys[i]);
    strcat(lista, temp);
  }
  sprintf(temp,"%ld]",xys[i+1]);
  strcat(lista, temp);
  char *ptr=lista;
  return ptr;
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

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  //TODO: create pipes and connect to the server
  unlink(req_pipe_path);
  unlink(resp_pipe_path);
  char buf[TAMMSG];
  char sessionID[MAX_SESSION_COUNT];
  strcpy(reqst_pipe_path,req_pipe_path);
  strcpy(respn_pipe_path,resp_pipe_path);

  fillString(reqst_pipe_path,MAX_PATH_SIZE);
  fillString(respn_pipe_path,MAX_PATH_SIZE);

  // Criar os named pipes para comunicação entre cliente e servidor
  if (mkfifo(req_pipe_path, 0666) == -1 || mkfifo(resp_pipe_path, 0666) == -1) {
    printf("Erro ao criar named pipes\n");
    return 1; // Retornar 1 em caso de erro
  }
  
  // Abrir os pipes para leitura e escrita
  server_pipe = open(server_pipe_path, O_WRONLY);
  
  if (server_pipe == -1) {
    printf("Erro ao abrir os pipes\n");
    return 1; // Retornar 1 em caso de erro
  }
  
  sprintf(buf,"%s|%s|%s","OP_CODE=1",reqst_pipe_path,respn_pipe_path);
  fillString(buf,TAMMSG);
  write(server_pipe,buf, TAMMSG);
  
  if((resp_pipe = open(respn_pipe_path, O_RDONLY))<0)
    perror("Read error");
  read(resp_pipe,sessionID,sizeof(MAX_SESSION_COUNT));
  
  session_id= atoi(sessionID);
  // Retornar 0 em caso de sucesso
  return 0;
}

int ems_quit(void) { 
  //TODO: close pipes
  char buf[TAMMSG];
  strcpy(buf,"OP_CODE=2|" );
  fillString(buf,TAMMSG);
  write(req_pipe,buf, TAMMSG);
  close(req_pipe);
  close(resp_pipe);
  unlink(reqst_pipe_path);
  unlink(respn_pipe_path);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  char buf[TAMMSG];
  sprintf(buf,"%s|%d|%ld|%ld","OP_CODE=3",event_id,num_rows,num_cols);
  fillString(buf,TAMMSG);
  write(req_pipe,buf, TAMMSG);
  
  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  char buf[TAMMSG];
  sprintf(buf,"%s|%d|%ld|%s|%s","OP_CODE=4",event_id,num_seats,arrayXY(xs,num_seats),arrayXY(ys,num_seats));
  fillString(buf,TAMMSG);
  write(req_pipe,buf, TAMMSG);

  return 0;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  
  char buf[TAMMSG];
  char atribts[5][MAX_RESERVATION_SIZE];
  sprintf(buf,"%s|%d|%d","OP_CODE=5",out_fd,event_id);

  char buffer[TAMMSG*2];
  resp_pipe=open(respn_pipe_path,O_WRONLY);
  read(resp_pipe,buffer,TAMMSG*2);
  fillString(buffer,TAMMSG);
  parseMsg(buffer,atribts);
  
  write(out_fd,atribts[3],MAX_RESERVATION_SIZE);
  return 0;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  char buf[TAMMSG];
  char atribts[5][MAX_RESERVATION_SIZE];
  sprintf(buf,"%s|%d","OP_CODE=5",out_fd);
  fillString(buf,TAMMSG);
  parseMsg(buf,atribts);
  
  //write(out_fd,atribts[3],MAX_RESERVATION_SIZE);
  return 0;
}
