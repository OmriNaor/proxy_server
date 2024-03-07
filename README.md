# C Project: ProxyServer

## Introduction

ProxyServer is a lightweight, multi-threaded HTTP proxy server written in C. It is designed to forward requests from clients to destination servers, applying filtering rules to block access to certain resources based on a text file containing disallowed hostnames or IP addresses.

## Features

ProxyServer offers several key features:

- Forwarding HTTP GET requests to destination servers.
- Handling multiple concurrent client connections using a thread pool.
- Filtering requests based on hostnames or IP addresses specified in a text file, with support for CIDR notation for IP filtering.
- Dynamic handling of client and server connections.
- Logging and error handling capabilities.

## Components

- `main.c`: The entry point of the program, handling argument parsing and server initialization.
- `threadpool.c/h`: Implementation of a thread pool for managing concurrent client connections.
- `filter.txt`: A sample text file containing rules for filtering requests. Users should replace this with their own filtering rules file.

## How It Works

The program operates as follows:

1. Initializes the server with specified configurations: port, pool size, maximum number of requests, and path to the filter file.
2. Listens for incoming client connections, dispatching them to the thread pool for processing.
3. Each worker thread:
   - Reads the client's request.
   - Validates and possibly modifies the request (e.g., adding "Connection: close").
   - Checks the filter file to allow or block the request based on predefined rules.
   - Forwards the request to the destination server if allowed.
   - Returns the response to the client.

## Browser Configuration

To use ProxyServer, configure your web browser's network settings to use the proxy. Set the proxy address to `localhost` and the port to the one specified when starting ProxyServer. 
This setup must be completed before attempting to access websites through the browser, ensuring that web traffic is correctly routed through the proxy server.

## Compilation and Execution

To compile and run the project, follow these steps:

1. Clone the repository or download the source code.
2. Navigate to the project directory.
3. Compile the project using a C compiler (e.g., `gcc` or `clang`): gcc *.c -o proxyServer -lpthread
4. Run the compiled executable with the necessary arguments: ./proxyServer <port> <pool-size> <max-number-of-request> <path-to-filter.txt>

## Remarks

- ProxyServer is capable of handling basic HTTP GET requests and is primarily intended for educational and demonstration purposes.
- The project showcases multi-threaded server design, socket programming, HTTP protocol nuances, and basic content filtering mechanisms in C.

## Getting Started

1. Ensure you have GCC or another C compiler installed on your system.
2. Compile the project using the provided command.
3. Configure your browser to route traffic through the proxy server.
4. Start the proxy server with appropriate configuration arguments, specifying the path to your filter text file.
   

For testing and demonstration purposes, you may use the following HTTP URLs with ProxyServer to explore its capabilities:

- [PDF widgets sample](http://www.pdf995.com/samples/widgets.pdf)
- [Physics questions](http://www.csun.edu/science/ref/games/questions/97_phys.pdf)
- [PDF995 website](http://www.pdf995.com/)
- [HTTP Forever](http://httpforever.com/index.html)
- [Joseph W Carrillo's website](http://www.josephwcarrillo.com/index.html)
- [022 Israeli news site](http://www.022.co.il/welcome.html)
- [HMPJ Hebrew site](http://www.hmpj.manhi.org.il/)

Please note that these URLs are provided for testing and demonstration purposes only and are meant to illustrate the usage of HTTP (not HTTPS) with ProxyServer.
