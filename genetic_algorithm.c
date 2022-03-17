#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "genetic_algorithm.h"

int read_input(sack_object **objects, int *object_count, int *sack_capacity, int *generations_count, int argc, char *argv[])
{
	FILE *fp;

	if (argc < 3) {
		fprintf(stderr, "Usage:\n\t./tema1 in_file generations_count\n");
		return 0;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		return 0;
	}

	if (fscanf(fp, "%d %d", object_count, sack_capacity) < 2) {
		fclose(fp);
		return 0;
	}

	if (*object_count % 10) {
		fclose(fp);
		return 0;
	}

	sack_object *tmp_objects = (sack_object *) calloc(*object_count, sizeof(sack_object));

	for (int i = 0; i < *object_count; ++i) {
		if (fscanf(fp, "%d %d", &tmp_objects[i].profit, &tmp_objects[i].weight) < 2) {
			free(objects);
			fclose(fp);
			return 0;
		}
	}

	fclose(fp);

	*generations_count = (int) strtol(argv[2], NULL, 10);
	
	if (*generations_count == 0) {
		free(tmp_objects);

		return 0;
	}

	*objects = tmp_objects;

	return 1;
}

void print_objects(const sack_object *objects, int object_count)
{
	for (int i = 0; i < object_count; ++i) {
		printf("%d %d\n", objects[i].weight, objects[i].profit);
	}
}

void print_generation(const individual *generation, int limit)
{
	for (int i = 0; i < limit; ++i) {
		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			printf("%d ", generation[i].chromosomes[j]);
		}

		printf("\n%d - %d\n", i, generation[i].fitness);
	}
}

void print_best_fitness(const individual *generation)
{
	printf("%d\n", generation[0].fitness);
}

void compute_fitness_function(const sack_object *objects, individual *generation, int object_count, int sack_capacity,
								int num_threads, int id)
{
	int weight;
	int profit;

	int start = id * (double)object_count / num_threads;
	int end = (id + 1) * (double)object_count / num_threads;
	if (object_count < end) {
		end = object_count;
	} 

	for (int i = start; i < end; ++i) {
		weight = 0;
		profit = 0;

		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			if (generation[i].chromosomes[j]) {
				weight += objects[j].weight;
				profit += objects[j].profit;
			}
		}

		generation[i].fitness = (weight <= sack_capacity) ? profit : 0;
	}
}

int cmpfunc(const void *a, const void *b)
{
	int i;
	individual *first = (individual *) a;
	individual *second = (individual *) b;

	int res = second->fitness - first->fitness; // decreasing by fitness
	if (res == 0) {
		int first_count = 0, second_count = 0;

		for (i = 0; i < first->chromosome_length && i < second->chromosome_length; ++i) {
			first_count += first->chromosomes[i];
			second_count += second->chromosomes[i];
		}

		res = first_count - second_count; // increasing by number of objects in the sack
		if (res == 0) {
			return second->index - first->index;
		}
	}

	return res;
}

void mutate_bit_string_1(const individual *ind, int generation_index)
{
	int i, mutation_size;
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	if (ind->index % 2 == 0) {
		// for even-indexed individuals, mutate the first 40% chromosomes by a given step
		mutation_size = ind->chromosome_length * 4 / 10;
		for (i = 0; i < mutation_size; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	} else {
		// for even-indexed individuals, mutate the last 80% chromosomes by a given step
		mutation_size = ind->chromosome_length * 8 / 10;
		for (i = ind->chromosome_length - mutation_size; i < ind->chromosome_length; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	}
}

void mutate_bit_string_2(const individual *ind, int generation_index)
{
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	// mutate all chromosomes by a given step
	for (int i = 0; i < ind->chromosome_length; i += step) {
		ind->chromosomes[i] = 1 - ind->chromosomes[i];
	}
}

void crossover(individual *parent1, individual *child1, int generation_index)
{
	individual *parent2 = parent1 + 1;
	individual *child2 = child1 + 1;
	int count = 1 + generation_index % parent1->chromosome_length;

	memcpy(child1->chromosomes, parent1->chromosomes, count * sizeof(int));
	memcpy(child1->chromosomes + count, parent2->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));

	memcpy(child2->chromosomes, parent2->chromosomes, count * sizeof(int));
	memcpy(child2->chromosomes + count, parent1->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));
}

