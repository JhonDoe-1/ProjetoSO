#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

int executeCommand(int command);
void removeSubStr(char str[]);
void executaFicheiro(struct dirent *dp);

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  int i=1;;
  if (argc > 1) {
    char *endptr;
    unsigned long int delay = strtoul(argv[1], &endptr, 10);
    /*
    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }
    */
    state_access_delay_ms = (unsigned int)delay;

  }

  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }
  if(argc>1){
    if(strcmp(argv[1],"jobs")==0){
      DIR *dirp;
      struct dirent *dp;
      dirp = opendir(argv[1]);
      chdir(argv[1]);
      if (dirp == NULL) {
        printf("opendir failed on '%s'", argv[1]);
      }
      else{
        for (;;) {
          /*Lê o dp igual ao primeiro ficheiro do diretório*/
          dp = readdir(dirp);
          if (dp == NULL)
            break;
          /*se o nome do ficheiro for "." ou ".." salta*/
          if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            continue; /* Skip . and .. */

          /*se o nome do ficheiro tiver extensão ".jobs" */
          if(strstr(dp->d_name,".jobs")!=NULL){
            executaFicheiro(dp);
          }
            
        }
      }
    }
  }
  while (i==1) {
    
    printf("> ");
    fflush(stdout);

    i=executeCommand(STDIN_FILENO);
    if(i==0){
      return 0;
    }
  }
}



int executeCommand(int command){
  unsigned int event_id, delay;
    
  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
  
  switch (get_next(command)) {
      case CMD_CREATE:
        if (parse_create(command, &event_id, &num_rows, &num_columns) != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          
        }

        if (ems_create(event_id, num_rows, num_columns)) {
          fprintf(stderr, "Failed to create event\n");
        }

        return 1;

      case CMD_RESERVE:
        num_coords = parse_reserve(command, MAX_RESERVATION_SIZE, &event_id, xs, ys);

        if (num_coords == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          
        }

        if (ems_reserve(event_id, num_coords, xs, ys)) {
          fprintf(stderr, "Failed to reserve seats\n");
        }

        return 1;

      case CMD_SHOW:
        if (parse_show(command, &event_id) != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          
        }

        if (ems_show(event_id)) {
          fprintf(stderr, "Failed to show event\n");
        }

        return 1;

      case CMD_LIST_EVENTS:
        if (ems_list_events()) {
          fprintf(stderr, "Failed to list events\n");
        }

        return 1;

      case CMD_WAIT:
        if (parse_wait(command, &delay, NULL) == -1) {  // thread_id is not implemented
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          
        }

        if (delay > 0) {
          printf("Waiting...\n");
          ems_wait(delay);
        }

        return 1;

      case CMD_INVALID:
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        return 1;

      case CMD_HELP:
        printf(
            "Available commands:\n"
            "  CREATE <event_id> <num_rows> <num_columns>\n"
            "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
            "  SHOW <event_id>\n"
            "  LIST\n"
            "  WAIT <delay_ms> [thread_id]\n"  // thread_id is not implemented
            "  BARRIER\n"                      // Not implemented
            "  HELP\n");

        return 1;

      case CMD_BARRIER:  // Not implemented
      case CMD_EMPTY:
        return 1;

      case EOC:
        ems_terminate();
        return 0;
    }
    return 0;
}



void removeSubStr(char str[]){
    int i=0;
    while(str[i]!='.'){
      i++;
    }
    str[i+4]='\0';
    str[i+3]='\0';
    str[i+2]='\0';
    str[i+1]='\0';
    str[i]='\0';
    
}

void executaFicheiro(struct dirent *dp){
  /*outputname é o nome do ficheiro output a ser criado*/
  char outputname[strlen(dp->d_name)+1];
  strcpy(outputname,dp->d_name);
  removeSubStr(outputname);
  strcat(outputname,".out");
  /*declaraçao dos file descriptores*/
  int file,outfile, fd;

  /*cria um ficheiro de nom outname para o file descriptor outfile*/
  outfile=open(outputname,O_WRONLY | O_CREAT, 0644);
  if (outfile == -1) {
    perror("open failed");
    exit(1);
  }

  /*abre o ficheiro dp*/
  file =open(dp->d_name,O_RDONLY);
  if (file == -1)
     printf("error opening file %s", dp->d_name);
  /** fd=dub(1) cria um novo file descriptor  que é basicamente um ponteiro para o ficheiro stdout "1"
  preciseide fazer isto porque tive que alterar momentaneamente o ficheiro de output (em ve de se no terminal "1" passará a ser no ficheiro criado)
  */
  fd=dup(1);
  if (fd == -1) {
    perror("dup failed"); 
    exit(1);
  }
  /** dup2(outfile,1) altera o ficheiro de output (da terminal para outfile)*/
  if (dup2(outfile, 1) == -1) {
    perror("dup2 failed"); 
    exit(1);
  }

  /*tamanho do ficheiro "file" a ser lido*/
  long int size=lseek(file,0,SEEK_END);
  /*coloca o cursor no inicio de "file"*/
  lseek(file,0,SEEK_SET );
  /*enquanto o cursor não chegar ao final */
  while(lseek(file,0,SEEK_CUR) != size){
  /*executa comandos*/
    executeCommand(file);
  }
  /*reset do cursor (não sei se é necessário mas são 4am e o meu cérebro já não funciona bem)*/
  lseek(file,0,SEEK_SET);

  /*fecha os ficheiros*/
  if (close(file) == -1)
    printf("close input");
  if (close(outfile) == -1)
    printf("close input");
            
  /*não sei oq faz só sei que sem isto n funciona lol*/
  fflush(stdout);

  /*volta a alterar o ficheiro de output [de outfile (onde 1 está a apontar agora) para o stdout (onde fd está a apontar)] */
  if (dup2(fd, 1) == -1) {
    perror("dup2 failed"); 
    exit(1);
  }
}
