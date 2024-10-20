#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <vector>

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

// Comment out the unused function
// static void doSomething(int connfd){
//     char rbuf[64] = {};
//     ssize_t n = read(connfd, rbuf, sizeof(rbuf));
//     if(n < 0){
//         msg("read() error");
//         return;
//     }
//     printf("client says: %s\n", rbuf);

//     char wbuf[] = "world";
//     write(connfd, wbuf, strlen(wbuf));
// }

// static int32_t read_full(int fd, char *buf, size_t n) {
//     while (n > 0) {
//         ssize_t rv = read(fd, buf, n); // Attempt to read n bytes from fd into buf
//         if (rv <= 0) {
//             return -1; // Return -1 if read fails or EOF is reached
//         }
//         assert((size_t)rv <= n); // Ensure no overflow: rv must be <= n
//         n -= (size_t)rv; // Decrease n by the number of bytes read
//         buf += rv; // Move buffer pointer forward by rv bytes
//     }
//     return 0; // Return 0 if all n bytes are read successfully
// }

// static int32_t write_all(int fd, const char *buf, size_t n) {
//     while (n > 0) {
//         ssize_t rv = write(fd, buf, n); // Attempt to write n bytes from buf to fd
//         if (rv <= 0) {
//             return -1; // Return -1 if write fails
//         }
//         assert((ssize_t)rv <= (ssize_t)n); // Ensure no overflow: rv must be <= n
//         n -= (size_t)rv; // Decrease n by the number of bytes written
//         buf += rv; // Move buffer pointer forward by rv bytes
//     }
//     return 0; // Return 0 if all n bytes are written successfully
// }

const size_t k_max_msg = 4096; // Maximum allowed message size

// static int32_t one_request(int connfd) {
//     // 4 bytes header
//     char rbuf[4 + k_max_msg + 1]; // Buffer for reading data (header + max message + null terminator)
//     errno = 0; // Reset errno to 0
//     int32_t err = read_full(connfd, rbuf, 4); // Read the 4-byte header
//     if (err) {
//         if (errno == 0) {
//             msg("EOF"); // End of file
//         } else {
//             msg("read() error"); // Read error
//         }
//         return err; // Return error code
//     }
//     uint32_t len = 0;
//     memcpy(&len, rbuf, 4); // Copy first 4 bytes to len (assumes little-endian) - the header tells you how long the body will be
//     if (len > k_max_msg) {
//         msg("too long"); // Message too long
//         return -1; // Return error
//     }
//     // request body
//     err = read_full(connfd, &rbuf[4], len); // Read the message body of length len
//     if (err) {
//         msg("read() error"); // Read error
//         return err; // Return error code
//     }
//     // do something
//     rbuf[4 + len] = '\0'; // Null-terminate the message
//     printf("client says: %s\n", rbuf + 4); // Print the message starting from the 5th byte
//     // reply using the same protocol
//     const char reply[] = "world"; // Reply message
//     char wbuf[4 + sizeof(reply)]; // Buffer for reply (header + message)
//     len = (uint32_t)strlen(reply); // Length of the reply message
//     memcpy(wbuf, &len, 4); // Copy length of reply to first 4 bytes
//     memcpy(&wbuf[4], reply, len); // Copy reply message to buffer starting from the 5th byte
//     return write_all(connfd, wbuf, 4 + len); // Write the reply back to the client
// }

void process_request(const char *data, size_t len) {
    // Process a single request
    printf("Processing request: %.*s\n", static_cast<int>(len), data);
}

int bufferedIO(int connfd){
    int buffer_size = k_max_msg * 4;
    char buffer[buffer_size];
    size_t buffer_offset = 0;

    while(true){
        
        ssize_t rv = read(connfd, buffer + buffer_offset, buffer_size - buffer_offset);
        if(rv <= 0){
            if (rv == 0) {
                msg("EOF"); // End of file
            } else {
                msg("read() error"); // Read error
            }
            return rv; // Return error code
        }

        buffer_offset += rv;

        // process each request from buffer
        size_t processed_offset = 0;
        while(buffer_offset - processed_offset <= 4){
            uint32_t len = 0;
            memcpy(&len, buffer + processed_offset, 4);
            if(len > k_max_msg){
                msg("too long");
                return -1;
            }

            if(buffer_offset - processed_offset < 4 + len){
                msg("not enough data read");
                return -1;
            }

            process_request(buffer + processed_offset + 4, len);
            processed_offset += 4 + len;
        }

        // Move unprocessed data to the beginning of the buffer
        memmove(buffer, buffer + processed_offset, buffer_offset - processed_offset);
        buffer_offset -= processed_offset;
    }
}

