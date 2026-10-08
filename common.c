#include "common.h"

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

//linea di partenza
void sem_wait_zero(int sem_id, int sem_num) {
    struct sembuf sops;
    sops.sem_num = sem_num;
    sops.sem_op = 0;
    sops.sem_flg = 0;
    while (semop(sem_id, &sops, 1) == -1) {
        if (errno != EINTR) error_exit("sem_wait_zero failed");
    }
}

//conversione tempi lunghi
void wait_sim_time(int minutes, int n_nano_secs_per_min) {
    long total_ns = (long)minutes * n_nano_secs_per_min;
    struct timespec t;
    t.tv_sec = total_ns / 1000000000L;
    t.tv_nsec = total_ns % 1000000000L;
    nanosleep(&t, NULL);
}

//conversione tempi brevi
void wait_sim_seconds(int seconds, int n_nano_secs_per_min) {
    if (seconds < 0) seconds = 0;

    long ns_per_sec = n_nano_secs_per_min / 60;

    long total_ns = (long)seconds * ns_per_sec;

    struct timespec t;
    t.tv_sec = total_ns / 1000000000L;
    t.tv_nsec = total_ns % 1000000000L;

    nanosleep(&t, NULL);
}

long get_random_in_range(long center, int percent_var) {
    if (percent_var == 0) return center;
    long delta = (center * percent_var) / 100;
    long min = center - delta;
    if (min < 0) min = 0;
    long max = center + delta;
    return min + rand() % (max - min + 1);
}

// semafori

int create_sem_set(key_t key, int nsems) {
    int id = semget(key, nsems, IPC_CREAT | 0666);
    if (id == -1) error_exit("semget failed");
    return id;
}

int sem_get_val(int sem_id, int sem_num) {
    int val = semctl(sem_id, sem_num, GETVAL);
    if (val == -1) error_exit("semctl GETVAL failed");
    return val;
}

void init_sem(int semid, int semnum, int val) {
    union semun arg;
    arg.val = val;
    if (semctl(semid, semnum, SETVAL, arg) == -1) {
        error_exit("semctl SETVAL failed");
    }
}

void sem_reserve(int semid, int semnum) {
    struct sembuf sop;
    sop.sem_num = semnum;
    sop.sem_op = -1;
    sop.sem_flg = 0;

    while (semop(semid, &sop, 1) == -1) {
        if (errno != EINTR) error_exit("semop reserve failed");
    }
}

void sem_release(int semid, int semnum) {
    struct sembuf sop;
    sop.sem_num = semnum;
    sop.sem_op = 1;
    sop.sem_flg = 0;

    if (semop(semid, &sop, 1) == -1) {
        error_exit("semop release failed");
    }
}

// messaggi

void send_msg(int msg_id, MsgRequest *msg) {
    if (msgsnd(msg_id, msg, sizeof(MsgRequest) - sizeof(long), 0) == -1) {
        if (errno != EIDRM && errno != EINVAL) perror("msgsnd failed");
    }
}

ssize_t receive_msg(int msg_id, MsgRequest *msg, long type) {
    ssize_t ret = msgrcv(msg_id, msg, sizeof(MsgRequest) - sizeof(long), type, 0);
    if (ret == -1) {
        if (errno != EIDRM && errno != EINVAL && errno != EINTR) perror("msgrcv failed");
    }
    return ret;
}