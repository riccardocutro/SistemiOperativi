#include "common.h"

int shm_id = -1;
int sem_id = -1;
int msg_id = -1;
SharedData *shm_ptr = NULL;

pid_t *child_pids = NULL;
int total_child_count = 0;
int max_child_pids = 0;

void cleanup_resources(int sig) {
    (void)sig;
    printf("\n[RESPONSABILE] Ricevuto segnale di chiusura/Termine simulazione.\n");

    if (child_pids != NULL) {
        for (int i = 0; i < total_child_count; i++) {
            if (child_pids[i] > 0) kill(child_pids[i], SIGTERM);
        }
        free(child_pids);
    }

    if (shm_ptr != NULL) shmdt(shm_ptr);
    if (shm_id != -1) shmctl(shm_id, IPC_RMID, NULL);
    if (sem_id != -1) semctl(sem_id, 0, IPC_RMID);
    if (msg_id != -1) msgctl(msg_id, IPC_RMID, NULL);

    printf("[RESPONSABILE] Risorse rimosse.\n");
    exit(0);
}

void parse_config(const char *filename, Config *cfg) {
    FILE *f = fopen(filename, "r");
    if (!f) error_exit("Impossibile aprire file di configurazione");

    char line[256];
    char key[128];
    char val_str[128];

    memset(cfg, 0, sizeof(Config));

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (sscanf(line, "%[^=]=%s", key, val_str) == 2) {
            if (strcmp(key, "SIM_DURATION") == 0) cfg->sim_duration = atoi(val_str);
            else if (strcmp(key, "N_NANO_SECS") == 0) cfg->n_nano_secs = atoi(val_str);
            else if (strcmp(key, "NOF_WORKERS") == 0) cfg->nof_workers = atoi(val_str);
            else if (strcmp(key, "NOF_USERS") == 0) cfg->nof_users = atoi(val_str);
            else if (strcmp(key, "OVERLOAD_THRESHOLD") == 0) cfg->overload_threshold = atoi(val_str);
            else if (strcmp(key, "SEATS_PRIMI") == 0) cfg->seats_primi = atoi(val_str);
            else if (strcmp(key, "SEATS_SECONDI") == 0) cfg->seats_secondi = atoi(val_str);
            else if (strcmp(key, "SEATS_COFFEE") == 0) cfg->seats_coffee = atoi(val_str);
            else if (strcmp(key, "SEATS_CASSA") == 0) cfg->seats_cassa = atoi(val_str);
            else if (strcmp(key, "SEATS_TAVOLI") == 0) cfg->seats_tavoli = atoi(val_str);
            else if (strcmp(key, "AVG_SRVC_PRIMI") == 0) cfg->avg_srvc_primi = atoi(val_str);
            else if (strcmp(key, "AVG_SRVC_SECONDI") == 0) cfg->avg_srvc_secondi = atoi(val_str);
            else if (strcmp(key, "AVG_SRVC_COFFEE") == 0) cfg->avg_srvc_coffee = atoi(val_str);
            else if (strcmp(key, "AVG_SRVC_CASSA") == 0) cfg->avg_srvc_cassa = atoi(val_str);
            else if (strcmp(key, "AVG_REFILL_PRIMI") == 0) cfg->avg_refill_primi = atoi(val_str);
            else if (strcmp(key, "AVG_REFILL_SECONDI") == 0) cfg->avg_refill_secondi = atoi(val_str);
            else if (strcmp(key, "MAX_PORZIONI_PRIMI") == 0) cfg->max_porzioni_primi = atoi(val_str);
            else if (strcmp(key, "MAX_PORZIONI_SECONDI") == 0) cfg->max_porzioni_secondi = atoi(val_str);
            else if (strcmp(key, "PRICE_PRIMI") == 0) cfg->price_primi = atof(val_str);
            else if (strcmp(key, "PRICE_SECONDI") == 0) cfg->price_secondi = atof(val_str);
            else if (strcmp(key, "PRICE_COFFEE") == 0) cfg->price_coffee = atof(val_str);
            else if (strcmp(key, "NOF_PAUSE") == 0) cfg->nof_pause = atoi(val_str);
        }
    }
    fclose(f);
    printf("[RESPONSABILE] Configurazione caricata da %s\n", filename);
}