int bind(int fd){
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);
    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    if(rv < 0){
        die("bind()");
    }
    return rv;
}

static void fd_set_nb(int fd){
    // Clear errno before making the call
    errno = 0;
    
    // Get the current file descriptor flags
    int flags = fcntl(fd, F_GETFL, 0);
    if(errno) { // Check for error
        die("fcntl error");
        return;
    }

    // Add the non-blocking flag to the current flags
    flags |= O_NONBLOCK;    // |= is an OR operation, if its there keep it, otherwise add it

    // Clear errno before making the call
    errno = 0;
    
    // Set the file descriptor flags to the new flags
    (void)fcntl(fd, F_SETFL, flags);
    if(errno) { // Check for error
        die("fcntl error");
        return;
    }
}

enum{
    STATE_REQ = 0,
    STATE_RES = 1,
    STATE_END = 2,
};

struct Conn{
    int fd = -1;
    uint32_t state = 0; // either STATE_REQ or STATE_RES
    // buffer for reading
    size_t rbuf_size = 0;
    uint8_t rbuf[4 + k_max_msg];
    // buffer for writing;
    size_t wbuf_size = 0;
    size_t wbuf_sent = 0;
    uint8_t wbuf[4 + k_max_msg];
};

static void conn_put(std::vector<Conn *> &fd2conn, struct Conn *conn){
    if(fd2conn.size() <= (size_t)conn->fd){
        fd2conn.resize(conn->fd);
    }
    fd2conn[conn->fd] = conn;
}

static int32_t accept_new_conn(std::vector<Conn *> &fd2conn, int fd){
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
    if(connfd < 0){
        msg("accept() error");
        return -1;
    }

    // set the new connection fd to nonblocking mode
    fd_set_nb(connfd);
    // creating the struct Conn
    struct Conn *conn = (struct Conn *)malloc(sizeof(struct Conn));
    if(!conn){
        close(connfd);
        return -1;
    }
    conn->fd = connfd;
    conn->state = STATE_REQ;
    conn->rbuf_size = 0;
    conn->wbuf_size = 0;
    conn->wbuf_sent = 0;
    conn_put(fd2conn, conn);
    return 0;
}

static bool try_flush_buffer(Conn *conn){
    ssize_t rv = 0;
    do {
        size_t remain = conn->wbuf_size - conn->wbuf_sent;
        rv = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain);
    } while (rv < 0 && errno == EAGAIN);
    if (rv < 0 && errno == EAGAIN){
        // got EAGAIN, stop
        return false;
    }
    if(rv < 0){
        msg("write() error");
        conn->state = STATE_END;
        return false;
    }
    conn->wbuf_sent += (size_t)rv;
    assert(conn->wbuf_sent <= conn->wbuf_size);
    if(conn->wbuf_sent == conn->wbuf_size){
        // response was fully sent, change state back
        conn->state = STATE_REQ;
        conn->wbuf_sent = 0;
        conn->wbuf_size = 0;
        return false;
    }
    // still got some data in wbuf, could try again
    return true;
}

static void state_res(Conn *conn){
    while(try_flush_buffer(conn));
}

static bool try_one_request(Conn *conn){
    // try to parse a request from the server
    if(conn->rbuf_size < 4){
        // not enough data in the buffer. Will try in the next iteration
        return false;
    }
    uint32_t len = 0;
    memcpy(&len, &conn->rbuf[0], 4);
    if(len > k_max_msg){
        msg("too long");
        conn->state = STATE_END;
        return false;
    }
    if(4 + len > conn->rbuf_size){
        // not enough data in the buffer. Will try in the next iteration
        return false;
    }

    // got one request, do something with it
    printf("client says: %.*s\n", len, &conn->rbuf[4]);

    // generating echoing response
    memcpy(&conn->wbuf, &len, 4);
    memcpy(&conn->wbuf[4], &conn->rbuf[4], len);
    conn->wbuf_size = 4 + len;

    // remove the request from the buffer
    // note: frequent memmove is inefficient
    // note: need better handling for production code
    size_t remain = conn->rbuf_size - 4 - len;
    if(remain){
        memmove(conn->rbuf, &conn->rbuf[4 + len], remain);
    }
    conn->rbuf_size = remain;

    // change state 
    conn->state = STATE_RES;
    state_res(conn);

    // continue the outer loop if the request was fully processed
    return (conn->state == STATE_REQ); 
}

