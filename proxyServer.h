#ifndef PROXYSERVER3_PROXYSERVER_H
#define PROXYSERVER3_PROXYSERVER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include "threadpool.h"

#define BUFFER_SIZE 4096

// Enumeration for error types
typedef enum {
    ERROR_400_BAD_REQUEST = 400,
    ERROR_404_NOT_FOUND = 404,
    ERROR_501_NOT_IMPLEMENTED = 501,
    ERROR_403_FORBIDDEN = 403,
    ERROR_500_INTERNAL = 500
} ErrorType;

typedef struct{
    char* host_name;
    char* clean_host_name;
    char* request;
    int client_socket;
    int host_port;
} communication_info;



/**
 * Forwards data read from a server socket to a client socket. This function reads chunks of data from
 * the server socket and immediately writes those chunks to the client socket. It continues this process
 * until there is no more data to read from the server socket, indicating the end of the response. This
 * function is typically used in proxy or forwarding scenarios where data needs to be relayed between a
 * client and a server transparently.
 *
 * @param server_socket: The socket descriptor for the connection to the destination server.
 * @param client_socket: The socket descriptor for the connection to the client.
 * @return
 *   - 1 on successful completion of data forwarding.
 *   - -1 if an error occurs during reading from the server or writing to the client.
 */
int get_response_from_destination(int server_socket, int client_socket);

/**
 * Configures and initializes the server socket for listening to incoming connections.
 * This function creates a TCP socket using the IPv4 protocol, binds it to the specified
 * address and port in the provided sockaddr_in structure, and sets it to listen for
 * incoming connections with a specified backlog queue.
 *
 * @param server_info: A struct sockaddr_in that contains the IP address and port number
 *                     on which the server should listen for incoming connections.
 * @return
 *   - The socket descriptor (a positive integer) of the successfully configured and
 *     listening socket, ready to accept incoming connections.
 *   - 0 if an error occurs during socket creation, binding, or setting it to listen.
 */
int set_my_server_configuration(struct sockaddr_in server_info);

/**
 * Establishes a TCP connection to a specified host and port, preparing a socket for communication.
 * This function attempts to resolve the hostname to an IP address and then creates a socket that
 * connects to the given port at the resolved IP address. It is used to connect to a destination
 * server in a client-server architecture, enabling the client to communicate with the server
 * over the established connection.
 *
 * @param host: The hostname or IP address of the destination server.
 * @param port: The port number on the destination server to connect to.
 * @return
 *   - The socket descriptor for the established connection, if successful.
 *   - -1 to indicate a failure in any step of the connection setup, including socket creation,
 *     hostname resolution, or the connection attempt itself.
 */
int set_destination_server_connection(const char* host, int port);

/**
 * Validates the HTTP version in a request string to ensure it is either HTTP/1.0 or HTTP/1.1.
 * This function searches the given string for the "HTTP/" prefix and checks the version
 * number immediately following this prefix. It is designed to ensure that requests adhere to
 * expected HTTP standards, specifically targeting versions 1.0 and 1.1, which are the most
 * commonly supported versions in HTTP-based applications.
 *
 * @param str: A string containing the HTTP request line to be checked.
 * @return
 *   - 1 if the HTTP version is either 1.0 or 1.1.
 *   - 0 if the HTTP version is not 1.0 or 1.1, or if the "HTTP/" prefix is not found.
 */
int is_legal_http_version(const char* str);

/**
 * Validates the format of the first line of an HTTP request string. This function checks if the request line
 * conforms to the expected format by containing three parts: an HTTP method, a path, and an HTTP version,
 * separated by spaces. The validation ensures that the request starts with a valid request line,
 * which is crucial for further processing of the request.
 *
 * @param str: The HTTP request string to be validated.
 * @return
 *   - 1 (true) if the first line of the request is in the correct format, indicating a method, path, and version.
 *   - 0 (false) if the first line does not conform to the expected format or if no end-of-line marker is found,
 *     indicating a malformed request.
 */
int is_legal_request_format(const char* str);

/**
 * Extracts the hostname from an HTTP request string. This function searches the request string
 * for the "Host: " header and extracts the hostname specified immediately after this header,
 * stopping at the first occurrence of a carriage return and newline sequence ("\r\n") that
 * denotes the end of the host line. This is useful in HTTP request parsing to identify the
 * target host for the request.
 *
 * @param str: A string containing the full HTTP request.
 * @return
 *   - A dynamically allocated string containing the hostname extracted from the HTTP request.
 *     The caller is responsible for freeing this memory.
 *   - NULL if the request is NULL, the "Host: " header is not found, or the hostname cannot be extracted.
 */
char* get_host_name(const char* str);

/**
 * Constructs and sends an HTTP error response based on a specified error type. The function
 * builds a complete HTTP response, including the appropriate status line, headers, and a
 * body that describes the error. It formats the response with the current date, the server
 * identifier, and sets the connection to close after sending the message. The content length
 * is calculated dynamically based on the error description to ensure the HTTP header correctly
 * reflects the response size.
 *
 * @param error: An ErrorType enumeration value that specifies the type of error to respond with.
 * @param sd: The socket descriptor to which the error message is sent.
 */
