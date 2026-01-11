# Embedded-Debug-Logging

 Embedded Debug Logging (C++ )

Description:
This project implements a robust logging system for processes running on multiple machines, allowing logs to be filtered, collected, and monitored in real-time. Python scripts are used for automation and integration with a front-end dashboard.

Key Features:

C++ Multi-threaded Logger:

Logs include severity, timestamp, file name, function name, line number, and message.

Supports log levels: DEBUG, WARNING, ERROR, CRITICAL.

Uses mutexes and non-blocking UDP sockets for thread-safe logging.

UDP-based Server:

Receives logs from multiple processes and writes to a central log file.

Provides runtime log level updates and log file dump options.

Python Automation Scripts:

Monitor server log file changes.

Update log levels dynamically.


Technologies Used:

C++17, POSIX Threads, UDP Sockets, Mutexes

How to Run:

Build Logger and LogServer with provided Makefiles.

Start the LogServer.

Run any client process using the logger.

Use Python script to monitor logs or interact with the dashboard.
