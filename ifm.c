#include <stdio.h>
#include <string.h>
#include "config.h"

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
    int present;
} Prefix;

typedef struct {
    Prefix prefix;
    Command cmd;
    char params[IRC_MAX_PARAMS][IRC_PARAM_LEN];
    int params_count;
    char trailing[IRC_TRAILING_LEN];
    int has_trailing;
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