void spawn_worker(int type, int shm_id, int sem_id, int msg_id) {
    pid_t pid = fork();
    if (pid == -1) error_exit("fork worker failed");

    if (pid == 0) {
        char shm_str[16], sem_str[16], type_str[16], msg_str[16];
        sprintf(shm_str, "%d", shm_id);
        sprintf(sem_str, "%d", sem_id);
        sprintf(type_str, "%d", type);
        sprintf(msg_str, "%d", msg_id);

        if (type == OP_CASSA) {
            char *args[] = {"./operatore_cassa", shm_str, sem_str, msg_str, NULL};
            execve("./operatore_cassa", args, NULL);
        } else {
            char *args[] = {"./operatore", shm_str, sem_str, type_str, msg_str, NULL};
            execve("./operatore", args, NULL);
        }
        exit(EXIT_FAILURE);
    }
    child_pids[total_child_count++] = pid;
}

void spawn_user(int shm_id, int sem_id, int msg_id) {
    pid_t pid = fork();
    if (pid == 0) {
        char shm_str[16], sem_str[16], msg_str[16];
        sprintf(shm_str, "%d", shm_id);
        sprintf(sem_str, "%d", sem_id);
        sprintf(msg_str, "%d", msg_id);

        char *args[] = {"./utente", shm_str, sem_str, msg_str, NULL};
        execve("./utente", args, NULL);
        exit(EXIT_FAILURE);
    }
    if (total_child_count < max_child_pids) {
        child_pids[total_child_count++] = pid;
    } else {
        printf("[WARNING] Limite tracciamento PID raggiunto! Impossibile salvare PID utente.\n");
    }
}

//refill pentole
void do_refill() {
    sem_reserve(sem_id, SEM_MUTEX_STATS);

    for (int i=0; i<MAX_DISH_TYPE; i++) {
        shm_ptr->q_primi[i] += shm_ptr->config.avg_refill_primi;
        if (shm_ptr->q_primi[i] > shm_ptr->config.max_porzioni_primi)
            shm_ptr->q_primi[i] = shm_ptr->config.max_porzioni_primi;

        shm_ptr->q_secondi[i] += shm_ptr->config.avg_refill_secondi;
        if (shm_ptr->q_secondi[i] > shm_ptr->config.max_porzioni_secondi)
            shm_ptr->q_secondi[i] = shm_ptr->config.max_porzioni_secondi;
    }

    sem_release(sem_id, SEM_MUTEX_STATS);
}

//avanzi
void calcola_avanzi_giornalieri() {
    int avanzi_primi = 0;
    for(int i=0; i<MAX_DISH_TYPE; i++) avanzi_primi += shm_ptr->q_primi[i];
    shm_ptr->stats.piatti_avanzi_daily[0] = avanzi_primi;

    int avanzi_secondi = 0;
    for(int i=0; i<MAX_DISH_TYPE; i++) avanzi_secondi += shm_ptr->q_secondi[i];
    shm_ptr->stats.piatti_avanzi_daily[1] = avanzi_secondi;

    shm_ptr->stats.piatti_avanzi_daily[2] = 0;
}

