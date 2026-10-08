#include "common.h"
#include <time.h>

SharedData *shm_ptr = NULL;

#define IDX_PRIMI   0
#define IDX_SECONDI 1
#define IDX_COFFEE  2
#define IDX_TAVOLI  3

void cleanup(int sig) {
    (void)sig;
    if (shm_ptr != NULL) shmdt(shm_ptr);
    exit(0);
}

void registra_attesa(int sem_id, int station_idx, double seconds) {
    sem_reserve(sem_id, SEM_MUTEX_STATS);
    shm_ptr->stats.tempo_attesa_daily[station_idx] += seconds;
    shm_ptr->stats.count_attesa_daily[station_idx]++;
    sem_release(sem_id, SEM_MUTEX_STATS);
}

int richiedi_piatto_timed(int msg_id, int sem_id, pid_t my_pid, int type_req, int dish_idx, int stats_idx) {
    MsgRequest msg;
    msg.mtype = type_req;
    msg.sender_pid = my_pid;
    msg.quantity = dish_idx; //tipo di piatto

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    send_msg(msg_id, &msg);
    if (receive_msg(msg_id, &msg, my_pid) == -1) return 0;
    clock_gettime(CLOCK_MONOTONIC, &t_end);

    double attesa = (t_end.tv_sec - t_start.tv_sec) + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
    registra_attesa(sem_id, stats_idx, attesa);
    return (msg.service_type == RSP_OK);
}

// Legge menu e stabilisce cosa vorrebbe
void stabilisci_desideri(int *voglio_primo, int *voglio_secondo, int *voglio_coffee) {
    FILE *f = fopen("menu.txt", "r");
    if (!f) {
        *voglio_primo = 1; *voglio_secondo = 1; *voglio_coffee = 0;
        return;
    }

    *voglio_primo = 0; *voglio_secondo = 0; *voglio_coffee = 0;
    int esiste_primo = 0, esiste_secondo = 0;

    char line[64];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "Primi")) esiste_primo = 1;
        if (strstr(line, "Secondi")) esiste_secondo = 1;
        if (strstr(line, "Dolce_Caffe") && (rand() % 100 < 60)) *voglio_coffee = 1;
    }
    fclose(f);

    if (esiste_primo && (rand() % 100 < 70)) *voglio_primo = 1;
    if (esiste_secondo && (rand() % 100 < 70)) *voglio_secondo = 1;

    if (!*voglio_primo && !*voglio_secondo) {
        if (esiste_primo && esiste_secondo) {
            if (rand() % 2 == 0) *voglio_primo = 1; else *voglio_secondo = 1;
        } else if (esiste_primo) *voglio_primo = 1;
        else if (esiste_secondo) *voglio_secondo = 1;
    }
}

