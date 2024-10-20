#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

int socket(){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
        die("socket()");
    }
    return fd;
}

int connect(int fd){
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if(rv){
        die("connect()");
    }
    return rv;
}

static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n); // Attempt to read n bytes from fd into buf
        if (rv <= 0) {
            return -1; // Return -1 if read fails or EOF is reached
        }
        assert((size_t)rv <= n); // Ensure no overflow: rv must be <= n
        n -= (size_t)rv; // Decrease n by the number of bytes read
        buf += rv; // Move buffer pointer forward by rv bytes
    }
    return 0; // Return 0 if all n bytes are read successfully
}

static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n); // Attempt to write n bytes from buf to fd
        if (rv <= 0) {
            return -1; // Return -1 if write fails
        }
        assert((size_t)rv <= n); // Ensure no overflow: rv must be <= n
        n -= (size_t)rv; // Decrease n by the number of bytes written
        buf += rv; // Move buffer pointer forward by rv bytes
    }
    return 0; // Return 0 if all n bytes are written successfully
}

const size_t k_max_msg = 4096; // Maximum allowed message size

// static int32_t query(int fd, const char *text){
    // uint32_t len = (uint32_t)strlen(text);  // store the length of the message in Length variable
    // if (len > k_max_msg){ // check the length of the query is less than 4KB
    //     return -1;
    // }
    // // create a place to store the message
    // char wbuf[4 + k_max_msg]; // 4 bytes for header and 4KB for body
    // memcpy(wbuf, &len, 4); // assume little endian, store the length of the message -len- in the header
    // memcpy(&wbuf[4], text, len); // store the text in the main body of the message, starting from index 4
    // if(int32_t err = write_all(fd, wbuf, 4 + len)) { // attempt to write this out into the port
    //     return err;
    // }
//     // now read response
//     // 4 bytes header
//     char rbuf[4 + k_max_msg + 1]; // create a buffer to read the message from server
//     errno = 0;
//     int32_t err = read_full(fd, rbuf, 4);
//     if(err){
//         if(errno == 0){
//             msg("EOF");
//         } else {
//             msg("read() error");
//         }
//         return err;
//     }
//     // copy the header of the return message and check if the size indicated is too big
//     memcpy(&len, rbuf, 4);
//     if(sizeof(len) > k_max_msg){
//         msg("too long");
//     }
//     //reply body
//     err = read_full(fd, &rbuf[4], len);
//     if(err){
//         msg("read() error");
//         return err;
//     }
//     // do something
//     rbuf[4 + len] = '\0';
//     printf("server says: %s\n", &rbuf[4]);
//     return 0;
// }
// int buffered_IO(int fd, const char *text){
// }

// Function to send a request to a file descriptor (socket).
// The request is built from a vector of strings (cmd) and sent using write_all.
// static int32_t send_req(int fd, const std::vector<std::string> &cmd) {
//     // Calculate the total length of the message, starting with the initial 4 bytes for the length.
//     uint32_t len = 4;
//     // Add the size of each command string plus 4 bytes for each string's length prefix.
//     for (const std::string &s : cmd) {
//         len += 4 + s.size();
//     }
//     // Check if the total length exceeds the maximum allowed message size.
//     if (len > k_max_msg) {
//         return -1;
//     }
//     // Create a buffer to hold the serialized request, including the length and command strings.
//     char wbuf[4 + k_max_msg];
//     // Copy the total length of the message to the start of the buffer.
//     memcpy(&wbuf[0], &len, 4);
//     // Copy the number of command strings (cmd.size()) to the buffer, starting at offset 4.
//     uint32_t n = cmd.size();
//     memcpy(&wbuf[4], &n, 4);
//     // Initialize the current offset in the buffer to 8 (after length and number of commands).
//     size_t cur = 8;
//     // Copy each command string to the buffer along with its length prefix.
//     for (const std::string &s : cmd) {
//         // Copy the length of the string.
//         uint32_t p = (uint32_t)s.size();
//         memcpy(&wbuf[cur], &p, 4);
//         // Copy the actual string data.
//         memcpy(&wbuf[cur + 4], s.data(), s.size());
//         // Move the offset to the next position after the current string.
//         cur += 4 + s.size();
//     }
//     // Write the entire buffer to the file descriptor (socket).
//     return write_all(fd, wbuf, 4 + len);
// }

static int32_t send_req(int fd, const char *text) {
    uint32_t len = (uint32_t)strlen(text);
    if (len > k_max_msg) {
        return -1;
    }

    char wbuf[4 + k_max_msg];
    memcpy(wbuf, &len, 4);  // assume little endian
    memcpy(&wbuf[4], text, len);
    return write_all(fd, wbuf, 4 + len);
}

static int32_t read_res(int fd) {
    // 4 bytes header
    char rbuf[4 + k_max_msg + 1];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if (err) {
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);  // assume little endian
    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }

    // reply body
    err = read_full(fd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }

    // do something
    rbuf[4 + len] = '\0';
    printf("server says: %s\n", &rbuf[4]);
    return 0;
}

// static int32_t read_res(int fd){
//     // 4 bytes header
//     char rbuf[4 + k_max_msg + 1];
//     errno = 0;
//     int32_t err = read_full(fd, rbuf, 4);
//     if(err){
//         if(errno == 0){
//             msg("EOF");
//         } else {
//             msg("read() error");
//         }
//         return err;
//     }
//     // reply body
//     uint32_t len = 0;
//     memcpy(&len, rbuf, 4);
//     if(len > k_max_msg){
//         msg("too long");
//         return -1;
//     }
//     // print the result
//     uint32_t rescode = 0;
//     if(len < 4){
//         msg("bad response");
//         return -1;
//     }
//     memcpy(&rescode, &rbuf[4], 4);
//     printf("server says: [%u] %.*s\n", rescode, len - 4, &rbuf[8]);
//     return 0;
// }

int main(){
    int fd = socket();

    int rv = connect(fd);
    if (rv) {
        die("connect");
    }

    // multiple pipelined requests
    const char *query_list[3] = {"hello1", "hello2", "hello3"};
    for (size_t i = 0; i < 3; ++i) {
        int32_t err = send_req(fd, query_list[i]);
        if (err) {
            goto L_DONE;
        }
    }
    for (size_t i = 0; i < 3; ++i) {
        int32_t err = read_res(fd);
        if (err) {
            goto L_DONE;
        }
    }

L_DONE:
    close(fd);
    return 0;
}
