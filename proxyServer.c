#include "proxyServer.h"


int set_connection_close(communication_info* ci)
{
    // Check if the request is NULL
    if (ci->request == NULL)
        return -1; // Return -1 indicating failure

    char* request = ci->request; // Alias for readability
    char* modified_request = NULL; // To hold the modified request
    char* connection_header = NULL; // To search for the Connection header in the request
    char* end_of_headers = strstr(request, "\r\n\r\n"); // Find the end of the HTTP headers

    // Attempt to find "Connection:" header in a case-insensitive manner
    const char* connection_patterns[] = {"Connection:", "connection:"};
    for (int i = 0; i < 2; ++i)
    {
        connection_header = strstr(request, connection_patterns[i]);
        if (connection_header != NULL)
            break; // Stop if found
    }

    // If Connection header is found and there are headers in the request
    if (connection_header != NULL && end_of_headers != NULL)
    {
        // Replace the Connection header's value with "close"
        char* start_of_next_header = strstr(connection_header, "\r\n");
        if (start_of_next_header != NULL)
        {
            // Calculate parts lengths for memory allocation
            size_t header_part_len = connection_header - request;
            size_t end_part_len = strlen(start_of_next_header);
            // Allocate memory for the modified request
            modified_request = malloc(header_part_len + strlen("Connection: close") + end_part_len + 1);
            if (!modified_request)
            {
                fprintf(stderr, "Memory allocation failed\n");
                return -1; // Return -1 if allocation fails
            }
            // Construct the modified request
            strncpy(modified_request, request, header_part_len);
            strcpy(modified_request + header_part_len, "Connection: close");
            strcpy(modified_request + header_part_len + strlen("Connection: close"), start_of_next_header);
        }
    }
    else if (end_of_headers != NULL)
    {
        // If Connection header is not found, prepare to add it before the end of headers marker "\r\n\r\n"
        size_t headers_len = end_of_headers - request;
        // Allocate memory for the modified request including space for "\r\nConnection: close\r\n\r\n"
        modified_request = malloc(headers_len + strlen("\r\nConnection: close\r\n\r\n") + 1);
        if (!modified_request)
        {
            fprintf(stderr, "Memory allocation failed\n");
            return -1; // Return -1 if allocation fails
        }
        // Copy the part of the request up to the end of headers marker into the modified request buffer
        strncpy(modified_request, request, headers_len);
        // Append "\r\nConnection: close\r\n\r\n" after the copied part, effectively replacing "\r\n\r\n" with it
        strcpy(modified_request + headers_len, "\r\nConnection: close\r\n\r\n");
    }
    else
        // Malformed request (no end of headers found)
        return -1; // Return -1 indicating failure

    // Replace the original request with the modified one
    free(ci->request);
    ci->request = modified_request;

    return 1; // Return 1 indicating success
}

void init_communication_info(communication_info* ci)
{
    ci->host_name = NULL;          // Set host name to NULL indicating no host name is set
    ci->clean_host_name = NULL;    // Set clean host name to NULL indicating no processed host name is set
    ci->request = NULL;            // Set request to NULL indicating no HTTP request is currently stored
    ci->filter_content = NULL;     // Set filter content to NULL indicating no filter content is available at the moment
    ci->client_socket = -1;        // Initialize client socket to -1, marking it as invalid or not yet assigned
    ci->host_port = -1;            // Initialize host port to -1, indicating that it is not yet specified
}


void destroy_communication_info(communication_info* ci)
{
    if (ci->host_name != NULL)
        free(ci->host_name); // Free the memory allocated for host name, if any

    if (ci->clean_host_name != NULL)
        free(ci->clean_host_name); // Free the memory allocated for cleaned host name, if any

    if (ci->request != NULL)
        free(ci->request); // Free the memory allocated for the HTTP request, if any

    if (ci->client_socket != -1)
        close(ci->client_socket); // Close the client socket if it is open

    if (ci->filter_content != NULL)
        free(ci->filter_content);

    free (ci);
}