void print_daily_stats(int day) {
    sem_reserve(sem_id, SEM_MUTEX_STATS);

    calcola_avanzi_giornalieri();

    printf("\n==================================================\n");
    printf("     REPORT GIORNATA %d\n", day + 1);
    printf("==================================================\n");

    int utenti_generati_oggi = shm_ptr->config.nof_users;
    int serviti_oggi = shm_ptr->stats.n_utenti_serviti_daily;
    int rinunciatari_oggi = shm_ptr->stats.n_utenti_non_serviti_daily;

    int ancora_in_mensa = utenti_generati_oggi - (serviti_oggi + rinunciatari_oggi);
    if (ancora_in_mensa < 0) ancora_in_mensa = 0;

    printf("[UTENZA]\n");
    printf("  - Totale Arrivati:   %d\n", utenti_generati_oggi);
    printf("  - Serviti (Usciti):  %d\n", serviti_oggi);
    printf("  - Rinunci (Usciti):  %d\n", rinunciatari_oggi);
    printf("  > ANCORA IN MENSA:   %d\n", ancora_in_mensa);

    printf("\n[PIATTI DISTRIBUITI]\n");
    printf("  - Primi:             %d\n", shm_ptr->stats.piatti_distr_daily[0]);
    printf("  - Secondi/Contorni:  %d\n", shm_ptr->stats.piatti_distr_daily[1]);
    printf("  - Dolce/Caffe:       %d\n", shm_ptr->stats.piatti_distr_daily[2]);
    int tot_piatti = shm_ptr->stats.piatti_distr_daily[0] +
                     shm_ptr->stats.piatti_distr_daily[1] +
                     shm_ptr->stats.piatti_distr_daily[2];
    printf("  > Totale Piatti:     %d\n", tot_piatti);

    printf("[PIATTI AVANZATI]\n");
    printf("  - Primi rimasti:     %d\n", shm_ptr->stats.piatti_avanzi_daily[0]);
    printf("  - Secondi rimasti:   %d\n", shm_ptr->stats.piatti_avanzi_daily[1]);

    printf("[TEMPI ATTESA MEDI (Secondi)]\n");
    char *labels[] = {"Primi", "Secondi", "Caffe", "Tavoli"};
    double tot_time_day = 0;
    int tot_count_day = 0;
    for(int i=0; i<4; i++) {
        double avg = (shm_ptr->stats.count_attesa_daily[i] > 0) ?
                     (shm_ptr->stats.tempo_attesa_daily[i] / shm_ptr->stats.count_attesa_daily[i]) : 0.0;
        printf("  - %-8s: %.3f s\n", labels[i], avg);
        tot_time_day += shm_ptr->stats.tempo_attesa_daily[i];
        tot_count_day += shm_ptr->stats.count_attesa_daily[i];
    }
    double avg_day_global = (tot_count_day > 0) ? (tot_time_day / tot_count_day) : 0.0;
    printf("  > Media Complessiva Giornata: %.3f s\n", avg_day_global);

    printf("[OPERATORI & ECONOMIA]\n");
    printf("  - Operatori Attivi:  %d\n", shm_ptr->stats.operatori_attivi_daily);
    double avg_pause = (shm_ptr->stats.operatori_attivi_daily > 0) ?
                       ((double)shm_ptr->stats.n_pause_daily / shm_ptr->stats.operatori_attivi_daily) : 0.0;
    printf("  - Media Pause/Op:    %.2f\n", avg_pause);
    printf("  > RICAVO OGGI:       %.2f Euro\n", shm_ptr->stats.ricavo_daily);

    shm_ptr->stats.n_utenti_serviti_tot += shm_ptr->stats.n_utenti_serviti_daily;
    shm_ptr->stats.n_utenti_non_serviti_tot += shm_ptr->stats.n_utenti_non_serviti_daily;

    for(int i=0; i<3; i++) {
        shm_ptr->stats.piatti_distr_tot[i] += shm_ptr->stats.piatti_distr_daily[i];
        shm_ptr->stats.piatti_avanzi_tot[i] += shm_ptr->stats.piatti_avanzi_daily[i];
    }

    for(int i=0; i<4; i++) {
        shm_ptr->stats.tempo_attesa_tot[i] += shm_ptr->stats.tempo_attesa_daily[i];
        shm_ptr->stats.count_attesa_tot[i] += shm_ptr->stats.count_attesa_daily[i];
    }

    shm_ptr->stats.operatori_attivi_sim_tot += shm_ptr->stats.operatori_attivi_daily;
    shm_ptr->stats.n_pause_tot += shm_ptr->stats.n_pause_daily;
    shm_ptr->stats.ricavo_totale += shm_ptr->stats.ricavo_daily;

    //report finale della giornata
    shm_ptr->stats.n_utenti_serviti_daily = 0;
    shm_ptr->stats.n_utenti_non_serviti_daily = 0;
    memset(shm_ptr->stats.piatti_distr_daily, 0, sizeof(shm_ptr->stats.piatti_distr_daily));
    memset(shm_ptr->stats.piatti_avanzi_daily, 0, sizeof(shm_ptr->stats.piatti_avanzi_daily));
    memset(shm_ptr->stats.tempo_attesa_daily, 0, sizeof(shm_ptr->stats.tempo_attesa_daily));
    memset(shm_ptr->stats.count_attesa_daily, 0, sizeof(shm_ptr->stats.count_attesa_daily));
    shm_ptr->stats.n_pause_daily = 0;
    shm_ptr->stats.ricavo_daily = 0.0;
    shm_ptr->stats.operatori_attivi_daily = 0;

    sem_release(sem_id, SEM_MUTEX_STATS);
}

