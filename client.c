#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_MSG_LEN 200
#define BUFFER_SIZE 201

void get_user_input(char *serverIP, int *port, char *Msg){
    //ask user for server ip
    printf("Enter server IP address:  ");
    scanf("%49s", serverIP); //%49s prevents buffer overflow

    //ask user for the port number
    printf("Enter port number: ");
    scanf("%d",&port); // read and store port
    //clear leftover newline
    getchar();

    //ask user for message 
    printf("Enter message(max 200 chars): ");
    fgets(Msg, MAX_MSG_LEN+1, stdin);// read full line including spaces and store it
    
    //remove trailing newline 
    Msg[strcspn(Msg, "\n")] = '\0'; 
}

int create_and_connect_socket(const char *serverIP, int port){
    int sockfd;

    //server address holder
    struct sockaddr_in server_addr;

    //create socket 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror ("socket failed");
        return -1;
    }

    //clear struct 
    memset(&server_addr, 0, sizeof(server_addr));

    //fill in server address info
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    //convert IP address to binary
    if(inet_pton(AF_INET, serverIP, &server_addr.sin_addr)<= 0){
        perror("invalid IP address");
        close(sockfd);
        return -1;
    }

    //connect to server
    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("connect failed");
        close(sockfd);
        return -1;
    }

    //successful connection
    return sockfd;

}

//initialize the openssl library
void init_openssl(){
    SSL_library_init(); //initialize the SSL library
    OpenSSl_add_all_algorithms(); //load cryptographic algorithms
    SSL_load_error_strings(); //load readable error messages
}

//create SSL context
SSL_CTX *create_ssl_context(){
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    //client compatible TLS method
    method = TLS_client_method();

    //create SSL context using method
    ctx = SSL_CTX_new(method);
    if (ctx == NULL){
        fprintf(stderr, "Unable to create SSL context\n");
        ERR_print_errors_fp(stderr); //print OpenSSL specific errors
        return NULL;
    }
    return ctx;
}

//create SSL connection
SSL *create_SSL_connection(SSL_CTX *ctx, int sockfd){
    SSL *ssl;

    //create a new SSL object from SSL context
    ssl = SSL_new(ctx);
    if (ssl == NULL){
        fprintf(stderr, "Failed to create SSL object\n");
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    //attach SSL object to the existing TCP socket
    if (SSL_set_fd(ssl, sockfd) == 0){
        fprintf(stderr, "Failed to attach socket to SSL\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        return NULL;
    }

    //SSL/TLS handshake
    if (SSL_connect(ssl) <= 0){
        fprintf(stderr, "SSL handshake failed\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        return NULL;
    }

    //SSL connection is ready to use
    return ssl;
}

//send secure message 
int send_secure_message(SSL *ssl, const char *message){
    int bytes_sent;

    //send encrypted data through the SSL connection
    bytes_sent = SSL_write(ssl, message, strlen(message));
    if (bytes_sent <= 0) {
        fprintf(stderr, "SSL_write failed\n");
        ERR_print_errors_fp(stderr);
        return -1
    }
    return bytes_sent;

}

//recieve secure message 
int recieve_secure_message(SSL *ssl){
    char buffer[BUFFER_SIZE];
    int bytes_recieved;

    //initialize buffer to all zeros
    memset(buffer, 0, sizeof(buffer));

    //read encrypted message from SSL connection
    bytes_recieved = SSL_read(ssl, buffer, sizeof(buffer)-1);
    if (bytes_recieved <= 0){
        fprintf(stderr, "SSL_read failes\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    buffer[bytes_recieved] = '\0';

    printf("Server response: %s\n", buffer);

    return bytes_recieved;
}

//clean up
void cleanup(SSL *ssl, SSL_CTX *ctx, int sockfd){
    //shut down and free SSL connection
    if (ssl != NULL){
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }

    //free SSL context 
    if (ctx != NULL){
        SSL_CTX_free(ctx);
    }

    //close the socket
    if (sockfd >= 0){
        close(sockfd);
    }
}

int main(){
    char serverIP[50];
    int port;
    char message[MAX_MSG_LEN + 1];

    int sockfd = -1; //initialize to invalid value in case something fails
    SSL_CTX *ctx = NULL; //start as NULL so cleanup is safe
    SSL *ssl = NULL; //start as NULL so clean up is safe

    //get input from user
    get_user_input(serverIP, &port, message);

    //create TCP socket and connect server
    sockfd = create_and_connect_socket(serverIP, port);
    if (sockfd < 0){
        fprintf(stderr, "TCP connection Failed\n");
        cleanup(ssl, ctx, sockfd);
        return 1;
    }

    //initialize OpenSSL library
    init_openssl();

    //create SSL context for the client 
    ctx = create_ssl_context();
    if (ctx == NULL){
        fprintf(stderr, "SSL context creation failed\n");
        cleanup(ssl, ctx, sockfd);
        return 1;
    }

    //create SSL object, attach to socket
    ssl = create_SSL_connection(ctx, sockfd);
    if (ssl == NULL){
        fprintf(stderr, "SSL connection failed\n");
        cleanup(ssl, ctx, sockfd);
        return 1;
    }

    //send user message to server
    if (send_secure_message(ssl, message) < 0){
        fprintf(stderr, "Failed to send secure message\n");
        cleanup(ssl, ctx, sockfd);
        return 1;
    }

    //recieve user message to server
    if (recieve_secure_message(ssl) < 0){
        fprintf(stderr, "Failed to recieve secure message\n");
        cleanup(ssl, ctx, sockfd);
        return 1;
    }

    //clean up
    cleanup(ssl, ctx, sockfd);
    return 0;
}






