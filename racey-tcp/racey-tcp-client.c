/*
 * racey-tcp-client.c
 * Copyright (C) 2015 Yuzhong Wen <wyz2014@vt.edu>
 *
 * Distributed under terms of the MIT license.
 */

#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define WORKER_NUM 110
#define OUTPUT_FILE "./output.txt"

const char quote[64][256] = {
	"I love you, Pumpkin.\n",
	"I love you, Honey Bunny.\n",
	"All right, everybody be cool, this is a robbery!\n",
	"Any of you fucking pricks move, and I'll execute every motherfucking last one of ya!\n",
	"Whose motorcycle is this?\n",
	"It's a chopper, baby.\n",
	"Whose chopper is this?\n",
	"It's Zed's\n",
	"Who's Zed?\n",
	"Zed's dead, baby. Zed's dead.\n",
	"You now what they call a Quarter Pounder with Cheese in France?\n",
	"No.\n",
	"Tell 'em, Vincent.\n",
	"A Royale with Cheese.\n",
	"A Royale with cheese! You now why they call it that?\n",
	"....Because...of the metric system?\n",
	"Check out the big brain on Brett! You're a smart motherfucker. That's right, the metric system!\n",
	0,
};

unsigned short server_port ;
char *server_ip;

void* racey_worker(void* data)
{
	int fd;
	struct sockaddr_in server_addr; /* Echo server address */
	int n = 0;
	printf("Thread into position\n");

	printf("Ready to connect to %s:%d\n", server_ip, server_port);

	/* Send Plup Fiction to the someone who loves it */
	while (quote[n][0] != 0) {
		if ((fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
			return;
		}

		memset(&server_addr, 0, sizeof(server_addr));     /* Zero out structure */
		server_addr.sin_family      = AF_INET;             /* Internet address family */
		server_addr.sin_addr.s_addr = inet_addr(server_ip);   /* Server IP address */
		server_addr.sin_port        = htons(server_port); /* Server port */

		if (connect(fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
			printf("Couldn't connect to %s\n", server_ip);
			return;
		}

		write(fd, quote[n], strlen(quote[n]) + 1);
		n++;
		close(fd);
	}

	printf("Done.\n");
}

int main(int argc, char **argv)
{
	int i;
	pthread_t threads[WORKER_NUM];
	pthread_attr_t attr;

	if ((argc < 3) || (argc > 4)) {    /* Test for correct number of arguments */
		fprintf(stderr, "Usage: %s <Server IP> <Port>\n",
			argv[0]);
		exit(1);
	}

	server_ip   = argv[1];         /* First arg: server IP address (dotted quad) */
	server_port = atoi(argv[2]);   /* Second arg: string to echo */

	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

	for (i = 0; i < WORKER_NUM; i++) {
		if (pthread_create(&threads[i], &attr, racey_worker, NULL) < 0) {
			printf("HOLY SHIT\n");
			return 1;
		}
	}

	for(i=0; i < WORKER_NUM; i++) {
		pthread_join(threads[i], NULL);
	}

	return 0;
}
