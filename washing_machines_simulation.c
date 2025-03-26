/* Single-server queuing system with automated seed generation and custom random generator, con traza y comentarios detallados */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "lcgrand.h"  /* Header file for random-number generator */

#define Q_LIMIT 100  // Límite máximo de clientes en cola.
#define BUSY      1  // Estado ocupado del servidor.
#define IDLE      0  // Estado libre del servidor.

/* Variables globales */
int next_event_type;                  // Tipo del siguiente evento (1: llegada | arribo, 2: salida).
int num_custs_delayed;                // Número de clientes atendidos (que pasaron por el sistema).
int num_in_q;                         // Número de clientes actualmente en cola.
int server_status;                    // Estado actual del servidor (BUSY o IDLE).
float area_num_in_q;                  // Área bajo la curva del número de clientes en cola (para promedios).
float area_server_status;             // Área bajo la curva de ocupación del servidor.
float sim_time;                       // Reloj de simulación.
float time_arrival[Q_LIMIT+1];        // Arreglo con los tiempos de llegada de los clientes en cola.
float time_last_event;                // Tiempo del último evento.
float time_next_event[3];             // Tiempos para los próximos eventos (índice 1: llegada, 2: salida).
float total_of_delays;                // Acumulador de todas las demoras de los clientes.
float total_service_time;             // Suma de los tiempos de servicio de todos los clientes atendidos

// Tiempos y probabilidades de llegadas y servicios según el enunciado.
float arrival_times[] = {5, 10, 15, 20, 25, 30, 35, 40};
float arrival_probs[] = {0.05, 0.05, 0.1, 0.1, 0.3, 0.2, 0.15, 0.05};
float service_times[] = {10, 20, 30, 40};
float service_probs[] = {0.15, 0.25, 0.4, 0.2};

/* Prototipos de funciones */
void initialize(void);
void timing(void);
void arrive(void);
void depart(void);
void report(void);
void update_time_avg_stats(void);
float gen_next_interarrive(void);
float gen_next_depart(void);

int main() {
    initialize(); // Inicializar parámetros y estado.

    // Bucle principal de simulación hasta los 300 minutos.
    while (sim_time < 300) {
        timing();                // Determinar cuál será el próximo evento.
        update_time_avg_stats(); // Actualizar estadísticas acumuladas.

        // Ejecutar la acción según el tipo de evento.
        switch (next_event_type) {
            case 1:
                printf("\nEvento: LLEGADA en t=%.2f\n", sim_time);
                arrive();
                break;
            case 2:
                printf("\nEvento: SALIDA en t=%.2f\n", sim_time);
                depart();
                break;
        }

        // Mostrar estado actual para seguimiento.
        printf("Estado actual: Clientes en cola=%d, Servidor=%s\n", num_in_q, server_status == BUSY ? "OCUPADO" : "LIBRE");
        printf("Próxima llegada: %.2f, Próxima salida: %.2f\n", time_next_event[1], time_next_event[2]);
    }

    report(); // Mostrar resumen de resultados.
    return 0;
}

void initialize(void) {
    // Inicialización de variables.
    sim_time = 0.0;
    server_status = IDLE;
    num_in_q = 0;
    time_last_event = 0.0;

    num_custs_delayed = 0;       // Aún no hay clientes atendidos.
    total_of_delays = 0.0;       // No hay demoras acumuladas.
    total_service_time = 0.0;    // Inicializamos en cero el tiempo de servicio total.
    area_num_in_q = 0.0;         // Área inicial de clientes en cola.
    area_server_status = 0.0;    // Área inicial de utilización del servidor.

    // Configurar semillas automáticas para generadores de llegada y servicio.
    srand(time(NULL));
    lcgrandst(rand() % 2147483646 + 1, 1); // Semilla para llegadas.
    lcgrandst(rand() % 2147483646 + 1, 2); // Semilla para servicios.

    // Programar primer evento de llegada y fijar salida como infinito.
    time_next_event[1] = sim_time + gen_next_interarrive();
    time_next_event[2] = 1.0e+30;

    printf("Simulación iniciada. Primera llegada programada en: %.2f\n", time_next_event[1]);
}

void timing(void) {
    // Determinar el tipo de próximo evento, buscando el tiempo más cercano.
    int i;
    float min_time_next_event = 1.0e+29;

    next_event_type = 0; // Iniciar sin evento determinado.
    for (i = 1; i <= 2; ++i) {
        if (time_next_event[i] < min_time_next_event) {
            min_time_next_event = time_next_event[i];
            next_event_type = i;
        }
    }

    // Validar que haya próximos eventos.
    if (next_event_type == 0) {
        printf("\nError: No hay eventos futuros en tiempo %f\n", sim_time);
        exit(1);
    }

    // Avanzar el reloj de simulación al próximo evento.
    sim_time = min_time_next_event;
}

