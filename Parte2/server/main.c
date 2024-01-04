#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> // Include this for O_RDONLY and other file control options
#include <sys/stat.h> // Include this for mkfifo
#include <stdbool.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

#define MAX_SESSIONS 10  // Exemplo de limite de sessões

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

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

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

    // Se todas as sessões estiverem ativas, bloquear até que uma seja liberada
    while (generate_session_id() == -1) {
      // Pode adicionar um sleep aqui para evitar uso excessivo de CPU
    }

    char client_pipes[2][PATH_MAX];  // Array to hold client's request and response pipe names

    // Read from the server's named pipe
    ssize_t num_read = read(server_fd, client_pipes, sizeof(client_pipes));
    if (num_read > 0) {
      int session_id = generate_session_id();  // Generate new session ID
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

      close(client_response_fd);  // Close the client's response pipe
    } else if (num_read == 0) {
      // End of file, no data read, pipe was closed
      break;
    } else {
      // An error occurred
      perror("read");
      break;
    }
  }

  //TODO: Close Server
  // Clean up and close the server's named pipe
  close(server_fd);
  unlink(argv[1]);

  ems_terminate();
  return 0;
}