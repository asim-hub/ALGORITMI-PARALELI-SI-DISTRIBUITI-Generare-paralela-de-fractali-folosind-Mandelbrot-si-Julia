/*
 * APD - Tema 1
 * Octombrie 2020
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

pthread_barrier_t b1, b2;
int width1, height1;
int width2, height2;
int **result1, **result2;

int P;//nr thread-uri
int N1, N2;//dimensiune rezultat

char *in_filename_julia;
char *in_filename_mandelbrot;
char *out_filename_julia;
char *out_filename_mandelbrot;

// structura pentru un numar complex
typedef struct _complex {
	double a;
	double b;
} complex;

// structura pentru parametrii unei rulari
typedef struct _params {
	int is_julia, iterations;
	double x_min, x_max, y_min, y_max, resolution;
	complex c_julia;
} params;

params par1, par2;

// citeste argumentele programului
void get_args(int argc, char **argv)
{
	if (argc < 5) {
		printf("Numar insuficient de parametri:\n\t"
				"./tema1 fisier_intrare_julia fisier_iesire_julia "
				"fisier_intrare_mandelbrot fisier_iesire_mandelbrot\n");
		exit(1);
	}

	in_filename_julia = argv[1];
	out_filename_julia = argv[2];
	in_filename_mandelbrot = argv[3];
	out_filename_mandelbrot = argv[4];
	P = atoi(argv[5]);
}

// citeste fisierul de intrare
void read_input_file(char *in_filename, params* par)
{
	FILE *file = fopen(in_filename, "r");
	if (file == NULL) {
		printf("Eroare la deschiderea fisierului de intrare!\n");
		exit(1);
	}

	fscanf(file, "%d", &par->is_julia);
	fscanf(file, "%lf %lf %lf %lf",
			&par->x_min, &par->x_max, &par->y_min, &par->y_max);
	fscanf(file, "%lf", &par->resolution);
	fscanf(file, "%d", &par->iterations);

	if (par->is_julia) {
		fscanf(file, "%lf %lf", &par->c_julia.a, &par->c_julia.b);
	}

	fclose(file);
}

// scrie rezultatul in fisierul de iesire
void write_output_file(char *out_filename, int **result, int width, int height)
{
	int i, j;

	FILE *file = fopen(out_filename, "w");
	if (file == NULL) {
		printf("Eroare la deschiderea fisierului de iesire!\n");
		return;
	}

	fprintf(file, "P2\n%d %d\n255\n", width, height);
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			fprintf(file, "%d ", result[i][j]);
		}
		fprintf(file, "\n");
	}

	fclose(file);
}

// aloca memorie pentru rezultat
int **allocate_memory(int width, int height)
{
	int **result;
	int i;

	result = malloc(height * sizeof(int*));
	if (result == NULL) {
		printf("Eroare la malloc!\n");
		exit(1);
	}

	for (i = 0; i < height; i++) {
		result[i] = malloc(width * sizeof(int));
		if (result[i] == NULL) {
			printf("Eroare la malloc!\n");
			exit(1);
		}
	}

	return result;
}

// elibereaza memoria alocata
void free_memory(int **result, int height)
{
	int i;

	for (i = 0; i < height; i++) {
		free(result[i]);
	}
	free(result);
}

// ruleaza algoritmul Julia
void run_julia(params *par, int **result, int start1, int end1, int height, int thread_id)
{
	int w, h, i;

	for (w = start1; w < end1; w++) {
		for (h = 0; h < height; h++) {
			int step = 0;
			complex z = { .a = w * par->resolution + par->x_min,
							.b = h * par->resolution + par->y_min };

			while (sqrt(pow(z.a, 2.0) + pow(z.b, 2.0)) < 2.0 && step < par->iterations) {
				complex z_aux = { .a = z.a, .b = z.b };

				z.a = pow(z_aux.a, 2) - pow(z_aux.b, 2) + par->c_julia.a;
				z.b = 2 * z_aux.a * z_aux.b + par->c_julia.b;

				step++;
			}

			result[h][w] = step % 256;
		}
	}

	pthread_barrier_wait(&b2);

	// transforma rezultatul din coordonate matematice in coordonate ecran
	int N = (height / 2);
	int start = thread_id * (double)N / P;
    int end;

    if ((thread_id + 1) * (double)N / P < N){
        end = (thread_id + 1) * (double)N / P;
    } else {
        end = N;
    }

	for (i = start; i < end; i++) {
		int *aux = result[i];
		result[i] = result[height - i - 1];
		result[height - i - 1] = aux;
	}
}

// ruleaza algoritmul Mandelbrot
void run_mandelbrot(params *par, int **result, int start2, int end2, int height, int thread_id)
{
	int w, h, i;

	for (w = start2; w < end2; w++) {
		for (h = 0; h < height; h++) {
			complex c = { .a = w * par->resolution + par->x_min,
							.b = h * par->resolution + par->y_min };
			complex z = { .a = 0, .b = 0 };
			int step = 0;

			while (sqrt(pow(z.a, 2.0) + pow(z.b, 2.0)) < 2.0 && step < par->iterations) {
				complex z_aux = { .a = z.a, .b = z.b };

				z.a = pow(z_aux.a, 2.0) - pow(z_aux.b, 2.0) + c.a;
				z.b = 2.0 * z_aux.a * z_aux.b + c.b;

				step++;
			}

			result[h][w] = step % 256;
		}
	}

	pthread_barrier_wait(&b2);

	// transforma rezultatul din coordonate matematice in coordonate ecran
	int N = (height / 2);
	int start = thread_id * (double)N / P;
	int end;

    if ((thread_id + 1) * (double)N / P < N){
        end = (thread_id + 1) * (double)N / P;
    } else {
        end = N;
    }

	for (i = start; i < end; i++) {
		int *aux = result[i];
		result[i] = result[height - i - 1];
		result[height - i - 1] = aux;
	}
}

void *thr_func(void *arg)
{
	int thread_id = *(int *)arg;
	int start1 = thread_id * (double)width1 / P;
	int end1;
	if ((thread_id + 1) * (double)width1 / P < width1){
        end1 = (thread_id + 1) * (double)width1 / P;
    } else {
        end1 = width1;
    }

	// julia:
	run_julia(&par1, result1, start1, end1, height1, thread_id);

	// bariera
	pthread_barrier_wait(&b1);

	// scriu in fisier giulia
	if (thread_id == P - 1) {
		write_output_file(out_filename_julia, result1, width1, height1);
	}

	// bariera
	pthread_barrier_wait(&b1);

	// mandelbrot:
	int start2 = thread_id * (double)width2 / P;

	int end2;
	if ((thread_id + 1) * (double)width2 / P < width2){
        end2 = (thread_id + 1) * (double)width2 / P;
    } else {
        end2 = width2;
    }
	

	run_mandelbrot(&par2, result2, start2, end2, height2, thread_id);

	// bariera
	pthread_barrier_wait(&b1);

	// scriu in fisier mandelbrot:
	if (thread_id == P - 1) {
		write_output_file(out_filename_mandelbrot, result2, width2, height2);
	}

	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	get_args(argc, argv);

	int i;
	pthread_t tid[P];
	int thread_id[P];

	pthread_barrier_init(&b1, NULL, P);
	pthread_barrier_init(&b2, NULL, P);

	// Julia/Mandelbrot:
	// - se citesc parametrii de intrare
	// - se aloca tabloul cu rezultatul
	// - se ruleaza algoritmul paralelizat
	// - se scrie rezultatul in fisierul de iesire
	// - se elibereaza memoria alocata
	read_input_file(in_filename_julia, &par1);
	read_input_file(in_filename_mandelbrot, &par2);

	width1 = (par1.x_max - par1.x_min) / par1.resolution;
	height1 = (par1.y_max - par1.y_min) / par1.resolution;

	width2 = (par2.x_max - par2.x_min) / par2.resolution;
	height2 = (par2.y_max - par2.y_min) / par2.resolution;
	
	// dimensiune fisier (matrice)
	N1 = width1 * height1;
	N2 = width2 * height2;

	result1 = allocate_memory(width1, height1);
	result2 = allocate_memory(width2, height2);


	// se creeaza thread-urile
	for (i = 0; i < P; i++) {
		thread_id[i] = i;
		pthread_create(&tid[i], NULL, thr_func, &thread_id[i]);
	}

	// se asteapta thread-urile
	for (i = 0; i < P; i++) {
		pthread_join(tid[i], NULL);
	}

	// eliberez memoria alocata dinamic
	free_memory(result1, height1);
	free_memory(result2, height2);

	// sterg barierele
	pthread_barrier_destroy(&b1);
	pthread_barrier_destroy(&b2);

	return 0;
}
