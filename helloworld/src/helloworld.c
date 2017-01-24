/*
 ============================================================================
 Name        : helloworld.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mpi.h>

int CAPACITY = 2;
int TIME_TO_EAT = 10;
int num_diners = 0;
int diners_arrived = 0;
typedef struct Diner {
	int id;
	double time_waited;
} Diner;

Diner *diners[10];


int get_total_diners(){
	return sizeof(diners)/sizeof(Diner *);
}
typedef struct Event {
	double time;
	char *type;
	Diner *diner;
} Event;

typedef struct {
	int priority;
	Event *data;
} node_t;

typedef struct {
	node_t *nodes;
	int len;
	int size;
} heap_t;

heap_t *waitlist_list;

void push(heap_t *h, int priority, Event *data) {
	if (h->len + 1 >= h->size) {
		h->size = h->size ? h->size * 2 : 4;
		h->nodes = (node_t *) realloc(h->nodes, h->size * sizeof(node_t));
	}
	int i = h->len + 1;
	int j = i / 2;
	while (i > 1 && h->nodes[j].priority > priority) {
		h->nodes[i] = h->nodes[j];
		i = j;
		j = j / 2;
	}
	h->nodes[i].priority = priority;
	h->nodes[i].data = data;
	h->len++;
}

Event *pop(heap_t *h) {
	int i, j, k;
	if (!h->len) {
		return NULL;
	}
	Event *data = h->nodes[1].data;
	h->nodes[1] = h->nodes[h->len];
	h->len--;
	i = 1;
	while (1) {
		k = i;
		j = 2 * i;
		if (j <= h->len && h->nodes[j].priority < h->nodes[k].priority) {
			k = j;
		}
		if (j + 1 <= h->len
				&& h->nodes[j + 1].priority < h->nodes[k].priority) {
			k = j + 1;
		}
		if (k == i) {
			break;
		}
		h->nodes[i] = h->nodes[k];
		i = k;
	}
	h->nodes[i] = h->nodes[h->len + 1];
	return data;
}

void waitlist(Event *e){
	e->diner->time_waited = e->time;
	push(waitlist_list, e->time, e);
}
void arrival(heap_t *events, Event *e) {

	printf("Diner %d arrived at t=%f!\n", e->diner->id, e->time);
	diners_arrived ++;

	if (num_diners < CAPACITY) {
		num_diners++;
		double departure_time = e->time + TIME_TO_EAT;
		Event *departure = (Event *) malloc(sizeof(Event));
		departure->time = departure_time;
		departure->type = "departure";
		departure->diner = e->diner;
		push(events, departure->time, departure);
		printf("Diner %d was seated at t=%f. Wait time = 0\n", e->diner->id, e->time);
	} else {
		waitlist(e);
	}
	if(diners_arrived < get_total_diners()){
		/*Event *next_arrival = malloc(sizeof(Event));
		next_arrival->time = e->time + rand()/3000.0;
		next_arrival->type = "arrival";
		next_arrival->diner = diners[diners_arrived];
		push(events,next_arrival->time,next_arrival);*/
		double next_arrival_time = e->time + rand()/300000000.0;
		int world_rank;
		MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
		MPI_Request req;
		MPI_Isend(&next_arrival_time, sizeof(next_arrival_time), MPI_BYTE, (world_rank + 1) % 2, 0, MPI_COMM_WORLD,&req);
		printf("SENT %f from %d\n", next_arrival_time, (world_rank + 1 ) %2);
	}
}

void departure(heap_t *events, Event *e){
	num_diners--;
	printf("Diner %d departed at t=%f!\n", e->diner->id,e->time);
	if(waitlist_list->len){
		Event *offlist = pop(waitlist_list);
		offlist->diner->time_waited = e->time - offlist->diner->time_waited;
		double departure_time = e->time + TIME_TO_EAT;
		Event *new_departure = (Event *) malloc(sizeof(Event));
		new_departure->time = departure_time;
		new_departure->type = "departure";
		new_departure->diner = offlist->diner;
		push(events, new_departure->time, new_departure);
		printf("Diner %d was seated at t=%f. Wait time = %f\n", offlist->diner->id, e->time, offlist->diner->time_waited);
		num_diners++;
	}
}

int main(void) {
	// setup
         // Initialize the MPI environment
        MPI_Init(NULL, NULL);
        // Find out rank, size
        int world_rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
        int world_size;
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);

	srand(time(NULL));
	double now = 0;
	int id_num = 1;
	int total_diners = sizeof diners / sizeof(*diners);
	for (size_t i = 0; i < total_diners; i++){
		Diner *d = (Diner *) malloc(sizeof(Diner));
		d->id=id_num++;
		d->time_waited=0;
		diners[i] =d;
	}
	heap_t *events = (heap_t *) calloc(1, sizeof(heap_t));
	waitlist_list = (heap_t *) calloc(1, sizeof(heap_t));
	Event *first_arrival = (Event *) malloc(sizeof(Event));
	first_arrival->time = now;
	first_arrival->type = "arrival";
	first_arrival->diner = diners[diners_arrived];
	push(events, now, first_arrival);
	int count = 5;
	printf("MADE IT PAST SETUP\n");
	MPI_Request req;
	int flag;
	double arrival_time =-1;
	printf("%d\n",arrival_time);
	printf("byte%d\n",MPI_BYTE);
	printf("source%d\n",MPI_ANY_SOURCE);
	printf("comm%d\n",MPI_COMM_WORLD);
	printf("status%d\n",MPI_STATUS_IGNORE);
	MPI_Irecv(&arrival_time, sizeof(arrival_time),MPI_BYTE,MPI_ANY_SOURCE,0,MPI_COMM_WORLD, &req);
	//progress forward
	while ( diners_arrived < get_total_diners() || (events->len) > 0) {
		count --;
		//process departures
		//printf("%d\n", events->len);
		MPI_Status status;
		
		if((events->len) == 0){
			printf("MPI_WAIT!\n");
			MPI_Wait(&req, &status);
		}
		MPI_Test(&req,&flag,&status);
		
		if(flag!=0){
			printf("MADE IT PAST IRECV, arrival_time = %f\n", arrival_time);
			printf("RECEIVED ARRIVAL\n");
			Event *new_event = malloc(sizeof(Event));
			new_event->time = arrival_time;
			new_event->type = "arrival";
			new_event->diner = diners[diners_arrived];
			push(events, new_event->time, new_event);
			printf("Events len %d\n",events->len);
			MPI_Irecv(&arrival_time, sizeof(arrival_time),MPI_BYTE,MPI_ANY_SOURCE,0,MPI_COMM_WORLD,&req);
		}
	

		
		Event *e = pop(events);
		if(e){
			if(strncmp(e->type,"arrival", 8) ==0){
				arrival(events,e);
			}else if(strncmp(e->type, "departure", 10) == 0){
				departure(events,e);
			}else{
				printf("ERROR event type = %s\n", e->type);
			}
		}
	}

	/*
	 if waiting list, move people in
	 increment waiting list times

	 */
	double total_wait = 0;
	for(int i=0; i < total_diners; i++){
		total_wait +=diners[i]->time_waited;
	}
	printf("Average wait for %d diners = %f\n",total_diners, total_wait/total_diners);
	MPI_Finalize();
	return EXIT_SUCCESS;
}
