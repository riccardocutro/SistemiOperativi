#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <time.h>

#define FTOK_PATH "Makefile"
#define MSG_PROJ  'Q'
#define SHM_PROJ  'M'
#define SEM_PROJ  'S'

#define OP_PRIMI 0
#define OP_SECONDI 1
#define OP_COFFEE 2
#define OP_CASSA 3

#define MAX_DISH_TYPE 2
#define MAX_COFFEE_TYPE 4

#define MSG_TYPE_PRIMI   1
#define MSG_TYPE_SECONDI 2
#define MSG_TYPE_COFFEE  3
#define MSG_TYPE_CASSA   4

#define RSP_OK 1
#define RSP_FINITO -1

#define SEM_MUTEX_STATS  0
#define SEM_SEATS_PRIMI  1
#define SEM_SEATS_SECONDI 2
#define SEM_SEATS_COFFEE 3
#define SEM_SEATS_CASSA  4
#define SEM_SEATS_TAVOLI 5
#define SEM_START_SIM    6
#define SEM_NUM_TOTAL    7

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
#if defined(__linux__)
    struct seminfo *__buf;
#endif
};

typedef struct {
    long mtype;
    int service_type;
    int quantity;
    //risposta allo specifico utente
    pid_t sender_pid;
} MsgRequest;

//configurazione
typedef struct {
    int nof_users;
    int nof_workers;
    int sim_duration;
    int n_nano_secs;
    int overload_threshold;
    int seats_primi;
    int seats_secondi;
    int seats_coffee;
    int seats_cassa;
    int seats_tavoli;
    int avg_srvc_primi;
    int avg_srvc_secondi;
    int avg_srvc_coffee;
    int avg_srvc_cassa;
    int avg_refill_primi;
    int avg_refill_secondi;
    int max_porzioni_primi;
    int max_porzioni_secondi;
    int nof_pause;
    double price_primi;
    double price_secondi;
    double price_coffee;
} Config;

//statistiche
typedef struct {
    int n_utenti_serviti_daily;
    int n_utenti_serviti_tot;
    int n_utenti_non_serviti_daily;
    int n_utenti_non_serviti_tot;
    int piatti_distr_daily[3];
    int piatti_distr_tot[3];
    int piatti_avanzi_daily[3];
    int piatti_avanzi_tot[3];
    double tempo_attesa_daily[4];
    double tempo_attesa_tot[4];
    int count_attesa_daily[4];
    int count_attesa_tot[4];
    int operatori_attivi_daily;
    int operatori_attivi_sim_tot;
    int n_pause_daily;
    int n_pause_tot;
    double ricavo_daily;
    double ricavo_totale;
} Statistics;

// shared memory
typedef struct {
    Config config;
    Statistics stats;
    int q_primi[MAX_DISH_TYPE];
    int q_secondi[MAX_DISH_TYPE];
    int active_at_station[4];
    int simulation_active;
    int current_day;
} SharedData;

void error_exit(const char *msg);
long get_random_in_range(long center, int percent_var);

int create_sem_set(key_t key, int nsems);
void init_sem(int semid, int semnum, int val);
void sem_reserve(int semid, int semnum);
void sem_release(int semid, int semnum);
int sem_get_val(int sem_id, int sem_num);

void sem_wait_zero(int sem_id, int sem_num);
void wait_sim_time(int minutes, int n_nano_secs_per_min);
void wait_sim_seconds(int seconds, int n_nano_secs_per_min);

void send_msg(int msg_id, MsgRequest *msg);
ssize_t receive_msg(int msg_id, MsgRequest *msg, long type);

#endif