#include <signal.h>
#include <errno.h>

#include "biblioteca.h"

/* Estructura que almacena los datos de una reserva. */
typedef struct {
	int posiciones[ANCHO_AULA][ALTO_AULA];
	int cantidad_de_personas;
	int rescatistas_disponibles;
	
	pthread_mutex_t posiciones_mx;
	pthread_mutex_t cantidad_de_personas_mx;
	pthread_mutex_t rescatistas_disponibles_mx;
	pthread_cond_t rescatistas_disponibles_cv;
} t_aula;

/* Estructura que almacena los argumentos que se le pasan a un thread. */
typedef struct {
	int socket_fd;
	t_aula *el_aula;
} thread_args;

/* Sin cambios, lo ejecuta el main thread sin concurrencia */
void t_aula_iniciar_vacia(t_aula *el_aula)
{
	int i, j;
	for(i = 0; i < ANCHO_AULA; i++)
		for (j = 0; j < ALTO_AULA; j++)
			el_aula->posiciones[i][j] = 0;
	el_aula->cantidad_de_personas = 0;
	el_aula->rescatistas_disponibles = RESCATISTAS;

  pthread_mutex_init(&(el_aula->posiciones_mx), NULL);
	pthread_mutex_init(&(el_aula->cantidad_de_personas_mx), NULL);
	pthread_mutex_init(&(el_aula->rescatistas_disponibles_mx), NULL);
	pthread_cond_init(&(el_aula->rescatistas_disponibles_cv), NULL);
}

/* modifica el aula (hay que manejar la concurrencia) */
void t_aula_ingresar(t_aula *el_aula, t_persona *alumno)
{
	pthread_mutex_lock(&(el_aula->cantidad_de_personas_mx));
	el_aula->cantidad_de_personas++;
	pthread_mutex_unlock(&(el_aula->cantidad_de_personas_mx));

	pthread_mutex_lock(&(el_aula->posiciones_mx));
	el_aula->posiciones[alumno->posicion_fila][alumno->posicion_columna]++;
	pthread_mutex_unlock(&(el_aula->posiciones_mx));
}

/* modifica el aula (hay que manejar la concurrencia) */
t_comando intentar_moverse(t_aula *el_aula, t_persona *alumno, t_direccion dir)
{
	int fila = alumno->posicion_fila;
	int columna = alumno->posicion_columna;
	alumno->salio = direccion_moverse_hacia(dir, &fila, &columna);

	bool entre_limites = (fila >= 0) && (columna >= 0) &&
	     (fila < ALTO_AULA) && (columna < ANCHO_AULA);
	
	pthread_mutex_lock(&(el_aula->posiciones_mx));
	bool pudo_moverse = alumno->salio ||
	    (entre_limites && el_aula->posiciones[fila][columna] < MAXIMO_POR_POSICION);
	
	if (pudo_moverse)
	{
		if (!alumno->salio)
			el_aula->posiciones[fila][columna]++;
		el_aula->posiciones[alumno->posicion_fila][alumno->posicion_columna]--;
		alumno->posicion_fila = fila;
		alumno->posicion_columna = columna;
	}
	pthread_mutex_unlock(&(el_aula->posiciones_mx));
	
	return pudo_moverse;
}

/* modifica el aula (hay que manejar la concurrencia) */
void t_aula_liberar(t_aula *el_aula, t_persona *alumno)
{
	pthread_mutex_lock(&(el_aula->cantidad_de_personas_mx));
	el_aula->cantidad_de_personas--;
	pthread_mutex_unlock(&(el_aula->cantidad_de_personas_mx));
}

/* modifica el aula (hay que manejar la concurrencia) */
void colocar_mascara(t_aula *el_aula, t_persona *alumno)
{
	printf("Esperando rescatista. Ya casi %s!\n", alumno->nombre);

	pthread_mutex_lock(&(el_aula->rescatistas_disponibles_mx));
	while(el_aula->rescatistas_disponibles == 0)
		pthread_cond_wait(&(el_aula->rescatistas_disponibles_cv), &(el_aula->rescatistas_disponibles_mx));
	el_aula->rescatistas_disponibles--;
	pthread_mutex_unlock(&(el_aula->rescatistas_disponibles_mx));
	
	alumno->tiene_mascara = true;

	pthread_mutex_lock(&(el_aula->rescatistas_disponibles_mx));
	el_aula->rescatistas_disponibles++;
	pthread_cond_signal(&(el_aula->rescatistas_disponibles_cv));
	pthread_mutex_unlock(&(el_aula->rescatistas_disponibles_mx));
}

