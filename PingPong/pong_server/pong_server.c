/*
 * pong_server.c: server in ascolto su una porta TCP "registrata".
 *                Il client invia una richiesta specificando il protocollo
 *                desiderato (TCP/UDP), il numero di messaggi e la loro lunghezza.
 *                Il server (mediante una fork()) crea un processo dedicato
 *                a svolgere il ruolo di "pong" secondo le richieste del client.
 *
 * versione 5.1 
 *
 * Programma sviluppato a supporto del laboratorio di
 * Sistemi di Elaborazione e Trasmissione del corso di laurea
 * in Informatica classe L-31 presso l'Universita` degli Studi di
 * Genova, anno accademico 2016/2017.
 *
 * Copyright (C) 2013-2014 by Giovanni Chiola <chiolag@acm.org>
 * Copyright (C) 2015-2016 by Giovanni Lagorio <giovanni.lagorio@unige.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <signal.h>
#include "pingpong.h"

void sigchld_handler(int signum)
{
	int status;
	if (waitpid(-1, &status, WNOHANG) == -1)
		fail("Pong Server cannot wait for children in SIGCHLD handler");
}
			//dimenioni , fd di lettura , fd creato
void tcp_pong(int message_no, size_t message_size, FILE *in_stream, int out_socket)
{
	char buffer[message_size], *cp;
	int n_msg, n_c;
	
	for (n_msg = 1; n_msg <= message_no; ++n_msg) 
	{
		int seq = 0;
		debug(" tcp_pong: n_msg=%d\n", n_msg);

		//cp punta a buffer, i cambiamenti su cp verrando sentiti anche da buffer e viceversa
		for (cp = buffer, n_c = 0; n_c < message_size; ++n_c, ++cp) 
		{
			//leggo su fd in lettura oss leggo il messaggio 
			int cc = getc(in_stream);
		
			if (cc == EOF)
				//se finisco qua vuol dire che ho ricevuto meno byte di quanto mi aspettavo 
				fail("TCP Pong received fewer bytes than expected");
		
			//inizio a comporre la stinga del messaggio
			*cp = (char)cc; 
		}
		//ho ricevuto tutto il messaggio... 
		//converte in contenuto di buffer in un intero
		//la sequenza ricevuta, in questo caso è esattamente quanti byte ho inviato
		
		if (sscanf(buffer, "%d\n", &seq) != 1)
			fail("TCP Pong got invalid message");
		debug(" tcp_pong: got %d sequence number (expecting %d)\n%s\n", seq, n_msg, buffer);
		
		//se ho inviato meno byte me ne accorgo qua
		if (seq != n_msg)
			fail("TCP Pong received wrong message sequence number");
		
		//invio il messaggio in buffer
		if (write_all(out_socket, buffer, message_size) != message_size)
			fail_errno("TCP Pong failed sending data back");
	}
}

void udp_pong(int dgrams_no, int dgram_sz, int pong_socket)
{
	/*
		struct sockaddr {
	unsigned short sa_family; // address family, AF_xxx
	char sa_data[14]; // 14 bytes of protocol address
	};

	*/
	char buffer[dgram_sz]; // dove salvare la risposta 
	ssize_t received_bytes; //salva i byte ricevuti 
	int n, //conta i pacchetti 
	resend; // la grandezza 
	struct sockaddr_storage ping_addr; 
	socklen_t ping_addr_len; // dimensione della struttura
	