void send_error_message(ErrorType error, int sd);

/**
 * Extracts the hostname from a given string, omitting any "www." prefix and port numbers.
 * This function aims to standardize hostnames by removing common prefixes and stripping
 * port information if present. The cleaned hostname can then be used for consistent
 * processing or comparison against other hostnames within networking applications.
 *
 * @param host: A string containing the original hostname, possibly with "www." prefix and/or port number.
 * @return
 *   - A dynamically allocated string containing the cleaned hostname without the "www." prefix and port.
 *     The caller is responsible for freeing this memory.
 *   - NULL if memory allocation fails.
 */
char* get_clean_host(const char* host);

/**
 * Extracts the port number from a given string that contains a hostname or IP address followed
 * by an optional port number, separated by a colon. If no port is specified, a default value
 * of 80 is returned, assuming HTTP traffic. The function validates the extracted port number
 * to ensure it falls within the valid range for TCP/UDP ports (0 to 65535). If the port number
 * is invalid, not specified, or the input string is NULL, appropriate default or error values
 * are returned.
 *
 * @param str: A string containing the hostname or IP address, optionally followed by a colon and the port number.
 * @return
 *   - The extracted port number, if valid and specified.
 *   - 80 as a default value if no port number is specified.
 *   - -1 in case of errors, including invalid port numbers or processing errors.
 */
int get_port(const char* str);

/**
 * Checks if a given host is filtered based on a list stored in a file. The function first resolves
 * the host to its IP address, then converts this IP to its binary representation. It reads each line
 * of the specified file, treating lines that start with a digit as IP addresses (with optional subnet masks)
 * and compares these against the host's IP. Lines not starting with a digit are treated as hostnames and
 * directly compared to the given host. The host is considered filtered if a match is found in the file,
 * either by IP (considering subnet masks) or hostname.
 *
 * @param file_content: The content of the filter file.
 * @param host: The hostname to check against the filter list.
 * @return
 *   - 1 if the host is found in the filter list and is considered filtered.
 *   - 0 if the host is not found in the filter list.
 *   - -1 if there's an error resolving the host's IP or converting it to binary.
 */
int is_filtered_host(const char* file_content, const char* host);

/**
 * Retrieves the IP address for a given hostname and appends a "/32" subnet mask to it,
 * indicating a single host. This function first cleans the hostname by removing any
 * leading "www." prefix. It then uses the gethostbyname() system call to resolve the
 * hostname to an IP address. The resolved IP address is converted to a string and
 * concatenated with "/32" to denote a full, single-host mask.
 *
 * @param host: A string containing the hostname to be resolved.
 * @return
 *   - A dynamically allocated string containing the IP address of the host with a "/32" mask.
 *     The caller is responsible for freeing this memory.
 *   - NULL if the hostname cannot be cleaned, resolved, or if memory allocation fails.
 */
char* get_host_IP(const char* host);

/**
 * Converts an IP address with a subnet mask (in CIDR notation) to its binary representation,
 * limited to the number of bits specified by the mask. This function parses the input string
 * to extract the IP address and mask, then uses network utilities to convert the IP to binary
 * format. The binary representation is truncated or padded to match the mask size, providing
 * a string that represents the binary form of the IP address up to the specified subnet mask.
 *
 * @param ip_with_mask: A string containing the IP address and mask in CIDR notation (e.g., "192.168.1.1/24").
 * @return
 *   - A dynamically allocated string containing the binary representation of the IP address up to the mask.
 *     The caller is responsible for freeing this memory.
 *   - NULL if there's an error in conversion or memory allocation.
 */
char* ip_to_binary(const char* ip_with_mask);

/**
 * Compares two binary IP addresses up to a specified number of bits (mask size) to determine
 * if they are equivalent in the specified subnet mask. This is useful for operations such
 * as subnet filtering or matching IP addresses within the same subnet.
 *
 * @param binary_ip1: A string representing the first binary IP address to compare.
 * @param binary_ip2: A string representing the second binary IP address to compare.
 * @param mask_size: An integer specifying the number of bits to compare in the binary IP addresses.
 * @return
 *   - 1 if the specified bits of the IP addresses are the same, indicating they are within the same subnet.
 *   - 0 if the specified bits of the IP addresses differ, indicating they are in different subnets.
 */
int compare_binary_ips(const char* binary_ip1, const char* binary_ip2, int mask_size);

/**
 * Validates the HTTP request stored within the communication_info structure.
 * It checks for the presence of a host line, the HTTP version (1.0 or 1.1), and the
 * correct format of the request line (method, path, and version). It also validates
 * that the request uses the GET method. Additionally, this function checks whether
 * the requested host is filtered or blocked based on a predefined list.
 *
 * If any of these validations fail, an appropriate error message is sent to the client,
 * and the function returns 0 or -2 for specific failure cases.
 *
 * @param ci: A pointer to a communication_info structure containing the HTTP request
 *            and other relevant connection information.
 * @return
 *   - 1 if the request passes all checks and is considered legal.
 *   - 0 if any of the basic checks fail, or if the host is filtered or not found.
 *   - -2 if there is a problem with opening the filter file.
 */