/* no modifica el aula de forma directa */
static void terminar_servidor_de_alumno(int socket_fd, t_aula *el_aula, t_persona *alumno) 
{
	printf(">> Se interrumpió la comunicación con una consola.\n");
	close(socket_fd);
	
	t_aula_liberar(el_aula, alumno);
}

/* no modifica el aula de forma directa */
void terminar_thread(void* args)
{
	free(args);
	pthread_exit(NULL);
}

/* no modifica el aula de forma directa */
void *atendedor_de_alumno(void* args)
{
	int socket_fd = ((thread_args*) args)->socket_fd;
	t_aula *el_aula = ((thread_args*) args)->el_aula;

	t_persona alumno;
	t_persona_inicializar(&alumno);
	
	if (recibir_nombre_y_posicion(socket_fd, &alumno) != 0) {
		/* O la consola cortó la comunicación, o hubo un error. Cerramos todo. */
		terminar_servidor_de_alumno(socket_fd, NULL, NULL);
		terminar_thread(args);
	}
	
	printf("Atendiendo a %s en la posicion (%d, %d)\n", 
			alumno.nombre, alumno.posicion_fila, alumno.posicion_columna);
		
	t_aula_ingresar(el_aula, &alumno);
	
	/// Loop de espera de pedido de movimiento.
	for(;;) {
		t_direccion direccion;
		
		/// Esperamos un pedido de movimiento.
		if (recibir_direccion(socket_fd, &direccion) != 0) {
			/* O la consola cortó la comunicación, o hubo un error. Cerramos todo. */
			terminar_servidor_de_alumno(socket_fd, el_aula, &alumno);
			terminar_thread(args);
		}
		
		/// Tratamos de movernos en nuestro modelo
		bool pudo_moverse = intentar_moverse(el_aula, &alumno, direccion);

		printf("%s se movio a: (%d, %d)\n", alumno.nombre, alumno.posicion_fila, alumno.posicion_columna);

		/// Avisamos que ocurrio
		enviar_respuesta(socket_fd, pudo_moverse ? OK : OCUPADO);		
		
		if (alumno.salio)
			break;
	}

	colocar_mascara(el_aula, &alumno);
	t_aula_liberar(el_aula, &alumno);
	
	enviar_respuesta(socket_fd, LIBRE);
	printf("Listo, %s es libre!\n", alumno.nombre);
	
	terminar_thread(args);
}

int main(void)
{
	//signal(SIGUSR1, signal_terminar);
	int socketfd_cliente, socket_servidor, socket_size;
	struct sockaddr_in local, remoto;

	/* Crear un socket de tipo INET con TCP (SOCK_STREAM). */
	if ((socket_servidor = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("creando socket");
	}

	/* Crear nombre, usamos INADDR_ANY para indicar que cualquiera puede conectarse aquí. */
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(PORT);
	
	if (bind(socket_servidor, (struct sockaddr *)&local, sizeof(local)) == -1) {
		perror("haciendo bind");
	}

	/* Escuchar en el socket y permitir 5 conexiones en espera. */
	if (listen(socket_servidor, 5) == -1) {
		perror("escuchando");
	}
	
	t_aula el_aula;
	t_aula_iniciar_vacia(&el_aula);
	
	/// Aceptar conexiones entrantes.
	socket_size = sizeof(remoto);
	for(;;){		
		if (-1 == (socketfd_cliente = 
					accept(socket_servidor, (struct sockaddr*) &remoto, (socklen_t*) &socket_size)))
		{			
			printf("!! Error al aceptar conexion\n");
		}
		else
		{
			thread_args* args = (thread_args*) malloc(1 * sizeof(thread_args));
			args->socket_fd = socketfd_cliente;
			args->el_aula = &el_aula;

			pthread_t thread;
			if(pthread_create(&thread, NULL, atendedor_de_alumno, (void*) args))
			{
			   fprintf(stderr,"Error - pthread_create()\n");
			   exit(EXIT_FAILURE);
			}
			if(pthread_detach(thread))
			{
				fprintf(stderr,"Error - pthread_detach()\n");
			  exit(EXIT_FAILURE);
			}
		}
	}

	return 0;
}