void print_final_stats() {
    printf("\n##################################################\n");
    printf("     REPORT FINALE SIMULAZIONE (%d Giorni)\n", shm_ptr->config.sim_duration);

    int days = shm_ptr->config.sim_duration;
    if (days == 0) days = 1;

    long totale_utenti_generati = (long)shm_ptr->config.nof_users * days;
    long totale_serviti = shm_ptr->stats.n_utenti_serviti_tot;
    long totale_rinunci = shm_ptr->stats.n_utenti_non_serviti_tot;
    long rimasti_dentro = totale_utenti_generati - (totale_serviti + totale_rinunci);

    printf("1. UTENTI TOTALI\n");
    printf("   - Generati Totali:    %ld\n", totale_utenti_generati);
    printf("   - Serviti Totali:     %ld\n", totale_serviti);
    printf("   - Non Serviti Totali: %ld\n", totale_rinunci);
    printf("   - Espulsi:  %ld\n", rimasti_dentro);

    printf("\n2. PIATTI DISTRIBUITI\n");
    char *p_names[] = {"Primi", "Secondi", "Caffe"};
    int grand_total_distr = 0;
    for(int i=0; i<3; i++) {
        printf("   - %-8s: %d Totali (Media: %.1f/giorno)\n",
               p_names[i], shm_ptr->stats.piatti_distr_tot[i], (double)shm_ptr->stats.piatti_distr_tot[i]/days);
        grand_total_distr += shm_ptr->stats.piatti_distr_tot[i];
    }
    printf("   > TOTALE ASSOLUTO: %d (Media: %.1f/giorno)\n", grand_total_distr, (double)grand_total_distr/days);

    printf("\n3. PIATTI AVANZATI (Totali accumulati)\n");
    for(int i=0; i<2; i++) {
        printf("   - %-8s: %d Totali (Media: %.1f/giorno)\n",
               p_names[i], shm_ptr->stats.piatti_avanzi_tot[i], (double)shm_ptr->stats.piatti_avanzi_tot[i]/days);
    }

    printf("\n4. TEMPI DI ATTESA MEDI\n");
    char *s_names[] = {"Primi", "Secondi", "Caffe", "Tavoli"};
    double sum_time_sim = 0;
    int sum_count_sim = 0;
    for(int i=0; i<4; i++) {
        double avg = (shm_ptr->stats.count_attesa_tot[i] > 0) ?
                     (shm_ptr->stats.tempo_attesa_tot[i] / shm_ptr->stats.count_attesa_tot[i]) : 0.0;
        printf("   - %-8s: %.3f s\n", s_names[i], avg);
        sum_time_sim += shm_ptr->stats.tempo_attesa_tot[i];
        sum_count_sim += shm_ptr->stats.count_attesa_tot[i];
    }
    double global_avg = (sum_count_sim > 0) ? (sum_time_sim / sum_count_sim) : 0.0;
    printf("   > MEDIA GLOBALE ATTESA: %.3f s\n", global_avg);

    printf("\n5. OPERATORI & PAUSE\n");
    printf("   - Operatori Attivi (Somma turni): %d\n", shm_ptr->stats.operatori_attivi_sim_tot);
    printf("   - Totale Pause: %d\n", shm_ptr->stats.n_pause_tot);

    printf("\n6. ECONOMIA\n");
    printf("   - Ricavo Totale: %.2f Euro\n", shm_ptr->stats.ricavo_totale);
    printf("   - Ricavo Medio:  %.2f Euro/giorno\n", shm_ptr->stats.ricavo_totale / days);

}