struct timeval tv;
tv.tv_sec = 5;
tv.tv_usec = 0;
if (setsockopt(pong_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
fail_errno("failing to set a timeout");
	
	for (n = resend = 0; n < dgrams_no;)
	{
		int i;
		ping_addr_len = sizeof(struct sockaddr_storage); // dim struttura
		//uso la from perche' specifico chi mi manda i messaggi
		if ((received_bytes = recvfrom(pong_socket, buffer, sizeof buffer, 0, (struct sockaddr *)&ping_addr, &ping_addr_len)) < 0)
			fail_errno("UDP Pong recv failed");

		//controllo sulla correttezza dei bytes ricevuti
		if (received_bytes < dgram_sz)
			fail("UDP Pong received fewer bytes than expected");
		
		if (sscanf(buffer, "%d\n", &i) != 1)
					fail("UDP Pong received invalid message");
		#ifdef DEBUG
		{
			struct sockaddr_in *ipv4_addr = (struct sockaddr_in *)&ping_addr;
			char addrstr[INET_ADDRSTRLEN];
			/* 
			This function converts the network address structure src in the af address family into a character string. 
			NULL is returned if there was an error, with errno set to indicate the error.(e)
			const char *inet_ntop(int af, //indica se i o ipvp in questo caso 4 
							  const void *src, la struttura da convertire 
                              char *dst, dove mette il risusoltato 
							  socklen_t size); specifica la lunghezza
			*/	
			const char * const cp = inet_ntop(AF_INET, &(ipv4_addr->sin_addr), addrstr, INET_ADDRSTRLEN);
			int j;
			if (cp == NULL)
				printf(" could not convert address to string\n");
			else
				printf("  ... received %d bytes from %s, port %hu, sequence number %d (expecting %d)\n", (int)received_bytes, cp, ntohs(ipv4_addr->sin_port), i, n);

			for (j = 0; j < dgram_sz; ++j)
				printf("%c", buffer[j]);
			printf("\n-------------------------------------\n");
		}
		#endif
		
		if (i < n || i > dgrams_no || i < 1)
			fail("UDP Pong received wrong datagram sequence number");
				
		if (i > n) {	/* new datagram to pong back */
			n = i;
			resend = 0;
		} else {	/* resend previous datagram */
			if (++resend > MAXUDPRESEND)
				fail("UDP Pong maximum resend count exceeded");
		}
		//if (sendto(pong_socket, buffer, (size_t)received_bytes, 0, (struct sockaddr *)&ping_addr, ping_addr_len) < ping_addr_len)
		if (sendto(pong_socket, buffer, (size_t)received_bytes, 0, (struct sockaddr *)&ping_addr, ping_addr_len) < received_bytes)
			fail_errno("UDP Pong failed sending datagram back");
	}
}

/*** The following function creates a new UDP socket and binds it
 *   to a free Ephemeral port according to IANA definition.
 *   The port number is stored at the location pointed by "pong_port".
 */
int open_udp_socket(int *pong_port)
{
	/*
	
	struct addrinfo {
	int ai_flags;
	int ai_family; // AF_INET, AF_INET6, AF_UNSPEC
	int ai_socktype; // SOCK_STREAM, SOCK_DGRAM
	int ai_protocol; // 0 per "any"
	size_t ai_addrlen; // size of ai_addr in bytes
	struct sockaddr *ai_addr;
	char *ai_canonname; // canonical name
	struct addrinfo *ai_next; // this struct can form a linked list
	};

	struct sockaddr {
	unsigned short sa_family; // address family, AF_xxx
	char sa_data[14]; // 14 bytes of protocol address
	};

	*/
	struct addrinfo gai_hints, //struttura usata per i settaggi di base 
		   *pong_addrinfo; 		//struttura settata con la gedaddinfo
	int udp_socket, //socket 
		port_number, //numero della porta
		gai_rv, bind_rv;	//valori di ritorno
	//setta la struttura gai_hints a 0
	memset(&gai_hints, 0, sizeof gai_hints);
	
	gai_hints.ai_family = AF_INET;//inizializza il protocollo ipv4 
	gai_hints.ai_socktype = SOCK_DGRAM;//tipo di protocollo udp  
	gai_hints.ai_flags = AI_PASSIVE;//inizializza a "NULL" i flag di gai_hints 
	gai_hints.ai_protocol = IPPROTO_UDP;//settaggio protocollo udp  
	
	//ciclo sulle tutte le porte disponibili per trovarne una usabile 	
	for (port_number = IANAMINEPHEM; port_number <= IANAMAXEPHEM; ++port_number) 
	{
		//stringa di conversione della porta 
		char port_number_as_str[6];
		// conversione int to string 
		sprintf(port_number_as_str, "%d", port_number);	


/*** TO BE DONE START ***/ 
	
		//aggiunge alla gai l'ip e porta e genera la struttura pong_		
		if ((gai_rv=getaddrinfo(NULL,port_number_as_str,&gai_hints,&pong_addrinfo) )== -1)
			fail_errno("error getaddrinfo udp server");
		if((udp_socket = socket(pong_addrinfo->ai_family,pong_addrinfo->ai_socktype,pong_addrinfo->ai_protocol))==-1)
			fail_errno("ERROR: socket");
				
		if((bind_rv=bind(udp_socket,pong_addrinfo->ai_addr, pong_addrinfo->ai_addrlen))==0)
		{
			*pong_port=port_number;
			return udp_socket;
		}	
				
/*** TO BE DONE END ***/
		// EADDRINUSE -> indirizzo e porta gia usata 
		if (errno != EADDRINUSE) 
			fail_errno("UDP Pong could not bind the socket");
		//chiudi il socket
		if (close(udp_socket))
			fail_errno("UDP Pong could not close the socket");
	}
	fprintf(stderr, "UDP Pong could not find any free ephemeral port\n");
	return -1;
}

void serve_pong_udp(int request_socket, int pong_fd, int message_size, int message_no, int pong_port)
{
	//buffer della risposta
	char answer_buf[16];
	//setto la risposta
	sprintf(answer_buf, "OK %d\n", pong_port);

	if (write_all(request_socket, answer_buf, strlen(answer_buf)) != strlen(answer_buf))
		fail_errno("Pong Server UDP cannot send ok message to the client");
	
	//chiude la connsessione	
	if (shutdown(request_socket, SHUT_RDWR))
		fail_errno("Pong Server UDP cannot shutdown socket");
	
	//chiude il socket
	if (close(request_socket))
		fail_errno("Pong Server UDP cannot close request socket");
	//mi preparo alla risposta del client	
	udp_pong(message_no, message_size, pong_fd);
}

					//fd creato, fd di lettura, dimenioni varie
void serve_pong_tcp(int pong_fd, FILE *request_stream, size_t message_size, int message_no)
{
	const char *const ok_msg = "OK\n";
	const size_t len_ok_msg = strlen(ok_msg);
	int nodelay_value = 1;
	
	/*
	int setsockopt(	int socket, 
					int level,
					int option_name, -> specifies a single option to set
				    const void *option_value,  	-> valore da settare
					socklen_t option_len);	-> dimensione
	
	setta le opzioni passate ritorna 0 o -1
	*/
	
	if (setsockopt(pong_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay_value, sizeof nodelay_value))
		fail_errno("Pong Server TCP cannot set TCP_NODELAY option");
	
	//invio il messaggio in ok_msg
	if (write_all(pong_fd, ok_msg, len_ok_msg) != len_ok_msg)
		fail_errno("Pong Server TCP cannot send ok message to the client");
	
	tcp_pong(message_no, message_size, request_stream, pong_fd);
	
	//chiudo il socket creato
	if (shutdown(pong_fd, SHUT_RDWR))
		fail_errno("Pong Server TCP cannot shutdown socket");
}

void serve_client(int request_socket, struct sockaddr_in *client_addr)
{
	/*
		time_t         tv_sec      seconds
		suseconds_t    tv_usec     microseconds
	*/
	FILE *request_stream = fdopen(request_socket, "r"); // apro il socket in modalità lettura
	char *request_str = NULL, *strtokr_save; 
	char *protocol_str, *size_str, *number_str;
	size_t n;
	int message_size, message_no;
	struct timeval receiving_timeout;
	char client_addr_as_str[INET_ADDRSTRLEN];
	int is_tcp = 0, is_udp = 0; // tipologia di richiesta
	if (!request_stream) //stream non apreto
		fail_errno("Cannot obtain a stream from the socket");
	
	/*
	This function converts the network address structure src in the af address family into a character string. 
	NULL is returned if there was an error, with errno set to indicate the error.
		const char *inet_ntop(int af, //indica se ipv6 o ipv4 in questo caso 4 
							  const void *src, la struttura da convertire 
                              char *dst, dove mette il risusoltato 
							  socklen_t size); specifica la lunghezza
	*/	
	if (inet_ntop(AF_INET, &client_addr->sin_addr, client_addr_as_str, INET_ADDRSTRLEN) == NULL)
		fail_errno("Pong server could not convert client address to string");
	
	debug("Got connection from %s\n", client_addr_as_str);
	
	receiving_timeout.tv_sec = PONGRECVTOUT;//setto la struttura del tempo (drovebbe essere il ttl)
	receiving_timeout.tv_usec = 0; 
	
	/* 
	manipulate options for the socket referred to by the file descriptor sockfd.
	On success, zero is returned for the standard options.  On error, -1 is returned, and errno is set appropriately.

	int setsockopt( int sockfd, //stream dedicato 
					int level, 	// the level at which the option resides and the name of the option must be specified. 
					int optname,
                    const void *optval, is used to access option values
					socklen_t optlen); is used to access option values
	*/
	//setto il ttl al socket da spedire
	if (setsockopt(request_socket, SOL_SOCKET/*To  manipulate  options at  the  sockets API level*/, SO_RCVTIMEO, &receiving_timeout, sizeof receiving_timeout))
		fail_errno("Cannot set socket timeout");
	
	if (getline(&request_str, &n, request_stream) < 0) {
	send_request_error:
		{
			const char *const error_msg = "ERROR\n";
			const size_t len_error_msg = strlen(error_msg);
			if (write_all(request_socket, error_msg, len_error_msg) != len_error_msg)
				fail_errno("Pong server cannot send error message to the client");
			if (fclose(request_stream))
				fail_errno("Pong server cannot close request stream");
			exit(EXIT_FAILURE);
		}
	}
	
	
	/*
	The  strtok() function breaks a string into a sequence of zero or more nonempty tokens.
	return a pointer to the next token, or NULL if there are no more tokens.
	
	char *strtok_r( char *str,  specifica la stringa da analizzare (solo alla prima chiamta)
					const char *delim, insieme di bytes che delimitano i token in str 
					char **saveptr); //credo ritorni una lista di token
	*/
	
	protocol_str = strtok_r(request_str, " ", &strtokr_save);// trovo il primo spazio se esiste 
	if (!protocol_str) {
		free_str_and_send_request_error:
		free(request_str);
		goto send_request_error;
	}
	//controllo la tipologia di richiesta
	if (strcmp(protocol_str, "TCP") == 0) 
		is_tcp = 1;
	else if (strcmp(protocol_str, "UDP") == 0)
		is_udp = 1;
	else
		goto free_str_and_send_request_error;

	size_str = strtok_r(NULL, " ", &strtokr_save);//trolo il secondo spazio
	if (!size_str)
		goto free_str_and_send_request_error;
	
	
	if (sscanf(size_str/*stringa da analizzare*/,"%d"/*path cercato*/, &message_size/*dove salvare*/) != 1)
		goto free_str_and_send_request_error;
	if (message_size < MINSIZE || message_size > MAXTCPSIZE || (is_udp && message_size > MAXUDPSIZE))
		goto free_str_and_send_request_error;
	
	number_str = strtok_r(NULL, " ", &strtokr_save);//cerco un altro spazio nella stringa di prima
	if (!number_str)
		goto free_str_and_send_request_error;
	if (sscanf(number_str, "%d", &message_no) != 1)
		goto free_str_and_send_request_error;
	if (message_no < 1 || message_no > MAXREPEATS)
		goto free_str_and_send_request_error;
	free(request_str);
	if (is_udp) //se sono udp 
	{//preparo la risposta di tipo udp 
		int pong_port;
		int pong_fd = open_udp_socket(&pong_port);
		if (pong_fd < 0)
			goto send_request_error;
		serve_pong_udp(request_socket, pong_fd, message_size, message_no, pong_port);
	} else // se sono tcp
	{//preparo la risposta di tipo tcp
		assert(is_tcp);
		serve_pong_tcp(request_socket/*fd creato*/, request_stream/*fd in_lettura*/, (size_t)message_size, message_no);
	}
	fclose(request_stream);
	exit(EXIT_SUCCESS);
}

void server_loop(int server_socket)// accetta o meno una connessione e richiama serve_client(request_socket, &client_addr);
{
	
	/*
	 IPv4 AF_INET sockets:
		struct sockaddr_in {
		short sin_family; // e.g. AF_INET
		unsigned short sin_port; // e.g. htons(3490)
		struct in_addr sin_addr; // see struct in_addr,below
		char sin_zero[8]; // zero this if you want to
		};	
	*/
	
	for (;;) {
		struct sockaddr_in client_addr;//struttura per i dati client
		socklen_t addr_size = sizeof(client_addr);
		
		/*
		accept() extracts the first connection request on the queue of pending connections for the listening socket
		and returns a new file descrip‐tor  referring  to that socket
		int accept(int sockfd, 
				   struct sockaddr *addr, 
				   socklen_t *addrlen);

		*/
		int request_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size); //mi ritorna il primo fd libero
		pid_t pid;
		if (request_socket == -1) // se non accetta la connsessione....
		{
			// EINTR  -> The system call was interrupted by a signal that was caught before a valid connection arrived
			if (errno == EINTR)
				continue;
			close(server_socket);//...chiude il socket
			fail_errno("Pong server could not accept client connection");
		}
		//....altrimenti 
		if ((pid = fork()) < 0)//la fork fallisce 
			fail_errno("Pong Server could not fork");
		if (pid == 0) //sono il figlio
			serve_client(request_socket, &client_addr); // richiamo la funzione
		if (close(request_socket)) // dealloco il fd (lo rendo disponibile a terzi forse per un altra listen()
			fail_errno("Pong Server cannot close request socket");
	}
}