void copy_individual(const individual *from, const individual *to)
{
	memcpy(to->chromosomes, from->chromosomes, from->chromosome_length * sizeof(int));
}

void free_generation(individual *generation)
{
	int i;

	for (i = 0; i < generation->chromosome_length; ++i) {
		free(generation[i].chromosomes);
		generation[i].chromosomes = NULL;
		generation[i].fitness = 0;
	}
}

void merge(individual *source, int start, int mid, int end, individual *destination) {
	int iA = start;
	int iB = mid;
	int i;

	for (i = start; i < end; i++) {
		if (end == iB || (iA < mid && (cmpfunc(&source[iA], &source[iB]) < 0))) {
			destination[i] = source[iA];
			iA++;
		} else {
			destination[i] = source[iB];
			iB++;
		}
	}
}

void merge_by_threads(individual *current_generation, int object_count, int num_threads) {
	if (num_threads == 2) {
		// se interclaseaza prima jumatate cu cea de a doua
		individual *new_current_generation = (individual*) calloc(object_count, sizeof(individual));

		merge(current_generation, 0, object_count/num_threads, object_count, new_current_generation);
		memcpy(current_generation, new_current_generation, object_count * sizeof(individual));
	} else if (num_threads == 3) {
		// se interclaseaza primele 2 treimi, iar bucata sortata rezultata, se interclaseaza
		// cu cea de a3a
		individual *new_current_generation = (individual*) calloc(object_count, sizeof(individual));

		merge(current_generation, 0, object_count/num_threads, 2 * object_count/num_threads, new_current_generation);
		memcpy(current_generation, new_current_generation, (2 * object_count/num_threads) * sizeof(individual));
		new_current_generation = (individual*) calloc(object_count, sizeof(individual));

		merge(current_generation, 0, 2 * object_count/num_threads, object_count, new_current_generation);
		memcpy(current_generation, new_current_generation, object_count * sizeof(individual));
	} else if (num_threads == 4) {
		// se interclaseaza primele 2 parti, apoi urmatoarele 2 parti, apoi cele 2 parti rezultate
		// in urma primelor 2 interclasari
		individual *new_current_generation = (individual*) calloc(object_count, sizeof(individual));

		merge(current_generation, 0, object_count/num_threads, 2 * object_count/num_threads, new_current_generation);
		memcpy(current_generation, new_current_generation, (2 * object_count/num_threads) * sizeof(individual));
		new_current_generation = (individual*) calloc(object_count, sizeof(individual));
				
		merge(current_generation, 2 * object_count/num_threads, 3 * object_count/num_threads, object_count, new_current_generation);
		memcpy(current_generation + 2 * object_count/num_threads, new_current_generation + 2 * object_count/num_threads,
									 (2 * object_count/num_threads) * sizeof(individual));
		new_current_generation = (individual*) calloc(object_count, sizeof(individual));

		merge(current_generation, 0, 2 * object_count/num_threads, object_count, new_current_generation);
		memcpy(current_generation, new_current_generation, object_count * sizeof(individual));
	}
}