int main(int argc, char *argv[]) {
    signal(SIGTERM, cleanup);
    srand(getpid() * time(NULL));

    if (argc < 4) exit(EXIT_FAILURE);
    int shm_id = atoi(argv[1]);
    int sem_id = atoi(argv[2]);
    int msg_id = atoi(argv[3]);
    pid_t my_pid = getpid();

    shm_ptr = (SharedData *)shmat(shm_id, NULL, 0);
    if (shm_ptr == (void *)-1) error_exit("shmat user failed");

    sem_wait_zero(sem_id, SEM_START_SIM);

    int desideri[3] = {0, 0, 0};
    stabilisci_desideri(&desideri[0], &desideri[1], &desideri[2]);

    int visitati[3] = {0, 0, 0};
    int piatti_mangiati = 0;
    double conto_totale = 0.0;
    int continua = 1;

    while (continua) {
        int scelta_migliore = -1;
        int max_liberi = -1;

        int posti_liberi[3];
        //scelta della coda
        posti_liberi[0] = sem_get_val(sem_id, SEM_SEATS_PRIMI);
        posti_liberi[1] = sem_get_val(sem_id, SEM_SEATS_SECONDI);
        posti_liberi[2] = sem_get_val(sem_id, SEM_SEATS_COFFEE);

        for (int i = 0; i < 3; i++) {
            if (desideri[i] && !visitati[i]) {
                int is_better = 0;
                if (posti_liberi[i] > max_liberi) {
                    is_better = 1;
                } else if (posti_liberi[i] == max_liberi) {
                    if (rand() % 2 == 0) is_better = 1;
                }
                if (is_better) {
                    max_liberi = posti_liberi[i];
                    scelta_migliore = i;
                }
            }
        }

        if (scelta_migliore == -1) break;

        int ottenuto = 0;

        if (scelta_migliore == 0) { // PRIMI
            sem_reserve(sem_id, SEM_SEATS_PRIMI);

            int piatto = rand() % MAX_DISH_TYPE;
            ottenuto = richiedi_piatto_timed(msg_id, sem_id, my_pid, MSG_TYPE_PRIMI, piatto, IDX_PRIMI);
            if (!ottenuto) ottenuto = richiedi_piatto_timed(msg_id, sem_id, my_pid, MSG_TYPE_PRIMI, (piatto+1)%MAX_DISH_TYPE, IDX_PRIMI);
            if (ottenuto) conto_totale += shm_ptr->config.price_primi;

            sem_release(sem_id, SEM_SEATS_PRIMI);
        }
        else if (scelta_migliore == 1) { // SECONDI
            sem_reserve(sem_id, SEM_SEATS_SECONDI);

            int piatto = rand() % MAX_DISH_TYPE;
            ottenuto = richiedi_piatto_timed(msg_id, sem_id, my_pid, MSG_TYPE_SECONDI, piatto, IDX_SECONDI);
            if (!ottenuto) ottenuto = richiedi_piatto_timed(msg_id, sem_id, my_pid, MSG_TYPE_SECONDI, (piatto+1)%MAX_DISH_TYPE, IDX_SECONDI);
            if (ottenuto) conto_totale += shm_ptr->config.price_secondi;

            sem_release(sem_id, SEM_SEATS_SECONDI);
        }
        else if (scelta_migliore == 2) { // CAFFE
            sem_reserve(sem_id, SEM_SEATS_COFFEE);

            ottenuto = richiedi_piatto_timed(msg_id, sem_id, my_pid, MSG_TYPE_COFFEE, rand()%4, IDX_COFFEE);
            if (ottenuto) conto_totale += shm_ptr->config.price_coffee;

            sem_release(sem_id, SEM_SEATS_COFFEE);
        }

        if (ottenuto) piatti_mangiati++;
        visitati[scelta_migliore] = 1;

        int desideri_rimasti = 0;
        for(int i=0; i<3; i++) if(desideri[i] && !visitati[i]) desideri_rimasti=1;

        if (desideri_rimasti) {
            if (rand() % 100 < 20) continua = 0;
        } else {
            continua = 0;
        }
    }

    if (piatti_mangiati == 0) {
        sem_reserve(sem_id, SEM_MUTEX_STATS);
        shm_ptr->stats.n_utenti_non_serviti_daily++;
        sem_release(sem_id, SEM_MUTEX_STATS);
        cleanup(0);
    }

    // CASSA
    //sem_reserve(sem_id, SEM_SEATS_CASSA);
    MsgRequest msg_cassa;
    msg_cassa.mtype = MSG_TYPE_CASSA;
    msg_cassa.sender_pid = my_pid;
    msg_cassa.quantity = (int)(conto_totale * 100);
    send_msg(msg_id, &msg_cassa);
    receive_msg(msg_id, &msg_cassa, my_pid);
    //sem_release(sem_id, SEM_SEATS_CASSA);

    sem_reserve(sem_id, SEM_MUTEX_STATS);
    shm_ptr->stats.n_utenti_serviti_daily++;
    sem_release(sem_id, SEM_MUTEX_STATS);

    // TAVOLI
    struct timespec t_s, t_e;
    clock_gettime(CLOCK_MONOTONIC, &t_s);
    sem_reserve(sem_id, SEM_SEATS_TAVOLI);
    clock_gettime(CLOCK_MONOTONIC, &t_e);
    registra_attesa(sem_id, IDX_TAVOLI, (t_e.tv_sec - t_s.tv_sec) + (t_e.tv_nsec - t_s.tv_nsec)/1e9);

    // MANGIARE
    printf("[UTENTE %d] Mangio %d piatti...\n", my_pid, piatti_mangiati);
    int minuti_pasto = 5 * piatti_mangiati;
    wait_sim_time(minuti_pasto, shm_ptr->config.n_nano_secs);

    sem_release(sem_id, SEM_SEATS_TAVOLI);

    cleanup(0);
    return 0;
}