static bool try_fill_buffer(Conn *conn){
    // try to fill the buffer
    assert(conn->rbuf_size < sizeof(conn->rbuf)); // Ensure buffer is not full
    ssize_t rv = 0; // Variable to store the return value of read()
    do{
        size_t cap = sizeof(conn->rbuf) - conn->rbuf_size; // Calculate remaining capacity in buffer
        rv = read(conn->fd, &conn->rbuf[conn->rbuf_size], cap); // Attempt to read data into buffer
    } while(rv < 0 && errno == EINTR); // Retry read if interrupted by a signal
    if(rv < 0 && errno == EAGAIN) {
        // got EAGAIN, stop
        return false; // Non-blocking read and no data available, stop reading
    }
    if(rv < 0){
        msg("read error"); // Read error occurred
        conn->state = STATE_END; // Set connection state to end
        return false;
    }
    if(rv == 0){
        if(conn->rbuf_size > 0){
            msg("unexpected EOF"); // Unexpected end of file, data in buffer but no more data to read
        } else {
            msg("EOF"); // End of file, no data in buffer
        }
        conn->state = STATE_END; // Set connection state to end
        return false;
    }

    conn->rbuf_size += (size_t)rv; // Update buffer size with the number of bytes read
    assert(conn->rbuf_size <= sizeof(conn->rbuf)); // Ensure buffer size does not exceed buffer capacity

    // try to process requests one by one
    while(try_one_request(conn)){} // Process as many complete requests as possible
    return(conn->state == STATE_REQ); // Return true if connection is still in request state
}


static void state_req(Conn *conn){
    while(try_fill_buffer(conn)){}
}

static void connection_io(Conn *conn){
    if(conn->state == STATE_REQ){
        state_req(conn);
    } else if(conn->state == STATE_RES){
        state_res(conn);
    } else{
        assert(0);   // not expected
    }
}

int main(){
    // create socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    /*
    AF_INET is iPV4 protocol
    SOCK_STREAM is the type of socket used
    */
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));    // sets options for the socket

    int rv = bind(fd);

    rv = listen(fd, SOMAXCONN);     // SOMAXCONN sets how many clients can be in the queue waiting to connect
    if(rv){
        die("listen()");
    }

    // A map of all client connections, keyed by fd
    std::vector<Conn *> fd2conn; // Vector to keep track of connections by their file descriptors (fd).

    // Set the listen fd to non-blocking mode
    fd_set_nb(fd); 

    // The event loop
    std::vector<struct pollfd> poll_args; // Vector to store pollfd structures for polling.
    while(true){
        // Prepare the arguments for the poll()
        poll_args.clear(); // Clear the poll_args vector for new pollfd structures.
        // For convenience, the listening fd is put in the first position 
        struct pollfd pfd = {fd, POLLIN, 0}; // Create a pollfd structure for the listening socket to listen for incoming connections.
        poll_args.push_back(pfd); // Add the listening fd to the poll_args vector.
        // Connection fds
        for(Conn *conn : fd2conn){ // Iterate over all connection pointers in fd2conn.
            if(!conn){
                continue; // Skip null connections.
            }
            struct pollfd pfd = {}; // Create a pollfd structure for each connection.
            pfd.fd = conn->fd; // Set the fd for the pollfd structure.
            pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT; // Set the events to listen for: POLLIN for read requests, POLLOUT for write responses.
            pfd.events |= POLLERR; // Also listen for errors.
            poll_args.push_back(pfd); // Add the pollfd structure to the poll_args vector.
        }

        // Poll for active fds
        // The timeout argument doesn't matter here
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), 1000); // Call poll() to wait for events on the fds.
        if(rv < 0){
            die("poll()"); // If poll() returns an error, exit the program.
        }
        
        // Process active connections
        for(size_t i = 1; i < poll_args.size(); ++i){ // Start from 1 to skip the listening fd.
            if(poll_args[i].revents){ // If there are events on this fd.
                Conn *conn = fd2conn[poll_args[i].fd]; // Get the corresponding connection.
                connection_io(conn); // Handle the connection's IO.
                if(conn->state == STATE_END){ // If the connection should be closed.
                    // Client closed normally, or something bad happened
                    // Destroy this connection
                    fd2conn[conn->fd] = NULL; // Remove the connection from fd2conn.
                    (void)close(conn->fd); // Close the file descriptor.
                    free(conn); // Free the connection memory.
                }
            }
        }

        // Try to accept a new connection if the listening fd is active
        if(poll_args[0].revents){ // Check if there are events on the listening fd.
            (void)accept_new_conn(fd2conn, fd); // Accept new connection and add it to fd2conn.
        }
    }

    return 0;

}