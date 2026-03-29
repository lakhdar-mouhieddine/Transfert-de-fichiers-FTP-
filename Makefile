CC = gcc
CFLAGS = -Wall -Wextra -O2

all: ftp_master ftp_slave ftp_client

ftp_master: ftp_master.c ftp_common.h
	$(CC) $(CFLAGS) -o ftp_master ftp_master.c

ftp_slave: ftp_slave.c ftp_common.h
	$(CC) $(CFLAGS) -o ftp_slave ftp_slave.c

ftp_client: ftp_client.c ftp_common.h
	$(CC) $(CFLAGS) -o ftp_client ftp_client.c

clean:
	rm -f ftp_master ftp_slave ftp_client dl_*

tar: clean
	@echo "Création de l'archive tar.gz..."
	tar -czvf BOUGUERRA_DJERBOUA.tar.gz ftp_master.c ftp_slave.c ftp_client.c ftp_common.h Makefile RAPPORT.md