int set_my_server_configuration(struct sockaddr_in server_info)
{
    int ws; // welcome socket descriptor

    // Create a TCP socket using IPv4
    if((ws = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("error: socket\n"); // Log error if socket creation fails
        return 0; // Return 0 to indicate failure
    }

    // Bind the socket to the provided address and port
    if(bind(ws, (struct sockaddr*) &server_info, sizeof(struct sockaddr_in)) < 0)
    {
        perror("error: bind\n"); // Log error if bind operation fails
        close(ws); // Close the socket to release resources
        return 0; // Return 0 to indicate failure
    }

    // Set the socket to listen for incoming connections, with a backlog queue of 5 connections
    if(listen(ws, 5) < 0)
    {
        perror("error: listen\n"); // Log error if listen operation fails
        close(ws); // Close the socket to release resources
        return 0; // Return 0 to indicate failure
    }

    return ws; // Return the welcome socket descriptor on success
}


void send_error_message(ErrorType error, int sd)
{
    char header[BUFFER_SIZE]; // Buffer for HTTP response headers
    char body[BUFFER_SIZE]; // Buffer for the HTML body of the error message
    time_t now = time(NULL); // Current time for the Date header
    struct tm* gmt = gmtime(&now); // Convert time to GMT
    char date[100]; // Buffer for formatted date string
    // Format the date according to the HTTP date standard
    strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", gmt);

    const char* title; // Variable for storing the status line text
    const char* description; // Variable for storing the error description

    // Switch to set the title and description based on the ErrorType
    switch (error) {
        case ERROR_500_INTERNAL:
            title = "500 Internal Server Error";
            description = "Some server side error.";
            break;
        case ERROR_400_BAD_REQUEST:
            title = "400 Bad Request";
            description = "Bad Request.";
            break;
        case ERROR_404_NOT_FOUND:
            title = "404 Not Found";
            description = "File not found.";
            break;
        case ERROR_501_NOT_IMPLEMENTED:
            title = "501 Not supported";
            description = "Method is not supported.";
            break;
        case ERROR_403_FORBIDDEN:
            title = "403 Forbidden";
            description = "Access denied.";
            break;
        default:
            return; // Exit the function if the error type is unknown
    }

    // Construct the HTML body with the error title and description
    snprintf(body, sizeof(body), "<HTML><HEAD><TITLE>%s</TITLE></HEAD>\r\n"
                                 "<BODY><H4>%s</H4>\r\n"
                                 "%s\r\n"
                                 "</BODY></HTML>", title, title, description);

    size_t body_length = strlen(body); // Calculate the content length

    // Construct the HTTP response headers
    snprintf(header, sizeof(header),
             "HTTP/1.1 %s\r\n"
             "Server: webserver/1.0\r\n"
             "Date: %s\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n",
             title, date, body_length);

    // Combine the headers and the body into a full HTTP response message
    char full_message[BUFFER_SIZE * 2]; // Ensure enough space for headers and body
    snprintf(full_message, sizeof(full_message), "%s%s", header, body);

    // Send the complete HTTP response to the client
    write_to_socket(sd, full_message, strlen(full_message));
}



int is_legal_request_format(const char* str)
{
    const char* end_of_first_line = strstr(str, "\r\n");
    if (end_of_first_line == NULL)  // Malformed request: no end of line
        return 0;

    // Calculate the length of the first line and create a buffer for it
    size_t first_line_length = end_of_first_line - str;
    char first_line[first_line_length + 1]; // +1 for null terminator

    // Copy the first line into the buffer and null-terminate it
    strncpy(first_line, str, first_line_length);
    first_line[first_line_length] = '\0';

    char method[64]; // Buffer for the HTTP method
    char path[1024]; // Buffer for the PATH
    char version[64]; // Buffer for the HTTP version

    // Attempt to read the three elements from the first line of str
    int result = sscanf(first_line, "%63s %1023s %63s", method, path, version);

    return result == 3;
}


int is_legal_request(communication_info* ci)
{
    // Extract the host name from the request
    ci->host_name = get_host_name(ci->request);

    // Validate presence of host, HTTP version, and request format
    if (ci->host_name == NULL || !is_legal_http_version(ci->request) || !is_legal_request_format(ci->request))
    {
        // If any check fails, send a 400 Bad Request error and return 0
        send_error_message(ERROR_400_BAD_REQUEST, ci->client_socket);
        return 0;
    }

    // Ensure the request uses the GET method
    if (strncmp(ci->request, "GET ", 4) != 0)
    {
        // If not a GET request, send a 501 Not Implemented error and return 0
        send_error_message(ERROR_501_NOT_IMPLEMENTED, ci->client_socket);
        return 0;
    }

    // Check if the host is filtered or blocked
    int is_filtered = is_filtered_host(ci->filter_content, ci->host_name);

    if (is_filtered == -1)
    {
        // If the host's IP could not be found, send a 404 Not Found error and return 0
        send_error_message(ERROR_404_NOT_FOUND, ci->client_socket);
        return 0;
    }

    if (is_filtered == 1)
    {
        // If the host is filtered or blocked, send a 403 Forbidden error and return 0
        send_error_message(ERROR_403_FORBIDDEN, ci->client_socket);
        return 0;
    }

    // If all checks pass, return 1 indicating the request is legal
    return 1;
}


int compare_binary_ips(const char* binary_ip1, const char* binary_ip2, int mask_size)
{
    // Loop through each bit up to mask_size to compare the two binary IP strings
    for (int i = 0; i < mask_size; ++i)
        // If at any point the bits differ, return 0 indicating IPs are different in the compared bits
        if (binary_ip1[i] != binary_ip2[i])
            return 0;

    // If the loop completes without finding any differing bits, return 1 indicating IPs are the same within the mask
    return 1;
}


char* ip_to_binary(const char* ip_with_mask)
{
    struct in_addr ip_addr; // Structure to store the converted IP address
    long mask; // To store the subnet mask size
    char* binary_masked; // Pointer for the binary representation of the IP
    char ip_part[16], mask_str[5]; // Buffers to store the separated IP and mask from the input
    char* end_ptr; // For error checking in strtol
    char modified_ip_with_mask[100]; // Buffer to potentially modify the input IP with a default mask

    // Initialize mask_str with default "/32"
    strcpy(mask_str, "/32");

    // Check if the input already contains a mask
    if (strchr(ip_with_mask, '/') == NULL)
        // If no mask is present, append "/32" to the input IP address
        snprintf(modified_ip_with_mask, sizeof(modified_ip_with_mask), "%s%s", ip_with_mask, mask_str);
    else
    {
        // If a mask is present, use the input as is
        strncpy(modified_ip_with_mask, ip_with_mask, sizeof(modified_ip_with_mask) - 1);
        modified_ip_with_mask[sizeof(modified_ip_with_mask) - 1] = '\0'; // Ensure null-termination
    }

    // Extract IP address and mask size from the potentially modified input string
    sscanf(modified_ip_with_mask, "%15[^/]/%4s", ip_part, mask_str);

    // Convert the mask from string to long
    mask = strtol(mask_str, &end_ptr, 10);

    // Validate mask size and set to 32 if out of valid range
    if (end_ptr == mask_str || *end_ptr != '\0' || mask < 0 || mask > 32)
        mask = 32; // Correct invalid or out-of-range mask to 32

    // Convert the IP address to binary form
    if (!inet_pton(AF_INET, ip_part, &ip_addr))
    {
        fprintf(stderr, "Invalid IP address format.\n");
        return NULL;
    }

    unsigned long ip_binary = ntohl(ip_addr.s_addr); // Network to host long order conversion

    // Allocate memory for the binary representation
    binary_masked = (char*)malloc(mask + 1); // +1 for null terminator
    if (!binary_masked)
    {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL;
    }

    // Generate the binary string based on the mask size
    for (int i = 0; i < mask; i++)
    {
        unsigned long bit = (ip_binary >> (31 - i)) & 1; // Extract each bit based on the mask
        binary_masked[i] = bit ? '1' : '0';
    }
    binary_masked[mask] = '\0'; // Null-terminate the binary string

    return binary_masked;
}


char* get_host_IP(const char* host)
{
    struct hostent* he; // Structure to store host information

    // Clean the hostname to ensure uniformity, e.g., remove "www."
    char* clean_host = get_clean_host(host);
    if (clean_host == NULL)
        return NULL; // Failed to clean host name

    // Resolve the cleaned hostname to an IP address
    if ((he = gethostbyname(clean_host)) == NULL)
    {
        herror("error: gethostbyname"); // Log error if hostname resolution fails
        free(clean_host); // Clean up allocated memory for cleaned host
        return NULL;
    }

    free(clean_host); // Free the cleaned hostname after use

    // Correctly access the first IP address from h_addr_list and convert to string
    struct in_addr* first_ip_address = (struct in_addr*)he->h_addr_list[0];
    const char* ip_str = inet_ntoa(*first_ip_address);
    if (ip_str == NULL)
    {
        fprintf(stderr, "Error converting IP to string.\n");
        return NULL;
    }

    // Allocate memory for the IP string with added "/32" subnet mask
    char* ip_with_mask = (char*)malloc(strlen(ip_str) + 4); // +3 for "/32" and +1 for null terminator
    if (ip_with_mask == NULL)
    {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL;
    }

    // Format the IP string with the "/32" mask
    sprintf(ip_with_mask, "%s/32", ip_str);

    return ip_with_mask; // Return the formatted IP address with mask
}


int is_filtered_host(const char* file_content, const char* host)
{
    char* host_ip = get_host_IP(host); // Includes /32 mask
    if (host_ip == NULL)
        return -1; // Failed to resolve IP

    char* binary_host_ip = ip_to_binary(host_ip); // Convert IP to binary
    if (binary_host_ip == NULL)
    {
        free(host_ip);
        return -1; // Failed to convert IP to binary
    }

    free(host_ip); // Free resolved IP string after conversion

    // Duplicate file_content to avoid modifying the original string with strtok
    char* content_copy = strdup(file_content);
    if (!content_copy)
    {
        free(binary_host_ip);
        fprintf(stderr, "Memory allocation failed for duplicating file content.\n");
        return -1; // Failed to duplicate file content
    }

    int result = 0; // Default to no match
    char* line = strtok(content_copy, "\r\n"); // Start tokenizing the file content

    while (line != NULL)
    {
        if (isdigit(line[0])) { // Treat lines starting with a digit as IP addresses
            char* binary_line_ip = ip_to_binary(line); // Convert line IP to binary
            if (!binary_line_ip)
            {
                line = strtok(NULL, "\r\n"); // Move to next line
                continue; // Skip invalid IPs
            }

            int mask_size = 32; // Default mask size
            char* slash = strchr(line, '/');
            if (slash)
            {
                long mask = strtol(slash + 1, NULL, 10);
                mask_size = (mask >= 0 && mask <= 32) ? (int)mask : 32;
            }

            if (compare_binary_ips(binary_host_ip, binary_line_ip, mask_size))
            {
                result = 1; // Match found
                free(binary_line_ip);
                break;
            }

            free(binary_line_ip);
        } else if (strcmp(host, line) == 0)
        { // Direct hostname comparison
            result = 1; // Match found
            break;
        }

        line = strtok(NULL, "\r\n"); // Move to next line
    }

    free(binary_host_ip); // Free binary host IP
    free(content_copy); // Free the duplicated file content
    return result; // Return comparison result
}


char* get_clean_host(const char* host)
{
    const char* start = host; // Start with the original host string

    // Check and skip the "http://" prefix if present to normalize the hostname
    if (strncmp(start, "http://", 7) == 0)
        start += 7; // Advance past the "http://"

    // Check and skip the "www." prefix if present to normalize the hostname
    if (strncmp(start, "www.", 4) == 0)
        start += 4; // Advance past the "www."

    // Look for a colon which indicates a port number and isolate the hostname part
    const char* colon_pos = strchr(start, ':');
    size_t length; // To hold the length of the clean host string
    if (colon_pos != NULL)
        // If a colon is found, calculate length up to the colon to exclude the port number
        length = colon_pos - start;

    else
        // If no colon is found, the entire remaining string is part of the hostname
        length = strlen(start);


    // Allocate memory for the clean host string, including space for a null terminator
    char* clean_host = (char*)malloc(length + 1);
    if (!clean_host)
    {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL; // Return NULL on allocation failure
    }

    // Copy the clean host part of the string into the newly allocated memory
    strncpy(clean_host, start, length);
    clean_host[length] = '\0'; // Ensure the string is null-terminated


    return clean_host; // Return the clean host string
}


char* get_host_name(const char* str)
{
    if (str == NULL)
        return NULL; // Check for a NULL request string and return NULL if true

    // Search for the "Host: " header within the request string
    const char* host_start = strstr(str, "Host: ");
    if (host_start != NULL)
    {
        host_start += 6; // Skip over the "Host: " part to get to the start of the hostname
        // Look for the end of the line, marked by "\r\n", to find the end of the hostname
        const char* host_end = strstr(host_start, "\r\n");
        if (host_end != NULL)
        {
            // Calculate the length of the hostname by subtracting pointers
            size_t host_length = host_end - host_start;
            // Allocate memory for the hostname plus a null terminator
            char *host_name = (char*)malloc(host_length + 1);
            if (!host_name)
                return NULL; // Ensure memory allocation was successful

            // Copy the hostname into the newly allocated buffer
            strncpy(host_name, host_start, host_length);
            host_name[host_length] = '\0'; // Null-terminate the hostname string

            return host_name; // Return the extracted hostname
        }
    }

    return NULL; // Return NULL if "Host: " is not found or the hostname cannot be extracted
}


int is_legal_http_version(const char* str)
{
    // Look for "HTTP/" followed by "1.0" or "1.1"
    const char *pos = strstr(str, "HTTP/");
    if (pos != NULL && (strncmp(pos + 5, "1.0", 3) == 0 || strncmp(pos + 5, "1.1", 3) == 0))
        return 1;

    return 0;
}



char* read_from_client_socket(int sd)
{
    int buffer_size = BUFFER_SIZE; // Initial buffer size
    ssize_t total_bytes_read = 0; // Total number of bytes read
    ssize_t bytes_read; // Number of bytes read in the last read operation
    struct timeval tv = {5, 0}; // Timeout for read operation

    // Allocate initial buffer
    char* buffer = (char*)malloc(buffer_size);
    if (!buffer)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL; // Return NULL on allocation failure
    }

    // Set socket read timeout
    if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval)) == -1)
    {
        perror("error: setsockopt\n");
        free(buffer);
        return NULL; // Return NULL if setting the socket option fails
    }

    while (1)
    {
        // Expand buffer if needed
        if (total_bytes_read >= buffer_size - 1)
        {
            buffer_size *= 2; // Double the buffer size
            char* new_buffer = (char*)realloc(buffer, buffer_size);
            if (!new_buffer)
            {
                fprintf(stderr, "Buffer reallocation failed\n");
                free(buffer);
                return NULL; // Return NULL on reallocation failure
            }
            buffer = new_buffer;
        }

        // Read data from socket
        bytes_read = read(sd, buffer + total_bytes_read, buffer_size - total_bytes_read - 1);
        if (bytes_read <= 0) // Check for read errors or timeout
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                printf("Read operation timed out\n");
            else
                perror("error: read\n");

            free(buffer);
            return NULL; // Return NULL on read error or timeout
        }

        total_bytes_read += bytes_read; // Update total bytes read
        buffer[total_bytes_read] = '\0'; // Null-terminate the buffer

        // Check if the end of headers has been reached
        if (strstr(buffer, "\r\n\r\n") != NULL)
            break; // End of headers found
    }

    return buffer; // Return the buffer containing the headers
}


