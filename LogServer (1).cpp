/**
 * @file server.c
 * @brief UDP Logging Server
 *
 * This server listens for log messages from a client over UDP,
 * logs them to a file, and allows dynamic log level control.
 *
 * Features:
 * - Receives log messages from clients.
 * - Logs messages to a file.
 * - Allows log level changes via UDP commands.
 * - Provides a menu-driven interface for server management.
 *
 * 
 * @date 2025-03-23
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUF_LEN 1024          // Maximum buffer size for incoming messages
#define SERVER_PORT 54321     // Port number for the server to listen on
#define LOG_FILE "server_log.txt" // File where logs will be stored

// Global variables for server operation
static int sockfd = -1; // UDP socket file descriptor
static pthread_t recv_thread; // Thread for receiving log messages
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for synchronizing log file access
static int server_running = 1; // Flag to keep the server running

// Client information tracking
static struct sockaddr_in client_addr; // Stores the last sender of a log message
static struct sockaddr_in recv_client_addr; // Stores client's receive port for log level updates
static socklen_t client_addr_len = sizeof(client_addr);
static int client_known = 0; // Flag to indicate if a client has sent a log message
static int recv_client_known = 0; // Flag to indicate if a client has sent a hello message

/**
 * @brief Thread function to receive log messages from clients.
 *
 * This function runs in a separate thread, continuously listening for log messages
 * over UDP. It logs the messages to a file and stores client information for
 * potential log level updates.
 *
 * @param arg Unused parameter.
 * @return NULL when the thread exits.
 */
static void *receive_thread(void *arg) {
    char buf[BUF_LEN];
    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);

    // Open log file in append mode to store incoming log messages
    FILE *log_file = fopen(LOG_FILE, "a");
    if (!log_file) {
        perror("fopen");
        return NULL;
    }

    // Set appropriate permissions for the log file
    fchmod(fileno(log_file), 0666);

    while (server_running) {
        memset(buf, 0, BUF_LEN);
        int n = recvfrom(sockfd, buf, BUF_LEN - 1, 0, (struct sockaddr *)&src_addr, &addrlen);
        if (n > 0) {
            buf[n] = '\0'; // Ensure null-termination of received string

            pthread_mutex_lock(&mutex);
            if (!client_known) {
                // Store the first client that sends a log message
                memcpy(&client_addr, &src_addr, sizeof(src_addr));
                client_known = 1;
            }

            // If the client sends a "hello" message, store its receiving port
            if (strncmp(buf, "Client Hello", 12) == 0) {
                memcpy(&recv_client_addr, &src_addr, sizeof(src_addr));
                recv_client_known = 1;
            }

            // Log the received message to the file
            fprintf(log_file, "%s\n", buf);
            fflush(log_file);
            pthread_mutex_unlock(&mutex);
        } else {
            sleep(1); // Prevent high CPU usage when no data is received
        }
    }

    fclose(log_file);
    return NULL;
}

/**
 * @brief Dumps the contents of the log file to the console.
 *
 * This function reads the log file and prints its contents to the screen.
 * If the log file cannot be opened, an error message is displayed.
 */
static void dump_log_file() {
    FILE *log_file = fopen(LOG_FILE, "r");
    if (!log_file) {
        printf("Failed to open log file for reading\n");
        return;
    }

    char line[BUF_LEN];
    while (fgets(line, BUF_LEN, log_file)) {
        printf("%s", line);
    }

    fclose(log_file);
    printf("\nPress any key to continue: ");
    getchar();
}

/**
 * @brief Main function to start the UDP logging server.
 *
 * The function initializes the UDP socket, binds it to the server port,
 * starts the receiving thread, and provides a menu for log management.
 *
 * @return 0 on successful execution.
 */
int main() {
    // Create a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set socket to non-blocking mode
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    // Set up the server address struct
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind the socket to the specified port
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Start the receive thread to handle incoming log messages
    if (pthread_create(&recv_thread, NULL, receive_thread, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    int choice;
    char buf[BUF_LEN];

    // Server menu loop
    while (server_running) {
        printf("\nServer Menu:\n");
        printf("1. Set the log level\n");
        printf("2. Dump the log file here\n");
        printf("0. Shut down\n");
        printf("Enter choice: ");
        scanf("%d", &choice);
        getchar(); // Consume newline character after integer input

        if (choice == 1) {
            if (!recv_client_known) {
                printf("No client receive port known yet. Waiting for hello message.\n");
            } else {
                printf("Enter log level (0=DEBUG, 1=WARNING, 2=ERROR, 3=CRITICAL): ");
                int level;
                scanf("%d", &level);
                getchar();

                // Validate log level input
                if (level >= 0 && level <= 3) {
                    snprintf(buf, BUF_LEN, "Set Log Level=%d", level);
                    sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&recv_client_addr, sizeof(recv_client_addr));
                    printf("Sent log level %d to client\n", level);
                } else {
                    printf("Invalid level\n");
                }
            }
        } else if (choice == 2) {
            // Display the contents of the log file
            dump_log_file();
        } else if (choice == 0) {
            // Exit the server
            server_running = 0;
        } else {
            printf("Invalid choice\n");
        }
    }

    // Wait for the receiving thread to exit before shutting down
    pthread_join(recv_thread, NULL);
    close(sockfd);
    pthread_mutex_destroy(&mutex);

    printf("Server shut down\n");
    return 0;
}