void arrive(void) {
    // Gestionar llegada de un cliente.
    float delay;

    // Programar próxima llegada.
    time_next_event[1] = sim_time + gen_next_interarrive();

    if (server_status == BUSY) {
        // Si el servidor está ocupado, se forma la cola.
        ++num_in_q;
        if (num_in_q > Q_LIMIT) {
            printf("\nError: La cola excede el límite máximo.\n");
            exit(2);
        }
        time_arrival[num_in_q] = sim_time; // Guardar el tiempo de llegada.
        printf("Cliente agregado a cola. Total en cola: %d\n", num_in_q);
    } else {
        // Servidor libre: el cliente es atendido inmediatamente.
        delay = 0.0; // No hay demora.
        total_of_delays += delay;
        ++num_custs_delayed;
        server_status = BUSY;

        float service_time = gen_next_depart();
        total_service_time += service_time;  // Acumular tiempo de servicio
        printf("Servidor libre: comenzando servicio (duración: %.2f).\n", service_time);
        time_next_event[2] = sim_time + service_time; // Programar salida.
    }
}

void depart(void) {
    // Gestionar salida (fin de servicio).
    int i;
    float delay;

    if (num_in_q == 0) {
        // Si no hay clientes en cola, servidor queda libre.
        server_status = IDLE;
        time_next_event[2] = 1.0e+30;
        printf("Cola vacía: servidor pasa a libre.\n");
    } else {
        // Hay clientes esperando: atender siguiente.
        --num_in_q;
        delay = sim_time - time_arrival[1]; // Calcular demora de cliente que pasa al servidor.
        total_of_delays += delay;
        ++num_custs_delayed;

        // Desplazar tiempos de llegada para mantener la cola actualizada.
        for (i = 1; i <= num_in_q; ++i)
            time_arrival[i] = time_arrival[i + 1];

        float service_time = gen_next_depart();
        total_service_time += service_time;  // Acumular tiempo de servicio
        time_next_event[2] = sim_time + service_time;
        printf("Cliente de la cola entra al servidor. Demora: %.2f. Salida programada en %.2f.\n", delay, time_next_event[2]);
    }
}

void report(void) {
    // Cálculo de métricas ya existentes
    float utilization = (area_server_status / sim_time) * 100.0;
    float avg_delay_in_queue = total_of_delays / num_custs_delayed; // Demora promedio en cola
    float avg_service_time   = total_service_time / num_custs_delayed; 
    float avg_time_in_system = avg_delay_in_queue + avg_service_time;

    // Número promedio de clientes en cola
    float avg_num_in_queue   = area_num_in_q / sim_time;

    // Número promedio de clientes en el sistema (cola + siendo atendido)
    float avg_num_in_system  = (area_num_in_q + area_server_status) / sim_time;

    printf("\n\n===== RESULTADOS DE LA SIMULACIÓN =====\n\n");
    // i. Utilización de la máquina lavadora de autos
    printf("i. Utilización de la máquina lavadora de autos: %.2f%%\n", utilization);

    // ii. Tiempo promedio que un cliente pasa en el comercio
    printf("ii. Tiempo promedio que un cliente pasa en el comercio: %.2f minutos\n", avg_time_in_system);

    // iii. Tiempo promedio de un cliente en la cola
    printf("iii. Tiempo promedio de un cliente en la cola: %.2f minutos\n", avg_delay_in_queue);

    // iv. Número promedio de clientes en el comercio
    printf("iv. Número promedio de clientes en el comercio: %.2f\n", avg_num_in_system);

    // v. Número promedio de clientes en la cola
    printf("v. Número promedio de clientes en la cola: %.2f\n", avg_num_in_queue);

    // Mostrar tiempo total simulado, por claridad
    printf("\n- Tiempo total simulado: %.2f minutos\n", sim_time);

    // Umbral de recomendación
    if (utilization > 85.0) {
        printf("RECOMENDACIÓN: Es necesario agregar otra máquina.\n");
    } else {
        printf("RECOMENDACIÓN: No es necesario agregar otra máquina.\n");
    }
}

void update_time_avg_stats(void) {
    // Actualizar estadísticas de áreas acumuladas.
    float time_since_last_event;

    time_since_last_event = sim_time - time_last_event;
    time_last_event = sim_time;

    area_num_in_q += num_in_q * time_since_last_event; // Área bajo la curva de la cola.
    area_server_status += server_status * time_since_last_event; // Área bajo la curva de ocupación.
}

float gen_next_interarrive() {
    // Generar próximo tiempo de llegada basado en tabla de probabilidades.
    float r = lcgrand(1);
    int index;

    if (r <= 0.05) index = 0;
    else if (r <= 0.10) index = 1;
    else if (r <= 0.20) index = 2;
    else if (r <= 0.30) index = 3;
    else if (r <= 0.60) index = 4;
    else if (r <= 0.80) index = 5;
    else if (r <= 0.95) index = 6;
    else index = 7;

    // Retornar tiempo asociado.
    switch (index) {
        case 0: return arrival_times[0];
        case 1: return arrival_times[1];
        case 2: return arrival_times[2];
        case 3: return arrival_times[3];
        case 4: return arrival_times[4];
        case 5: return arrival_times[5];
        case 6: return arrival_times[6];
        case 7: return arrival_times[7];
        default: return arrival_times[7]; // Fallback.
    }
}

float gen_next_depart() {
    // Generar tiempo de servicio basado en tabla de probabilidades.
    float r = lcgrand(2);
    int index;

    if (r <= 0.15) index = 0;
    else if (r <= 0.40) index = 1;
    else if (r <= 0.80) index = 2;
    else index = 3;

    // Retornar tiempo de servicio asociado.
    switch (index) {
        case 0: return service_times[0];
        case 1: return service_times[1];
        case 2: return service_times[2];
        case 3: return service_times[3];
        default: return service_times[3];
    }
}