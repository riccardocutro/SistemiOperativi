CC = gcc
CFLAGS = -Wvla -Wextra -Werror -D_GNU_SOURCE -std=gnu99

SRCS_COMMON = common.c
OBJS_COMMON = $(SRCS_COMMON:.c=.o)

EXEC_RESP = responsabile_mensa
EXEC_OP = operatore
EXEC_CASSA = operatore_cassa
EXEC_USER = utente

all: $(EXEC_RESP) $(EXEC_OP) $(EXEC_CASSA) $(EXEC_USER)

$(EXEC_RESP): responsabile_mensa.c $(OBJS_COMMON) common.h
	$(CC) $(CFLAGS) -o $@ responsabile_mensa.c $(OBJS_COMMON)

$(EXEC_OP): operatore.c $(OBJS_COMMON) common.h
	$(CC) $(CFLAGS) -o $@ operatore.c $(OBJS_COMMON)

$(EXEC_CASSA): operatore_cassa.c $(OBJS_COMMON) common.h
	$(CC) $(CFLAGS) -o $@ operatore_cassa.c $(OBJS_COMMON)

$(EXEC_USER): utente.c $(OBJS_COMMON) common.h
	$(CC) $(CFLAGS) -o $@ utente.c $(OBJS_COMMON)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(EXEC_RESP) $(EXEC_OP) $(EXEC_CASSA) $(EXEC_USER)

.PHONY: all clean