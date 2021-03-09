//Qiwei He 47771452 and Liwei Lu 90101531

#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>

#define NTHREADS  4
#define SBUFSIZE  16
#define MAXLINE  8192  /* Max text line length */
#define MAXBUF   8192  /* Max I/O buffer size */
#define LISTENQ  1024  /* Second argument to listen() */



typedef struct {
    int *buf;          /* Buffer array */
    int n;             /* Maximum number of slots */
    int front;         /* buf[(front+1)%n] is first item */
    int rear;          /* buf[rear%n] is last item */
    sem_t mutex;       /* Protects accesses to buf */
    sem_t slots;       /* Counts available slots */
    sem_t items;       /* Counts available items */
} sbuf_t;
/* $end sbuft */

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

/* Create an empty, bounded, shared FIFO buffer with n slots */
/* $begin sbuf_init */
void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = calloc(n, sizeof(int));
    sp->n = n;                       /* Buffer holds max of n items */
    sp->front = sp->rear = 0;        /* Empty buffer iff front == rear */
    sem_init(&sp->mutex, 0, 1);      /* Binary semaphore for locking */
    sem_init(&sp->slots, 0, n);      /* Initially, buf has n empty slots */
    sem_init(&sp->items, 0, 0);      /* Initially, buf has zero data items */
}
/* $end sbuf_init */

/* Clean up buffer sp */
/* $begin sbuf_deinit */
void sbuf_deinit(sbuf_t *sp)
{
    free(sp->buf);
}
/* $end sbuf_deinit */

/* Insert item onto the rear of shared buffer sp */
/* $begin sbuf_insert */
void sbuf_insert(sbuf_t *sp, int item)
{
    sem_wait(&sp->slots);                          /* Wait for available slot */
    sem_wait(&sp->mutex);                          /* Lock the buffer */
    sp->buf[(++sp->rear)%(sp->n)] = item;   /* Insert the item */
    sem_post(&sp->mutex);                          /* Unlock the buffer */
    sem_post(&sp->items);                          /* Announce available item */
}
/* $end sbuf_insert */

/* Remove and return the first item from buffer sp */
/* $begin sbuf_remove */
int sbuf_remove(sbuf_t *sp)
{
    int item;
    sem_wait(&sp->items);                          /* Wait for available item */
    sem_wait(&sp->mutex);                          /* Lock the buffer */
    item = sp->buf[(++sp->front)%(sp->n)];  /* Remove the item */
    sem_post(&sp->mutex);                          /* Unlock the buffer */
    sem_post(&sp->slots);                          /* Announce available slot */
    return item;
}

struct OFT_Entry{
    char filename[MAXLINE];
    FILE* fd;
    sem_t mutex[2]; // mutex[0] is for reading, mutex[1] is for writing
    int ref_num; // number of reference
};

int open_listenfd(char *port)
{
    struct addrinfo hints, *listp, *p;
    int listenfd, rc, optval = 1;

    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;             /* Accept connections */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* ... on any IP address */
    hints.ai_flags |= AI_NUMERICSERV;            /* ... using port number */
    if ((rc = getaddrinfo(NULL, port, &hints, &listp)) != 0)
    {
        fprintf(stderr, "getaddrinfo failed (port %s): %s\n", port, gai_strerror(rc));
        return -2;
    }

    /* Walk the list for one that we can bind to */
    for (p = listp; p; p = p->ai_next)
    {
        /* Create a socket descriptor */
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; /* Socket failed, try the next */

        /* Eliminates "Address already in use" error from bind */
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, //line:netp:csapp:setsockopt
                   (const void *)&optval, sizeof(int));

        /* Bind the descriptor to the address */
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break; /* Success */
        if (close(listenfd) < 0)
        { /* Bind failed, try the next */
            fprintf(stderr, "open_listenfd close failed: %s\n", strerror(errno));
            return -1;
        }
    }

    /* Clean up */
    freeaddrinfo(listp);
    if (!p) /* No address worked */
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
    {
        close(listenfd);
        return -1;
    }
    return listenfd;
}

sbuf_t sbuf; /* Shared buffer of connected descriptors */

void process(int connfd);
void *thread(void *vargp);

struct OFT_Entry file_table[4];


int main(int argc, const char * argv[]) {
    // insert code here...
    printf("Hello, World!\n");
    
    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    listenfd = open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE); //line:conc:pre:initsbuf
    for (i = 0; i < NTHREADS; i++)  /* Create worker threads */ //line:conc:pre:begincreate
	pthread_create(&tid, NULL, thread, NULL);               //line:conc:pre:endcreate

   while (1) {
        clientlen = sizeof(struct sockaddr_storage);
	    connfd = accept(listenfd, (struct sockaddr*) &clientaddr, &clientlen);
	    sbuf_insert(&sbuf, connfd); /* Insert connfd in buffer */
    }
}

void *thread(void *vargp) 
{  
    pthread_detach(pthread_self()); 
    while (1) { 
        int connfd = sbuf_remove(&sbuf); /* Remove connfd from buffer */ //line:conc:pre:removeconnfd
        process(connfd);                /* Service client */
        printf("Connected\n");
        close(connfd);
    }
};

void process(int connfd){
    size_t n;
    int i = 0;
    int size;
    char input[MAXLINE];
    float result;
    char* filename; 
    char* date;
    char* buffer;
    char* spliter = " \n";
    char output[MAXLINE];
    char buf2[MAXLINE];

    while((n = read(connfd, input, MAXLINE)) != 0){
        printf("%s\n", &input[1]);
        
        buffer = strtok(input, spliter);
        if(strcmp(buffer, "openRead")==0){
            filename = strtok(NULL, spliter);
            file_table[0].fd = fopen(filename, "r");
            continue;
        }
        if(strcmp(buffer, "openAppend")==0){
            filename = strtok(NULL, spliter);
            file_table[0].fd = fopen(filename, "a+");
            continue;
        }
        if(strcmp(buffer, "read")==0){
            buffer = strtok(NULL, spliter);
            int readlen = atoi(buffer);
            fgets(buf2, readlen, file_table[0].fd);
            sprintf(output, "%s\n", buf2);
        }
        if(strcmp(buffer, "append")==0){
            buffer = strtok(NULL, spliter);
            fputs(buffer, file_table[0].fd);
            continue;
        }
        if(strcmp(buffer, "close")==0){
            for(i=0; i< NTHREADS;i++){
                
                
            }
            continue;
        }

        write(connfd, output, strlen(output));
        for(i =0 ; i< MAXLINE;i++){
            input[i] = '\0';
            output[i] = '\0';
        }
    }
    
};