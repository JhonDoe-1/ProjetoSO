#include "api.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int requestPipeFd, responsePipeFd, serverPipeFd;

// NOTA: Adapt the message formatting to match the servers expected protocol

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  //TODO: create pipes and connect to the server
  // Create request and response pipes
  mkfifo(req_pipe_path, 0666);
  mkfifo(resp_pipe_path, 0666);

  // Open the server, request, and response pipes
  serverPipeFd = open(server_pipe_path, O_WRONLY);
  requestPipeFd = open(req_pipe_path, O_WRONLY);
  responsePipeFd = open(resp_pipe_path, O_RDONLY);

  if (serverPipeFd == -1 || requestPipeFd == -1 || responsePipeFd == -1) {
    perror("Error opening pipes");
    return -1;
  }

  return 1;
}

int ems_quit(void) { 
  //TODO: close pipes
  close(requestPipeFd);
  close(responsePipeFd);
  close(serverPipeFd);
  return 1;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  // Prepare the create request message
  char message[100];
  sprintf(message, "Create Event: %u, Rows: %zu, Cols: %zu", event_id, num_rows, num_cols);

  // Write the message to the request pipe
  write(requestPipeFd, message, strlen(message) + 1);

  // Read the response from the server
  char response[100];
  read(responsePipeFd, response, sizeof(response));

  printf("Server Response: %s\n", response);
  return 1;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  // ---------------------------------------------------------------
  // Prepare and send the reserve request
  // ... similar to ems_create, including event_id, num_seats, xs, ys in the message ...

  // Read and handle the response
  // ... similar to ems_create ...
  // ---------------------------------------------------------------
  // Prepare the reserve request message
  char message[200];
  sprintf(message, "Reserve Event: %u, Seats: %zu", event_id, num_seats); 
  // Include xs, ys in the message if needed

  // Write the message to the request pipe
  write(requestPipeFd, message, strlen(message) + 1);

  // Read the response from the server
  char response[100];
  read(responsePipeFd, response, sizeof(response));
  printf("Server Response: %s\n", response);
  return 1;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  // ---------------------------------------------------------------
  // Send the show request
  // ... similar to ems_create, but with event_id ...

  // Read the response and write it to out_fd
  // ... use read to get the response and write to out_fd ...
  // ---------------------------------------------------------------
  // Send the show request
  char message[100];
  sprintf(message, "Show Event: %u", event_id);
  write(requestPipeFd, message, strlen(message) + 1);

  // Read the response and write it to out_fd
  char response[100];
  read(responsePipeFd, response, sizeof(response));
  write(out_fd, response, strlen(response) + 1);
  return 1;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  // ---------------------------------------------------------------
  // Send the list events request
  // ... similar to ems_create, but specific to listing events ...

  // Read the response and write it to out_fd
  // ... similar to ems_show ...
  // ---------------------------------------------------------------
  // Send the list events request
  char message[100] = "List Events";
  write(requestPipeFd, message, strlen(message) + 1);

  // Read the response and write it to out_fd
  char response[100];
  read(responsePipeFd, response, sizeof(response));
  write(out_fd, response, strlen(response) + 1);
  return 1;
}