int is_legal_request(communication_info* ci);

/**
 * Reads data from a client socket until the end of the HTTP headers is detected (indicated by "\r\n\r\n").
 * This function is designed to handle variable length reads by dynamically resizing the buffer as needed.
 * It also sets a read timeout to prevent indefinitely blocking on the socket read operation. If the read
 * operation times out or encounters an error, or if memory allocation fails, the function returns NULL.
 *
 * @param sd: The socket descriptor from which to read data.
 * @return
 *   - A dynamically allocated string containing the data read from the socket up to the end of the HTTP headers.
 *     The caller is responsible for freeing this memory.
 *   - NULL if a read error occurs, memory allocation fails, or the read operation times out.
 */
char* read_from_client_socket(int sd);


/**
 * Reads the entire content of a file specified by its file path and returns it as a single string.
 * Each line in the returned string is guaranteed to end with "\r\n", even if the original file does not
 * use this line ending convention. This is particularly useful for processing files in environments
 * that expect Windows-style line endings.
 *
 * @param file_path A string representing the path to the file to be read.
 * @return A pointer to a dynamically allocated string containing the file's content, with each
 *         line ending in "\r\n". The caller is responsible for freeing this memory. Returns NULL
 *         if the file cannot be opened or if a memory allocation fails.
 */
char* read_file_content(const char* file_path);

/**
 * Handles a single client request in a threaded server environment. This function performs multiple steps:
 * reading the request from the client socket, ensuring the "Connection: close" header is set, validating the
 * request, resolving the host, connecting to the destination server, forwarding the request, receiving the
 * response, and sending the response back to the client. It encapsulates the entire lifecycle of a proxy
 * server's handling of a client request, including error handling and resource cleanup.
 *
 * @param arg: A void pointer to a communication_info structure containing all necessary information
 *             about the client's request, including the client socket descriptor.
 * @return
 *   - 1 upon successful handling of the request, indicating the thread's work is complete.
 */
int thread_function(void* arg);

/**
 * Writes the entirety of a byte array to a socket, ensuring all data specified by the length is sent.
 * This function handles partial writes by continuing to send the remaining data until completion. It is
 * crucial for network communications where ensuring the complete transmission of data is necessary.
 *
 * @param sd: The socket descriptor to which the data will be written.
 * @param request: A char pointer to the byte array containing the data to be sent.
 * @param data_length: The total number of bytes in the array to send.
 * @return
 *   - The total number of bytes successfully written to the socket.
 *   - -1 if an error occurs during the write operation, indicating a failure to send the data.
 */
size_t write_to_socket(int sd, char* request, size_t data_length);

/**
 * Writes the entirety of an unsigned byte array to a socket. This function attempts to send all bytes
 * in the provided array to the specified socket descriptor, handling partial writes by continuing to send
 * the remainder of the data until the entire buffer has been transmitted. It is designed to ensure complete
 * data transfer, particularly useful in network communications where it's critical to send all data as a
 * continuous stream without loss.
 *
 * @param sd: The socket descriptor to which the data will be written.
 * @param request: An unsigned char pointer to the byte array that contains the data to be sent.
 * @param data_length: The total number of bytes in the array to send.
 * @return
 *   - The total number of bytes successfully written to the socket.
 *   - -1 if an error occurs during the write operation.
 */
size_t write_to_socket_unsigned(int sd, unsigned char* request, size_t data_length);

/**
 * Cleans up and deallocates resources associated with a communication_info structure.
 * This includes freeing any dynamically allocated memory for the host name, cleaned host name,
 * and the HTTP request. Additionally, if a client socket is open (indicated by a descriptor
 * not equal to -1), it is closed. This function ensures that all resources acquired during
 * the lifetime of the communication_info instance are properly released to avoid memory leaks
 * and to cleanly close any network connections.
 *
 * @param ci: A pointer to a communication_info structure whose resources are to be released.
 */
void destroy_communication_info(communication_info* ci);

/**
 * Initializes a communication_info structure by setting its fields to default values.
 * This includes setting string pointers to NULL, the client socket to -1 indicating
 * an invalid socket descriptor, and the host port to -1 indicating an unset port.
 *
 * @param ci: A pointer to a communication_info structure to be initialized.
 */
void init_communication_info(communication_info* ci);

/**
 * Modifies the HTTP request stored in a communication_info structure to ensure the
 * "Connection: close" header is present. This is to guarantee that the connection
 * will be closed after the request is completed.
 *
 * @param ci: A pointer to a communication_info structure containing the HTTP request.
 * @return
 *   - 1 if the request is successfully modified to include "Connection: close".
 *   - -1 if the request is NULL or if any error occurs during the modification process.
 */
int set_connection_close(communication_info* ci);

#endif