int main(int argc, char **argv)
{
	/*
	
	struct addrinfo {
	int ai_flags;
	int ai_family; // AF_INET, AF_INET6, AF_UNSPEC
	int ai_socktype; // SOCK_STREAM, SOCK_DGRAM
	int ai_protocol; // 0 per "any"
	size_t ai_addrlen; // size of ai_addr in bytes
	struct sockaddr *ai_addr;
	char *ai_canonname; // canonical name
	struct addrinfo *ai_next; // this struct can form a linked list
	};

	struct sockaddr {
	unsigned short sa_family; // address family, AF_xxx
	char sa_data[14]; // 14 bytes of protocol address
	};

	 struct sigaction {
               void     (*sa_handler)(int);                
               void     (*sa_sigaction)(int, siginfo_t *, void *);
               sigset_t   sa_mask;
               int        sa_flags;
               void     (*sa_restorer)(void);
           };

	*/

	struct  addrinfo gai_hints, //info del server (per  startare connessione)
	 		*server_addrinfo;	//info del serve in base alla richiesta del client (per il canale "privato")
	int server_socket, 	//file descriiptor che indica "indirizzo" del socket/servizio/porta utilizzato in memoria
		gai_rv; 	
	struct sigaction sigchld_action; // segnale da mandare al figlio 
	
	if (argc != 2)// se finisco qua manca la porta
		fail("Pong Server incorrect syntax. Use: pong_server PORT-NUMBER");
	
	//function fills the first n bytes of the memory area pointed to by s with the constant byte c. 		
	memset(&gai_hints, 0, sizeof gai_hints);//inizializzo la struttura a 0 ( 1-dove salvare 2-valore di settaggio 3-spazio riservato )
	gai_hints.ai_family = AF_INET; // AF_INET=ipv4 (This field specifies the desired address family for the returned addresses. F_INET / F_INET6 )
	gai_hints.ai_socktype = SOCK_STREAM; //tipo di socket da usare (tcp) (This field specifies the preferred socket type)
	gai_hints.ai_flags = AI_PASSIVE; // settaggio di falgs a 0 (This field specifies additional options)
	gai_hints.ai_protocol = IPPROTO_TCP; //settagio protocollo tcp (This field specifies the protocol for the returned socket addresses)

/*** TO BE DONE START ***/
	
	/*int getaddrinfo(const char* hostname (NULL 0.0.0.0 127.0.0.1 ),
                const char* service, ( tipo servizio/porta )
                const struct addrinfo* hints, ( dipende dal tipo di servizio potrebbe anche essere NULL )
                struct addrinfo** res ( dove va a finire in nuovo addrinfo con i settaggi delle prote e dei servizi richiesti ) );*/
	
	if ( (gai_rv = getaddrinfo(NULL, argv[1], &gai_hints, &server_addrinfo)) != 0) 
		fail_errno("ERROR: getaddrinfo server");
				
    if ( (server_socket = socket(server_addrinfo->ai_family, server_addrinfo->ai_socktype, server_addrinfo->ai_protocol)) == -1)
    	fail_errno("ERROR: socket server");

    if ( bind(server_socket, server_addrinfo->ai_addr, server_addrinfo->ai_addrlen) != 0) // associo porta a socket
		fail_errno("ERROR: could not bind server");
	
	if( listen(server_socket, LISTENBACKLOG)!=0)
		fail_errno("ERROR: could not listen server");
	
/*** TO BE DONE END ***/
	//a questo punto la server_addrinfo non serve piu' perche' abbiamo gia fatto la socket e la bind
	freeaddrinfo(server_addrinfo); // libera la variabile 
	fprintf(stderr, "Pong server listening on port %s ...\n", argv[1]); //stampa a terminale con lo stdError
	sigchld_action.sa_handler = sigchld_handler; // specifies the action to be associated with signum
	if (sigemptyset(&sigchld_action.sa_mask))
		fail_errno("Pong Server cannot initialize signal mask"); //funzine il file fail.c
	sigchld_action.sa_flags = SA_NOCLDSTOP;
	if (sigaction(SIGCHLD, &sigchld_action, NULL))
		fail_errno("Pong server cannot register SIGCHLD handler");
	server_loop(server_socket); // fa l'accept e il resto
}
