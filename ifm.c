#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

/*
    The BNF representation for this is:

    <message>  ::= [':' <prefix> <SPACE> ] <command> <params> <crlf>
    <prefix>   ::= <servername> | <nick> [ '!' <user> ] [ '@' <host> ]
    <command>  ::= <letter> { <letter> } | <number> <number> <number>
    <SPACE>    ::= ' ' { ' ' }
    <params>   ::= <SPACE> [ ':' <trailing> | <middle> <params> ]

    <middle>   ::= <Any *non-empty* sequence of octets not including SPACE
                   or NUL or CR or LF, the first of which may not be ':'>
    <trailing> ::= <Any, possibly *empty*, sequence of octets not including
                     NUL or CR or LF>
    <crlf>     ::= CR LF
*/

typedef enum {
    CMD_UNKNOW = 0,
    CMD_NICK,
    CMD_USER,
    CMD_JOIN,
    CMD_PART,
    CMD_PRIVMSG,
    CMD_NOTICE,
    CMD_QUIT,
    CMD_PING,
    CMD_PONG,
    CMD_COUNT
} Command;

typedef struct {
    char nick[IRC_NICK_LEN];
    char user[IRC_USER_LEN];
    char host[IRC_HOST_LEN];
    bool present;
} Prefix;

typedef struct {
    Prefix prefix;
    Command cmd;
    char params[IRC_MAX_PARAMS][IRC_PARAM_LEN];
    size_t params_count;
    char trailing[IRC_TRAILING_LEN];
    bool has_trailing;
} Message;


Command cmd_frm_string(const char* str) {
    if (strcmp(str, "NICK") == 0) return CMD_NICK;
    if (strcmp(str, "USER") == 0) return CMD_USER;
    if (strcmp(str, "JOIN") == 0) return CMD_JOIN;
    if (strcmp(str, "PART") == 0) return CMD_PART;
    if (strcmp(str, "PRIVMSG") == 0) return CMD_PRIVMSG;
    if (strcmp(str, "NOTICE") == 0) return CMD_NOTICE;
    if (strcmp(str, "QUIT") == 0) return CMD_QUIT;
    if (strcmp(str, "PING") == 0) return CMD_PING;
    if (strcmp(str, "PONG") == 0) return CMD_PONG;
    return CMD_UNKNOW;
}

void parse_message(const char *line, Message *msg) {
    memset(msg, 0, sizeof(*msg));

    const char* p = line;
    char token[IRC_TRAILING_LEN];

    //:alice!~user@host PRIVMSG #channel :hello world

    // <prefix> ::= <servername> | <nick> [ '!' <user> ] [ '@' <host> ]
    if (*p == ':') {
        p++;
        const char* space = strchr(p, ' ');
        if (!space) return;
        size_t len = space - p;
        if (len >= sizeof(token)) len = sizeof(token) - 1;
        strncpy(token, p, len);
        token[len] = 0;
        p = space + 1;

        char* ex = strchr(token, '!');
        char* at = strchr(token, '@');
        if (ex && at && at > ex) {
            *ex = 0;
            *at = 0;
            strncpy(msg->prefix.nick, token, IRC_NICK_LEN);
            strncpy(msg->prefix.user, ex + 1, IRC_USER_LEN);
            strncpy(msg->prefix.host, at + 1, IRC_HOST_LEN);
            msg->prefix.present = 1;
        }
    }

    // <command>  ::= <letter> { <letter> } | <number> <number> <number> (HOPE JUST LETTER)
    while (*p == ' ') p++;
    const char* space = strchr(p, ' ');
    size_t cmd_len;
    if (space) cmd_len = space - p;
    else cmd_len = strlen(p);

    if (cmd_len >= sizeof(token)) cmd_len = sizeof(token) - 1;

    strncpy(token, p, cmd_len);
    token[cmd_len] = 0;
    msg->cmd = cmd_frm_string(token);
    p += cmd_len;

    msg->params_count = 0;
    while (*p) {
        while (*p == ' ')p++;
        if (*p == '\0') break;
        if (*p == ':') {
            p++;
            strncpy(msg->trailing, p, IRC_TRAILING_LEN - 1);
            msg->has_trailing = 1;
            break;
        }

        const char* next_space = strchr(p, ' ');
        size_t len;
        if (next_space) len = next_space - p;
        else len = strlen(p);
        if (len >= IRC_PARAM_LEN) len = IRC_PARAM_LEN - 1;

        if (msg->params_count < IRC_MAX_PARAMS) {
            strncpy(msg->params[msg->params_count], p, len);
            msg->params[msg->params_count][len] = 0;
            msg->params_count++;
        }

        p += len;
    }
}

int conn_to_server(const char* ip, int port) {
    printf("[DEBUG] Connected to server at ip: %s and port : %d\n", ip, port);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) perror("server socket");

    struct sockaddr_in serv = {0};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv.sin_addr);

    if (connect(sfd, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("connect");
        exit(1);
    }

    printf("[DEBUG] Connected to server at ip: %s and port : %d\n", ip, port);
    return sfd;
}

int start_proxy_server(int port) {
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1) perror("socket");

    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    int b = bind(lfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(lfd, 5);

    printf("[DEBUG] Proxy listening on port: %d\n", port);
    return lfd;
}

int main() {
    int proxy_lfd = start_proxy_server(PORT_PROXY);
    int client_fd = accept(proxy_lfd, NULL, NULL);
    printf("[DEBUG] A client connected.\n");

    int server_fd = conn_to_server(IRC_SERVER, IRC_PORT);
    printf("[DEBUG] Connected to upstream IRC server.\n");

    char buf[BUFFER];
    fd_set fds;

    while (1) {
        FD_ZERO(&fds);
        FD_SET(client_fd, &fds);
        FD_SET(server_fd, &fds);
        int maxfd = (client_fd > server_fd ? client_fd : server_fd) + 1;
        if (select(maxfd, &fds, NULL, NULL, NULL) < 0) continue;

        if (FD_ISSET(client_fd, &fds)) {
            int n = recv(client_fd, buf, BUFFER - 1 , 0);
            if (n <= 0) break;
            buf[n] = 0;

            Message msg;
            parse_message(buf, &msg);

            printf("[INFO] From client: CMD=%d params=%zu trailing='%s'\n", msg.cmd, msg.params_count, msg.has_trailing ? msg.trailing : "");

            send(server_fd, buf, n, 0);
        }

        if (FD_ISSET(server_fd, &fds)) {
            int n = recv(server_fd, buf, BUFFER - 1, 0);
            if (n <= 0) break;
            buf[n] = 0;

            send(client_fd, buf, n, 0);
        }
    }

    close(client_fd);
    close(server_fd);
    close(proxy_lfd);
    return 0;
}