int conta_utenti_in_coda(int sem_id) {
    int w_primi = semctl(sem_id, SEM_SEATS_PRIMI, GETNCNT);
    int w_secondi = semctl(sem_id, SEM_SEATS_SECONDI, GETNCNT);
    int w_cassa = semctl(sem_id, SEM_SEATS_CASSA, GETNCNT);
    if (w_primi == -1) w_primi = 0;
    if (w_secondi == -1) w_secondi = 0;
    if (w_cassa == -1) w_cassa = 0;

    return w_primi + w_secondi + w_cassa;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    signal(SIGINT, cleanup_resources);
    signal(SIGTERM, cleanup_resources);
    srand(time(NULL));

    key_t k_shm = ftok(FTOK_PATH, SHM_PROJ);
    key_t k_sem = ftok(FTOK_PATH, SEM_PROJ);
    key_t k_msg = ftok(FTOK_PATH, MSG_PROJ);

    if (k_shm == -1 || k_sem == -1 || k_msg == -1) error_exit("ftok failed");

    shm_id = shmget(k_shm, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id == -1) error_exit("shmget failed");
    shm_ptr = (SharedData *)shmat(shm_id, NULL, 0);
    memset(shm_ptr, 0, sizeof(SharedData));

    const char *conf_file = (argc > 1) ? argv[1] : "config_timeout.conf";
    parse_config(conf_file, &shm_ptr->config);

    msg_id = msgget(k_msg, IPC_CREAT | 0666);
    if (msg_id == -1) error_exit("msgget failed");

    sem_id = create_sem_set(k_sem, SEM_NUM_TOTAL);

    init_sem(sem_id, SEM_MUTEX_STATS, 1);
    init_sem(sem_id, SEM_SEATS_PRIMI, shm_ptr->config.seats_primi);
    init_sem(sem_id, SEM_SEATS_SECONDI, shm_ptr->config.seats_secondi);
    init_sem(sem_id, SEM_SEATS_COFFEE, shm_ptr->config.seats_coffee);
    init_sem(sem_id, SEM_SEATS_CASSA, shm_ptr->config.seats_cassa);
    init_sem(sem_id, SEM_SEATS_TAVOLI, shm_ptr->config.seats_tavoli);
    init_sem(sem_id, SEM_START_SIM, 1);

    int num_workers = shm_ptr->config.nof_workers;
    int num_users = shm_ptr->config.nof_users;
    max_child_pids = num_workers + (num_users * shm_ptr->config.sim_duration) + 50;
    child_pids = malloc(sizeof(pid_t) * (num_workers + num_users * shm_ptr->config.sim_duration + 10));

    for (int i = 0; i < num_workers; i++) {
        spawn_worker(i % 4, shm_id, sem_id, msg_id);
    }

    printf("[RESPONSABILE] Avvio simulazione (Durata: %d giorni)...\n", shm_ptr->config.sim_duration);
    sleep(1);

    shm_ptr->simulation_active = 1;

    semctl(sem_id, SEM_START_SIM, SETVAL, 0);

    sleep(2);

    int minutes_in_day = 180;
    struct timespec minute_delay;

    minute_delay.tv_sec = shm_ptr->config.n_nano_secs / 1000000000L;
    minute_delay.tv_nsec = shm_ptr->config.n_nano_secs % 1000000000L;

    for (int day = 0; day < shm_ptr->config.sim_duration; day++) {
        shm_ptr->current_day = day + 1;
        printf("\n--- INIZIO GIORNO %d ---\n", shm_ptr->current_day);

        sem_reserve(sem_id, SEM_MUTEX_STATS);
        shm_ptr->stats.operatori_attivi_daily = num_workers;
        sem_release(sem_id, SEM_MUTEX_STATS);

        for (int i = 0; i < num_users; i++) {
            spawn_user(shm_id, sem_id, msg_id);
        }

        do_refill();

        for (int min = 0; min < minutes_in_day; min++) {
            nanosleep(&minute_delay, NULL);
            if (min > 0 && min % 10 == 0) {
                do_refill();
            }
        }

        int utenti_in_attesa = conta_utenti_in_coda(sem_id);

        printf("[RESPONSABILE] Fine Giorno %d. Utenti in coda: %d (Soglia: %d)\n",
               day+1, utenti_in_attesa, shm_ptr->config.overload_threshold);

        if (utenti_in_attesa > shm_ptr->config.overload_threshold) {
            printf(" [ALLARME] OVERLOAD RILEVATO %d!\n", day+1);
            printf(" Utenti bloccati in coda: %d > Soglia %d\n",
                   utenti_in_attesa, shm_ptr->config.overload_threshold);
            printf(" Interruzione simulazione.\n");

            shm_ptr->simulation_active = 0;
            goto FINE_SIMULAZIONE;
        }

        print_daily_stats(day);
    }

    FINE_SIMULAZIONE:

    shm_ptr->simulation_active = 0;
    print_final_stats();

    cleanup_resources(0);
    return 0;
}