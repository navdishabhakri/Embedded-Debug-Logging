#include "Logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// Define configurable buffer length, server and client port, and IP addresses
#define BUF_LEN 1024                  // Buffer size for message handling
#define SERVER_IP "127.0.0.1"         // Server IP address for communication
#define SERVER_PORT 54321             // Server port for receiving messages
#define CLIENT_PORT 54322             // Client port for receiving commands from server

// Static variables for sockets and thread handling
static int send_socket = -1;       // Socket for sending logs to the server
static int recv_socket = -1;       // Socket for receiving commands from the server
static struct sockaddr_in server_addr;      // Server address for sending logs
static LOG_LEVEL log_filter = DEBUG;         // Log level filter (default: DEBUG)
static pthread_t recv_thread;       // Thread to handle receiving commands
static int server_running = 1;      // Flag to keep the server running
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for thread safety

/**
 * Thread function to handle receiving commands from the server.
 * Changes the log level based on the received message.
 */
static void *receive_thread(void *arg) {
    char buf[BUF_LEN];           // Buffer for storing received messages
    struct sockaddr_in src_addr; // Source address of received messages
    socklen_t addrlen = sizeof(src_addr);  // Length of the source address

    // Main loop to receive messages from the server
    while (server_running) {
        memset(buf, 0, BUF_LEN);  // Clear the buffer
        int n = recvfrom(recv_socket, buf, BUF_LEN - 1, 0, (struct sockaddr *)&src_addr, &addrlen);
        if (n > 0) {
            buf[n] = '\0';  // Null-terminate the received string
            // If the message is a "Set Log Level" command, update the log level
            if (strncmp(buf, "Set Log Level=", 14) == 0) {
                int new_level = atoi(buf + 14);  // Extract new log level from the message
                pthread_mutex_lock(&log_mutex);  // Lock the mutex for thread safety
                log_filter = (LOG_LEVEL)new_level;  // Update the global log level
                pthread_mutex_unlock(&log_mutex);  // Unlock the mutex
            }
        } else {
            sleep(1);  // Sleep for 1 second if no message is received
        }
    }
    return NULL;
}

/**
 * Initializes logging system by creating necessary sockets
 * and setting up the server communication.
 * 
 * @return 0 on success, -1 on failure
 */
int InitializeLog() {
    // Create a socket for sending logs to the server
    send_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_socket < 0) {
        perror("Socket creation failed (send)");
        return -1;
    }
    int flags = fcntl(send_socket, F_GETFL, 0);
    fcntl(send_socket, F_SETFL, flags | O_NONBLOCK);  // Set socket to non-blocking

    // Create a socket for receiving commands from the server
    recv_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (recv_socket < 0) {
        perror("Socket creation failed (recv)");
        close(send_socket);
        return -1;
    }
    flags = fcntl(recv_socket, F_GETFL, 0);
    fcntl(recv_socket, F_SETFL, flags | O_NONBLOCK);  // Set socket to non-blocking

    // Set up client address and bind the receiving socket to the CLIENT_PORT
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(CLIENT_PORT);
    if (bind(recv_socket, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(send_socket);
        close(recv_socket);
        return -1;
    }

    // Configure server address for communication
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_aton(SERVER_IP, &server_addr.sin_addr);

    // Send initial hello message from the client to the server
    const char *hello_msg = "Client Hello from recv_socket";
    sendto(recv_socket, hello_msg, strlen(hello_msg), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // Start the receive thread
    server_running = 1;
    if (pthread_create(&recv_thread, NULL, receive_thread, NULL) != 0) {
        perror("Receive thread creation failed");
        close(send_socket);
        close(recv_socket);
        return -1;
    }
    return 0;
}

/**
 * Sets the log level for filtering logs based on severity.
 * 
 * @param level The desired log level (DEBUG, WARNING, ERROR, CRITICAL)
 */
void SetLogLevel(LOG_LEVEL level) {
    pthread_mutex_lock(&log_mutex);    // Lock the mutex for thread safety
    log_filter = level;                // Update the log level filter
    pthread_mutex_unlock(&log_mutex);  // Unlock the mutex
}

/**
 * Logs a message to the server based on the specified log level.
 * 
 * @param level Log level for the message (DEBUG, WARNING, ERROR, CRITICAL)
 * @param file Name of the source file from which the log is generated
 * @param func Name of the function from which the log is generated
 * @param line Line number in the source file where the log is generated
 * @param message The log message to send
 */
void Log(LOG_LEVEL level, const char *file, const char *func, int line, const char *message) {
    pthread_mutex_lock(&log_mutex);  // Lock the mutex for thread safety
    if (level < log_filter) {        // If the log level is below the filter level, return without logging
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    // Get the current time and format it
    time_t now = time(0);
    char *time_str = ctime(&now);
    time_str[strcspn(time_str, "\n")] = '\0';  // Remove newline character from the time string

    // Log level names
    char level_str[][16] = {"DEBUG", "WARNING", "ERROR", "CRITICAL"};
    char buf[BUF_LEN];  // Buffer for constructing the log message
    int len = snprintf(buf, BUF_LEN, "%s %s %s:%s:%d %s", time_str, level_str[level], file, func, line, message);
    if (len < 0) {
        pthread_mutex_unlock(&log_mutex);  // Unlock the mutex if snprintf fails
        return;
    }

    // Send the log message to the server
    sendto(send_socket, buf, len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    pthread_mutex_unlock(&log_mutex);  // Unlock the mutex
}

/**
 * Exits the logging system, stops the receive thread, and closes the sockets.
 */
void ExitLog() {
    server_running = 0;  // Stop the server loop
    pthread_join(recv_thread, NULL);  // Wait for the receive thread to finish
    close(send_socket);  // Close the sending socket
    close(recv_socket);  // Close the receiving socket
    pthread_mutex_destroy(&log_mutex);  // Destroy the mutex
}

