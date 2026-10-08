#include "common.h"

SharedData *shm_ptr = NULL;

void cleanup(int sig) {
    (void)sig;
    if (shm_ptr != NULL) shmdt(shm_ptr);
    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGTERM, cleanup);
    signal(SIGINT, cleanup);
    srand(getpid() * time(NULL));

    if (argc < 5) exit(EXIT_FAILURE);

    int shm_id = atoi(argv[1]);
    int sem_id = atoi(argv[2]);
    int type = atoi(argv[3]);
    int msg_id = atoi(argv[4]);

    shm_ptr = (SharedData *)shmat(shm_id, NULL, 0);
    if (shm_ptr == (void *)-1) error_exit("shmat operatore failed");

    int my_seat_sem = -1;
    char *name = "";
    long my_channel = 0;
    int active_idx = type;
    int stat_idx = 0;

    //configurazioni operatori
    switch (type) {
        case OP_PRIMI:
            my_seat_sem = SEM_SEATS_PRIMI;
            name = "PRIMI";
            my_channel = MSG_TYPE_PRIMI;
            stat_idx = 0;
            break;
        case OP_SECONDI:
            my_seat_sem = SEM_SEATS_SECONDI;
            name = "SECONDI";
            my_channel = MSG_TYPE_SECONDI;
            stat_idx = 1;
            break;
        case OP_COFFEE:
            my_seat_sem = SEM_SEATS_COFFEE;
            name = "COFFEE";
            my_channel = MSG_TYPE_COFFEE;
            stat_idx = 2;
            break;
        default: exit(EXIT_FAILURE);
    }

    sem_reserve(sem_id, my_seat_sem);

    //presenze
    sem_reserve(sem_id, SEM_MUTEX_STATS);
    shm_ptr->stats.operatori_attivi_daily++;
    shm_ptr->active_at_station[active_idx]++;
    sem_release(sem_id, SEM_MUTEX_STATS);

    printf("[OPERATOR %s] Ready. In attesa del semaforo di start...\n", name);

    //avvio sincr
    sem_wait_zero(sem_id, SEM_START_SIM);

    MsgRequest req;
    int pause_effettuate = 0;
    int max_pause = shm_ptr->config.nof_pause;

    while (shm_ptr->simulation_active) {

        //attende il messaggio
        if (receive_msg(msg_id, &req, my_channel) == -1) break;
        if (!shm_ptr->simulation_active) break;

        //tempi di srrvizio
        int avg_time_sec = (type == OP_PRIMI) ? shm_ptr->config.avg_srvc_primi :
                           (type == OP_SECONDI) ? shm_ptr->config.avg_srvc_secondi :
                           shm_ptr->config.avg_srvc_coffee;

        int range_perc = (type == OP_COFFEE) ? 80 : 50;

        int duration_sec = get_random_in_range(avg_time_sec, range_perc);

        wait_sim_seconds(duration_sec, shm_ptr->config.n_nano_secs);

        //gestione del piatto
        int dish_idx = req.quantity;
        int status = RSP_OK;

        sem_reserve(sem_id, SEM_MUTEX_STATS);
        if (type == OP_PRIMI) {
            if (shm_ptr->q_primi[dish_idx] > 0) {
                shm_ptr->q_primi[dish_idx]--;
                shm_ptr->stats.piatti_distr_daily[stat_idx]++;
            } else status = RSP_FINITO;
        } else if (type == OP_SECONDI) {
             if (shm_ptr->q_secondi[dish_idx] > 0) {
                shm_ptr->q_secondi[dish_idx]--;
                shm_ptr->stats.piatti_distr_daily[stat_idx]++;
            } else status = RSP_FINITO;
        } else {
            shm_ptr->stats.piatti_distr_daily[stat_idx]++;
        }
        sem_release(sem_id, SEM_MUTEX_STATS);

        req.mtype = req.sender_pid;
        req.service_type = status;
        send_msg(msg_id, &req);

        //pausa
        if (pause_effettuate < max_pause && (rand() % 100) < 15) {
            sem_reserve(sem_id, SEM_MUTEX_STATS);

            if (shm_ptr->active_at_station[active_idx] > 1) {
                shm_ptr->active_at_station[active_idx]--;
                shm_ptr->stats.n_pause_daily++;
                sem_release(sem_id, SEM_MUTEX_STATS);

                sem_release(sem_id, my_seat_sem);

                wait_sim_time(10, shm_ptr->config.n_nano_secs);

                sem_reserve(sem_id, my_seat_sem);

                sem_reserve(sem_id, SEM_MUTEX_STATS);
                shm_ptr->active_at_station[active_idx]++;
                sem_release(sem_id, SEM_MUTEX_STATS);

                pause_effettuate++;
            } else {
                sem_release(sem_id, SEM_MUTEX_STATS);
            }
        }
    }

    sem_reserve(sem_id, SEM_MUTEX_STATS);
    shm_ptr->active_at_station[active_idx]--;
    sem_release(sem_id, SEM_MUTEX_STATS);

    sem_release(sem_id, my_seat_sem);

    cleanup(0);
    return 0;
}