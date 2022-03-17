#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "genetic_algorithm.h"

int main(int argc, char *argv[]) {
	int num_threads = atoi(argv[3]);
	pthread_t threads[num_threads];
	int id = 0;
	int r;
	void *status;
	
	pthread_barrier_t barrier;
	pthread_barrier_init(&barrier, NULL, num_threads);

	// array with all the objects that can be placed in the sack
	sack_object *objects = NULL;

	// number of objects
	int object_count = 0;

	// maximum weight that can be carried in the sack
	int sack_capacity = 0;

	// number of generations
	int generations_count = 0;

	if (!read_input(&objects, &object_count, &sack_capacity, &generations_count, argc, argv)) {
		return 0;
	}

	individual *current_generation = (individual*) calloc(object_count, sizeof(individual));
	individual *next_generation = (individual*) calloc(object_count, sizeof(individual));

	// set initial generation (composed of object_count individuals with a single item in the sack)
	for (int i = 0; i < object_count; ++i) {
		current_generation[i].fitness = 0;
		current_generation[i].chromosomes = (int*) calloc(object_count, sizeof(int));
		current_generation[i].chromosomes[i] = 1;
		current_generation[i].index = i;
		current_generation[i].chromosome_length = object_count;

		next_generation[i].fitness = 0;
		next_generation[i].chromosomes = (int*) calloc(object_count, sizeof(int));
		next_generation[i].index = i;
		next_generation[i].chromosome_length = object_count;
	}

		run_args run_genetic_alg_args[num_threads];

	for (id = 0; id < num_threads; id++) {
		run_genetic_alg_args[id].objects = objects;
		run_genetic_alg_args[id].object_count = object_count;
		run_genetic_alg_args[id].generations_count = generations_count;
		run_genetic_alg_args[id].sack_capacity = sack_capacity;
		run_genetic_alg_args[id].num_threads = num_threads;
		run_genetic_alg_args[id].barrier = &barrier;
		run_genetic_alg_args[id].current_generation = current_generation;
		run_genetic_alg_args[id].next_generation = next_generation;
		run_genetic_alg_args[id].id = id;
		
		r = pthread_create(&threads[id], NULL, run_genetic_algorithm, &run_genetic_alg_args[id]);

		if (r) {
	  		printf("Eroare la crearea thread-ului %d\n", id);
	  		exit(-1);
		}
  	}
	
	for (id = 0; id < num_threads; id++) {
		int r = pthread_join(threads[id], &status);

		if (r) {
	  		printf("Eroare la asteptarea thread-ului %d\n", id);
	  		exit(-1);
		}
  	}

	pthread_barrier_destroy(&barrier);

	free(objects);
	return 0;
}