int set_destination_server_connection(const char* host, int port)
{
    int sd; // Socket descriptor
    struct hostent* server_info; // Information about the host
    struct sockaddr_in socket_info; // Socket address information

    // Attempt to create a socket with IPv4 and TCP
    if ((sd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("error: socket");
        return -1; // Indicate failure
    }

    // Initialize socket address structure
    memset(&socket_info, 0, sizeof(struct sockaddr_in));
    socket_info.sin_family = AF_INET; // Use IPv4 address family

    // Resolve hostname to IP address
    server_info = gethostbyname(host);
    if (server_info == NULL) {
        herror("error: gethostbyname");
        close(sd); // Clean up the socket
        return -1; // Indicate failure
    }

    // Correctly set the server's IP address in the socket structure using the first IP address in the list
    socket_info.sin_addr = *(struct in_addr *)server_info->h_addr;

    // Set the server port, converting from host byte order to network byte order
    socket_info.sin_port = htons(port);

    // Attempt to connect to the server
    if (connect(sd, (struct sockaddr*)&socket_info, sizeof(socket_info)) == -1) {
        perror("error: connect");
        close(sd); // Clean up the socket
        return -1; // Indicate failure
    }

    return sd; // Return the socket descriptor for the successful connection
}


int get_port(const char* str)
{
    if (str == NULL)
        return -1; // Return error if input string is NULL

    // Attempt to find a colon which separates the host/IP from the port number
    const char* colon_pos = strchr(str, ':');
    if (colon_pos == NULL)
        return 80; // Return default HTTP port if no colon/port is specified

    char* end_ptr;
    // Convert the string portion after the colon to a long integer, representing the port
    long port_number = strtol(colon_pos + 1, &end_ptr, 10); // Parse the port number

    // Check if conversion resulted in a valid number
    if ((colon_pos + 1) == end_ptr)
        return 80; // Return default port if conversion fails

    // Check for out-of-range values
    if (errno == ERANGE && (port_number == LONG_MAX || port_number == LONG_MIN))
    {
        fprintf(stderr, "Port number out of range.\n");
        return -1; // Return error for out-of-range port numbers
    }

    // Validate port number is within the valid TCP/UDP port range
    if (port_number < 0 || port_number > 65535)
    {
        fprintf(stderr, "Invalid port number. Port number should be between 0 and 65535.\n");
        return -1; // Return error for invalid port numbers
    }

    return (int)port_number; // Return the valid, extracted port number
}


int get_response_from_destination(int server_socket, int client_socket)
{
    unsigned char buffer[BUFFER_SIZE]; // Buffer for temporary data storage
    size_t total_read_bytes = 0; // Track the total number of bytes forwarded

    while (1)
    {
        // Attempt to read data from the server socket
        ssize_t read_bytes = read(server_socket, buffer, sizeof(buffer));
        if (read_bytes < 0)
        {
            perror("error: read\n");
            return -1; // Return error if read operation fails
        }

        if (read_bytes == 0)
            break; // Break the loop if end of data stream is reached (no more data)

        // Attempt to write the read data to the client socket
        if (write_to_socket_unsigned(client_socket, buffer, read_bytes) < read_bytes)
        {
            // Return error if not all data could be written to the client socket
            return -1;
        }

        total_read_bytes += read_bytes; // Accumulate the count of bytes forwarded
    }

    return 1; // Return success after all data has been forwarded
}



int thread_function(void* arg)
{
    communication_info* ci = (communication_info*) arg; // Cast argument to communication_info structure
    // Read the request from the client socket
    ci->request = read_from_client_socket(ci->client_socket);
    if (ci->request == NULL) // Check for read failure
    {
        send_error_message(ERROR_500_INTERNAL, ci->client_socket); // Send error response to client
        goto finish; // Skip to clean up and exit
    }

    // Ensure the connection will be closed after the request
    if (set_connection_close(ci) != 1)
    {
        send_error_message(ERROR_500_INTERNAL, ci->client_socket); // Send error response to client
        goto finish; // Skip to clean up and exit
    }


    // Validate the request format and headers
    if (!is_legal_request(ci))
        goto finish; // Skip to clean up and exit if request is illegal

    // Resolve and clean the host name from the request
    ci->clean_host_name = get_clean_host(ci->host_name);
    if (ci->clean_host_name == NULL)
    {
        send_error_message(ERROR_500_INTERNAL, ci->client_socket); // Send error response to client
        goto finish; // Skip to clean up and exit
    }

    // Get the port from the host header
    ci->host_port = get_port(ci->host_name);
    if (ci->host_port == -1)
    {
        send_error_message(ERROR_500_INTERNAL, ci->client_socket); // Send error response to client
        goto finish; // Skip to clean up and exit
    }

    // Connect to the destination server
    int destination_server_sd = set_destination_server_connection(ci->clean_host_name, ci->host_port);
    if (destination_server_sd != -1) // Check if connection was successful
    {
        // Forward the request to the destination server and get the response
        if (write_to_socket(destination_server_sd, ci->request, strlen(ci->request)) == -1 ||
            get_response_from_destination(destination_server_sd, ci->client_socket) == -1)
        {
            send_error_message(ERROR_500_INTERNAL, ci->client_socket); // Send error response to client
        }

        close(destination_server_sd); // Close the connection to the destination server
    }

    finish:
    destroy_communication_info(ci); // Clean up the communication_info structure
    return 1; // Indicate successful completion
}


size_t write_to_socket_unsigned(int sd, unsigned char* request, size_t data_length)
{
    size_t total_written_bytes = 0; // Initialize counter for tracking bytes sent

    do {
        // Write data to socket from the current offset
        ssize_t wrote_bytes = write(sd, request + total_written_bytes, data_length - total_written_bytes);

        if (wrote_bytes < 0) // Check for write error
        {
            perror("error: write\n"); // Report error
            return -1; // Return -1 on error
        }

        total_written_bytes += wrote_bytes; // Accumulate total bytes sent

    } while (total_written_bytes < data_length); // Ensure all data is sent

    return total_written_bytes; // Return the total bytes successfully written
}


size_t write_to_socket(int sd, char* request, size_t data_length)
{
    size_t total_written_bytes = 0; // Counter for total bytes successfully written



    do {
        // Attempt to write the request to the socket
        ssize_t wrote_bytes = write(sd, request + total_written_bytes, data_length - total_written_bytes);

        if (wrote_bytes < 0)
        {
            perror("error: write\n"); // Print error if write fails
            return -1;
        }

        // Increment the count of total bytes written
        total_written_bytes += wrote_bytes;

    } while (total_written_bytes < data_length); // Continue until the entire request is sent

    return total_written_bytes; // Return the count of total bytes written
}


char* read_file_content(const char* file_path)
{
    // Attempt to open the file for reading
    FILE* file = fopen(file_path, "r");
    if (!file)
    {
        fprintf(stderr, "Error opening file %s for reading.\n", file_path);
        return NULL;
    }

    // Move to the end of the file to determine its size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file); // Get the file size
    fseek(file, 0, SEEK_SET); // Reset the file pointer to the beginning

    // Allocate memory for the content, +1 for the null terminator
    char* content = malloc(file_size + 1);
    if (!content)
    {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    ssize_t read;
    size_t len = 0; // getline will allocate memory for the line
    char* line = NULL; // Pointer to the line read by getline
    long total_length = 0; // Track the total length of content read

    // Read the file line by line
    while ((read = getline(&line, &len, file)) != -1)
    {
        // Replace the last newline character with "\r\n"
        if (line[read - 1] == '\n')
        {
            line[read - 1] = '\r'; // Carriage return
            line[read] = '\n'; // New line
            line[read + 1] = '\0'; // Null-terminate the modified line
            read++; // Account for the added character
        }

        // Check if the current content buffer size is sufficient
        if (total_length + read >= file_size)
        {
            file_size = total_length + read + 1; // Update the expected file size
            char* temp_content = realloc(content, file_size); // Attempt to reallocate memory
            if (!temp_content)
            {
                fprintf(stderr, "Memory reallocation failed\n");
                free(content); // Avoid memory leak
                free(line); // getline allocates memory for line
                fclose(file);
                return NULL;
            }
            content = temp_content; // Update the content pointer after successful reallocation
        }

        // Copy the current line into the content buffer
        memcpy(content + total_length, line, read);
        total_length += read; // Update the total length of content
    }

    content[total_length] = '\0'; // Ensure the content is null-terminated

    // Cleanup
    free(line); // Free the memory allocated by getline
    fclose(file); // Close the file

    return content; // Return the dynamically allocated content
}

int main(int argc, char* argv[])
{
    // Check command line arguments for correct usage
    if (argc != 5)
    {
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
        exit(EXIT_FAILURE); // Exit if the number of arguments is incorrect
    }

    // Initialize server address structure
    struct sockaddr_in server_info;
    // Convert command line arguments to appropriate types
    long temp_port = (long) strtoul(argv[1], NULL, 10); // Server port
    size_t pool_size = strtoul(argv[2], NULL, 10); // Thread pool size
    size_t max_tasks = strtoul(argv[3], NULL, 10); // Maximum number of tasks
    char* filter = argv[4]; // Path to filter file

    if (temp_port < 0 || temp_port > 65535 || max_tasks < 1)
    {
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
        exit(EXIT_FAILURE); // Exit if the number of arguments is incorrect
    }
    in_port_t port = (in_port_t)temp_port;

    // Create a thread pool for handling connections
    threadpool* tp = create_threadpool((int)pool_size);
    if (tp == NULL)
        exit(EXIT_FAILURE); // Exit if thread pool creation fails

    char* filter_content = read_file_content(filter);
    if (filter_content == NULL)
    {
        destroy_threadpool(tp);
        exit(EXIT_FAILURE);
    }

    // Zero out the server address structure
    memset(&server_info, 0, sizeof(struct sockaddr_in));

    // Configure the server address
    server_info.sin_family = AF_INET; // Use IPv4 addresses
    server_info.sin_port = htons(port); // Set the port number, converting to network byte order
    server_info.sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections to any of the server's IP addresses

    // Set up the server configuration (socket creation, binding, and listening)
    int ws = set_my_server_configuration(server_info);
    if (ws == -1)
    {
        free (filter_content);
        exit(EXIT_FAILURE); // Exit if server setup fails
    }

    // Main loop to accept connections and dispatch them to the thread pool
    for (size_t i = 0; i < max_tasks; i++)
    {
        communication_info* ci = (communication_info*)malloc(sizeof(communication_info));
        if (ci == NULL)
        {
            fprintf(stderr, "Memory allocation failed\n");
            break; // Exit loop on memory allocation failure
        }

        init_communication_info(ci); // Initialize the communication info structure

        // Accept a connection
        ci->client_socket = accept(ws, NULL, NULL);
        if (ci->client_socket < 0)
        {
            perror("error: accept\n");
            free(ci); // Ensure allocated memory is freed on failure
            break; // Exit loop on accept failure
        }

        ci->filter_content = strdup(filter_content);
        dispatch(tp, thread_function, (void*)ci); // Dispatch the connection to a thread in the pool
    }

    close(ws); // Close the server socket
    destroy_threadpool(tp); // Clean up the thread pool
    free(filter_content);

    exit(EXIT_SUCCESS);
}