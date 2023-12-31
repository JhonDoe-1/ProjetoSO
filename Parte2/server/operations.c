#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <stdbool.h>

#include "common/io.h"
#include "eventlist.h"
#include "operations.h"

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_us = 0;

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @param from First node to be searched.
/// @param to Last node to be searched.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(unsigned int event_id, struct ListNode* from, struct ListNode* to) {
  struct timespec delay = {0, state_access_delay_us * 1000};
  nanosleep(&delay, NULL);  // Should not be removed

  return get_event(event_list, event_id, from, to);
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }

int ems_init(unsigned int delay_us) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  state_access_delay_us = delay_us;

  return event_list == NULL;
}

int ems_terminate() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_wrlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  free_list(event_list);
  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_wrlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  if (get_event_with_delay(event_id, event_list->head, event_list->tail) != NULL) {
    fprintf(stderr, "Event already exists\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }

  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  if (pthread_mutex_init(&event->mutex, NULL) != 0) {
    pthread_rwlock_unlock(&event_list->rwl);
    free(event);
    return 1;
  }
  event->data = calloc(num_rows * num_cols, sizeof(unsigned int));

  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    pthread_rwlock_unlock(&event_list->rwl);
    free(event);
    return 1;
  }

  if (append_to_list(event_list, event) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    pthread_rwlock_unlock(&event_list->rwl);
    free(event->data);
    free(event);
    return 1;
  }

  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

  pthread_rwlock_unlock(&event_list->rwl);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  if (pthread_mutex_lock(&event->mutex) != 0) {
    fprintf(stderr, "Error locking mutex\n");
    return 1;
  }

  for (size_t i = 0; i < num_seats; i++) {
    if (xs[i] <= 0 || xs[i] > event->rows || ys[i] <= 0 || ys[i] > event->cols) {
      fprintf(stderr, "Seat out of bounds\n");
      pthread_mutex_unlock(&event->mutex);
      return 1;
    }
  }

  for (size_t i = 0; i < event->rows * event->cols; i++) {
    for (size_t j = 0; j < num_seats; j++) {
      if (seat_index(event, xs[j], ys[j]) != i) {
        continue;
      }

      if (event->data[i] != 0) {
        fprintf(stderr, "Seat already reserved\n");
        pthread_mutex_unlock(&event->mutex);
        return 1;
      }

      break;
    }
  }

  unsigned int reservation_id = ++event->reservations;

  for (size_t i = 0; i < num_seats; i++) {
    event->data[seat_index(event, xs[i], ys[i])] = reservation_id;
  }

  pthread_mutex_unlock(&event->mutex);
  return 0;
}

int ems_show(int out_fd, unsigned int event_id) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

  pthread_rwlock_unlock(&event_list->rwl);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  if (pthread_mutex_lock(&event->mutex) != 0) {
    fprintf(stderr, "Error locking mutex\n");
    return 1;
  }

  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      char buffer[16];
      sprintf(buffer, "%u", event->data[seat_index(event, i, j)]);

      if (print_str(out_fd, buffer)) {
        perror("Error writing to file descriptor");
        pthread_mutex_unlock(&event->mutex);
        return 1;
      }

      if (j < event->cols) {
        if (print_str(out_fd, " ")) {
          perror("Error writing to file descriptor");
          pthread_mutex_unlock(&event->mutex);
          return 1;
        }
      }
    }

    if (print_str(out_fd, "\n")) {
      perror("Error writing to file descriptor");
      pthread_mutex_unlock(&event->mutex);
      return 1;
    }
  }

  pthread_mutex_unlock(&event->mutex);
  return 0;
}