void *run_genetic_algorithm(void *args)
{	
	run_args run_genetic_alg_args = *(run_args *)args;
	
	// se extrag argumentele din structura
	sack_object *objects = run_genetic_alg_args.objects;
    int object_count = run_genetic_alg_args.object_count;
    int generations_count = run_genetic_alg_args.generations_count;
    int sack_capacity = run_genetic_alg_args.sack_capacity;
    int num_threads = run_genetic_alg_args.num_threads;
	int id = run_genetic_alg_args.id;
	individual *current_generation = run_genetic_alg_args.current_generation;
	individual *next_generation = run_genetic_alg_args.next_generation;

	int start, end;
	int count, cursor;
	individual *tmp = NULL;

	// iterate for each generation
	for (int k = 0; k < generations_count; ++k) {
		cursor = 0;		
		// compute fitness and sort by it
		compute_fitness_function(objects, current_generation, object_count, sack_capacity, num_threads, id);
		// se asteapta ca toate threadurile sa termine de efectuat compute_fitness_function
		pthread_barrier_wait(run_genetic_alg_args.barrier);

		// se calculeaza indicii de start si end necesari apelarii qsort
		start = id * (double)object_count / num_threads;
		end = (id + 1) * (double)object_count / num_threads;
		if (object_count < end) {
			end = object_count;
		}
		qsort(current_generation + start, end - start, sizeof(individual), cmpfunc);
		// se asteapta ca toate threadurile sa termine de sortat portiunea corespunzatoare
		pthread_barrier_wait(run_genetic_alg_args.barrier);

		// dupa apelarea qsort-ului se unifica vectorul current_generation
		if (id == 0) {
			merge_by_threads(current_generation, object_count, num_threads);
		}
		pthread_barrier_wait(run_genetic_alg_args.barrier);

		// keep first 30% children (elite children selection)
		count = object_count * 3 / 10;

		// pentru for-urile viitoare se vor calcula indici de start
		// si end, in functie de id-ul threadurilor, pentru a paraleliza operatiile
		start = id * (double)count / num_threads;
		end = (id + 1) * (double)count / num_threads;
		if (count < end) {
			end = count;
		}
	
		for (int i = start; i < end; ++i) {
			copy_individual(current_generation + i, next_generation + i);
		}
		cursor = count;

		// mutate first 20% children with the first version of bit string mutation
		count = object_count * 2 / 10;

		start = id * (double)count / num_threads;
		end = (id + 1) * (double)count / num_threads;
		if (count < end) {
			end = count;
		}

		for (int i = start; i < end; ++i) {
			copy_individual(current_generation + i, next_generation + cursor + i);
			mutate_bit_string_1(next_generation + cursor + i, k);
		}
		cursor += count;

		// mutate next 20% children with the second version of bit string mutation
		count = object_count * 2 / 10;

		start = id * (double)count / num_threads;
		end = (id + 1) * (double)count / num_threads;
		if (count < end) {
			end = count;
		}

		for (int i = start; i < end; ++i) {
			copy_individual(current_generation + i + count, next_generation + cursor + i);
			mutate_bit_string_2(next_generation + cursor + i, k);
		}
		cursor += count;

		// crossover first 30% parents with one-point crossover
		// (if there is an odd number of parents, the last one is kept as such)
		count = object_count * 3 / 10;

		if (count % 2 == 1) {
			copy_individual(current_generation + object_count - 1, next_generation + cursor + count - 1);
			count--;
		}

		start = id * (double)count / num_threads;
		end = (id + 1) * (double)count / num_threads;
		if (count < end) {
			end = count;
		}
		if (start % 2 == 1) {
			start += 1;
		}

		for (int i = start; i < end; i += 2) {
			crossover(current_generation + i, next_generation + cursor + i, k);
		}

		// switch to new generation
		tmp = current_generation;
		current_generation = next_generation;
		next_generation = tmp;

		start = id * (double)object_count / num_threads;
		end = (id + 1) * (double)object_count / num_threads;
		if (count < end) {
			end = count;
		}

		for (int i = start; i < end; ++i) {
			current_generation[i].index = i;
		}

		// se asteapta ca toate threadurile sa ajunga in acest punct pentru a se putea
		// afisa best fitness
		pthread_barrier_wait(run_genetic_alg_args.barrier);
		// afisarea pentru best fitness se realizeaza doar pe un singur thread
		if ((k % 5 == 0) && (id == 0)) {
			print_best_fitness(current_generation);
		}
	}

	// se asteapta ca toate threadurile sa ajunga in acest punct pentru a se apela
	// compute_fitness_function
	pthread_barrier_wait(run_genetic_alg_args.barrier);
	compute_fitness_function(objects, current_generation, object_count, sack_capacity, num_threads, id);
	
	// se calculeaza indicii start si end pentru apelarea qsort-ului
	start = id * (double)object_count / num_threads;
	end = (id + 1) * (double)object_count / num_threads;
	if (object_count < end) {
		end = object_count;
	}
	qsort(current_generation + start, end - start, sizeof(individual), cmpfunc);
	// se asteapta ca toate threadurile sa termine de sortat portiunea din vector corespunzatoare
	pthread_barrier_wait(run_genetic_alg_args.barrier);

	// dupa apelarea qsort-ului se unifica vectorul current_generation
	if (id == 0) {
		merge_by_threads(current_generation, object_count, num_threads);
	}
	
	// se asteapta ca toate threadurile sa ajunga in acest punct pentru a se putea afisa best fitness
	pthread_barrier_wait(run_genetic_alg_args.barrier);

	if (id == 0) {
		// afisarea pentru best fitness se realizeaza doar pe un singur thread
		print_best_fitness(current_generation);
	}

  	pthread_exit(NULL);
}