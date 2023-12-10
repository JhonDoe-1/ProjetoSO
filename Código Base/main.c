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
char * removeSubStr(char * str,char * substr);

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
      if (dirp == NULL) {
        printf("opendir failed on '%s'", argv[1]);
      }
      else{
        for (;;) {
          dp = readdir(dirp);
          if (dp == NULL)
            break;
          if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            continue; /* Skip . and .. */
          
          if(strstr(dp->d_name,".jobs")!=NULL){
            
            printf("%s\n", dp->d_name);

          }
          
          /*printf("%s\n", dp->d_name);*/
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

char * removeSubStr(char * str, char *substr){
    char *scan_p, *temp_p;
    int subStrSize = 5;
    if (str == NULL){
        return 0;
    }
    else if (substr == NULL){
        return str;
    }
    else if (strlen(substr)> strlen(str)){
        return str;
    }
    temp_p = str;
    while((scan_p = strstr(temp_p,substr))){
        temp_p = scan_p + subStrSize;
        memmove(scan_p, temp_p, sizeof(temp_p)+1);
    }
    return str;
}