int ems_list_events(int out_fd) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  struct ListNode* to = event_list->tail;
  struct ListNode* current = event_list->head;

  if (current == NULL) {
    char buff[] = "No events\n";
    if (print_str(out_fd, buff)) {
      perror("Error writing to file descriptor");
      pthread_rwlock_unlock(&event_list->rwl);
      return 1;
    }

    pthread_rwlock_unlock(&event_list->rwl);
    return 0;
  }

  while (1) {
    char buff[] = "Event: ";
    if (print_str(out_fd, buff)) {
      perror("Error writing to file descriptor");
      pthread_rwlock_unlock(&event_list->rwl);
      return 1;
    }

    char id[16];
    sprintf(id, "%u\n", (current->event)->id);
    if (print_str(out_fd, id)) {
      perror("Error writing to file descriptor");
      pthread_rwlock_unlock(&event_list->rwl);
      return 1;
    }

    if (current == to) {
      break;
    }

    current = current->next;
  }

  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}

// Mutex for thread-safe incrementing of the session ID
pthread_mutex_t session_id_mutex = PTHREAD_MUTEX_INITIALIZER;



// Head of the sessions linked list
SessionNode* sessions_head = NULL;

// Mutex for thread-safe access to the sessions linked list
pthread_mutex_t sessions_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function to generate unique session IDs in a thread-safe manner
/**
 * Function is intended to produce a unique identifier for each new session started by a client 
 * it increments a static integer variable to ensure each session ID is different from the last
*/


// Function to store session details in a thread-safe manner
/**
 * Function is designed to save the details of a client's session. 
 * It takes the session ID and the paths to the client's named pipes for requests and responses
 * These details would be stored in a linked list, with each node representing a session. 
 * This list would be used to manage sessions and facilitate communication between the server and its clients.
*/

struct Event *getFirstEvent(){
  return event_list->head;
}

void store_session_details(int session_id, const char* request_pipe, const char* response_pipe) {
  pthread_mutex_lock(&sessions_mutex);
    SessionNode* new_node = malloc(sizeof(SessionNode));
    if (new_node == NULL) {
      // Handle memory allocation failure
      exit(EXIT_FAILURE);
    }
    new_node->session_id = session_id;
    strcpy(new_node->request_pipe, request_pipe);
    strcpy(new_node->response_pipe, response_pipe);
    new_node->next = NULL;

    
    // Add the new node to the front of the linked list for simplicity
    new_node->next = sessions_head;
    sessions_head = new_node;
    pthread_mutex_unlock(&sessions_mutex);
}

// ----------------------------------------------------------------------------------------------------------------------
// !!!!! MAKE SURE that these two functions are called during the server shutdown process after all threads have been 
// joined and no more sessions are active
// function to free the memory allocated for the session linked list before the server shuts down.
void free_sessions() {
    pthread_mutex_lock(&sessions_mutex);
    SessionNode* current = sessions_head;
    while (current != NULL) {
        SessionNode* temp = current;
        current = current->next;
        free(temp);
    }
    sessions_head = NULL;
    pthread_mutex_unlock(&sessions_mutex);
}
void free_Session(int id){
  SessionNode* current = sessions_head;
  while (current != NULL) {
    current = current->next;
    if(current->next!=NULL && current->next->session_id==id){
      SessionNode* temp = current->next;
      current->next=temp->next;
      free(temp);
      break;
    }
    
  }
}

SessionNode *SessionList(){
  return sessions_head;
}


// Function to clean up the mutexes
void destroy_mutexes() {
    pthread_mutex_destroy(&session_id_mutex);
    pthread_mutex_destroy(&sessions_mutex);
}
size_t getRows(int event_id){
  struct Event* event= get_event(event_list,(unsigned int) event_id, event_list->head, event_list->tail);
  return event->rows;
}
size_t getCols(int event_id){
  struct Event* event= get_event(event_list,(unsigned int) event_id, event_list->head, event_list->tail);
  return event->cols;
}
size_t getNumEvents(){
  struct ListNode *temp;
  size_t numEvents=0;
  temp= event_list->head;
  while(temp!=NULL){
    temp=temp->next;
    numEvents++;
  }
  return numEvents;
}
// ----------------------------------------------------------------------------------------------------------------------

// Estrutura para representar uma mensagem
/*typedef struct {
    int session_id;
    char message_type;  // 'E' para fim de sessão, outros tipos conforme necessário
    // Outros campos da mensagem...
} session_message;*/
