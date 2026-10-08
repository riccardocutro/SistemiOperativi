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

    if (argc < 4) {
        fprintf(stderr, "Uso: %s shm_id sem_id msg_id\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int shm_id = atoi(argv[1]);
    int sem_id = atoi(argv[2]);
    int msg_id = atoi(argv[3]);

    shm_ptr = (SharedData *)shmat(shm_id, NULL, 0);
    if (shm_ptr == (void *)-1) error_exit("shmat cassa failed");

    int my_seat_sem = SEM_SEATS_CASSA;
    int active_idx = 3;
    int max_pause = shm_ptr->config.nof_pause;
    int pause_effettuate = 0;

    sem_reserve(sem_id, my_seat_sem);

    //registra statistiche
    sem_reserve(sem_id, SEM_MUTEX_STATS);
    shm_ptr->stats.operatori_attivi_daily++;
    shm_ptr->active_at_station[active_idx]++;
    sem_release(sem_id, SEM_MUTEX_STATS);

    printf("[CASSIERE %d] Postazione acquisita. Attendo semaforo di avvio...\n", getpid());

    //avvio sincronizzazione
    sem_wait_zero(sem_id, SEM_START_SIM);

    printf("[CASSIERE] Simulazione avviata. Apro cassa.\n");

    MsgRequest req;

    while (shm_ptr->simulation_active) {

        // Ricezione messaggio bloccante
        if (receive_msg(msg_id, &req, MSG_TYPE_CASSA) == -1) break;
        if (!shm_ptr->simulation_active) break;

        //tempo servizio
        int duration_sec = get_random_in_range(shm_ptr->config.avg_srvc_cassa, 20);

        wait_sim_seconds(duration_sec, shm_ptr->config.n_nano_secs);

        double incasso_reale = (double)req.quantity / 100.0;

        sem_reserve(sem_id, SEM_MUTEX_STATS);
        shm_ptr->stats.ricavo_daily += incasso_reale;
        sem_release(sem_id, SEM_MUTEX_STATS);

        //invio risposta
        req.mtype = req.sender_pid;
        req.service_type = RSP_OK;
        send_msg(msg_id, &req);

        //pausa
        if (pause_effettuate < max_pause && (rand() % 100) < 15) {

            sem_reserve(sem_id, SEM_MUTEX_STATS);

            if (shm_ptr->active_at_station[active_idx] > 1) {

                shm_ptr->active_at_station[active_idx]--;
                shm_ptr->stats.n_pause_daily++;
                sem_release(sem_id, SEM_MUTEX_STATS);

                sem_release(sem_id, my_seat_sem);

                printf("[CASSIERE] Vado in pausa (Pausa %d)...\n", pause_effettuate + 1);

                wait_sim_time(15, shm_ptr->config.n_nano_secs);

                 sem_reserve(sem_id, my_seat_sem);

                sem_reserve(sem_id, SEM_MUTEX_STATS);
                shm_ptr->active_at_station[active_idx]++;
                sem_release(sem_id, SEM_MUTEX_STATS);

                pause_effettuate++;
                printf("[CASSIERE] Tornato dalla pausa.\n");

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