// Simple TFTP server implementation.
// Author: Sol Boucher <slb1566@rit.edu>

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Connection parameters:
static const in_port_t PORT = 1069;

// Protocol details:
static const char *const OPC_RRQ = "01";
static const char *const OPC_WRQ = "02";
static const char *const OPC_DAT = "03";
static const char *const OPC_ACK = "04";
static const char *const OPC_ERR = "05";
static const char *const MODE_ASCII = "netascii";
static const char *const MODE_OCTET = "octet";
static const char *const ERR_UNKNOWN      = "00";
static const char *const ERR_NOTFOUND     = "01";
static const char *const ERR_ACCESSDENIED = "02";
static const char *const ERR_DISKFULL     = "03";
static const char *const ERR_ILLEGALOPER  = "04";
static const char *const ERR_UNKNOWNTID   = "05";
static const char *const ERR_CLOBBER      = "06";
static const char *const ERR_UNKNOWNUSER  = "07";

static const int OFFSET_FILENAME = 2;
static const int OFFSET_TRSFMODE = 2;

typedef int bool;
enum protoc_state {WAIT, SEND, RECV, INVALID};
struct request
{
	char *message;
	size_t fname_len;
};

void req_init(struct request *, char *);
bool req_oftype(struct request *const, const char *const);
char *req_filename(struct request *);
char *req_mode(struct request *);
bool req_null(struct request *const);
bool req_del(struct request *);

static char *recvstr(int);
static char *recvstra(int, struct sockaddr_in *, socklen_t *);
static void handle_error(const char *);

int main(void)
{
	// Open UDP socket over IP:
	int socketfd;
	if((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) // TODO IPv6 support
		handle_error("socket()");

	// Bind to interface port:
	struct sockaddr_in saddr_local;
	saddr_local.sin_family = AF_INET;
	saddr_local.sin_port = htons(PORT); // TODO Try privileged port, then fall back
	saddr_local.sin_addr.s_addr = INADDR_ANY;
	if(bind(socketfd, (struct sockaddr *)&saddr_local, sizeof saddr_local))
		handle_error("bind()");

	enum protoc_state state = WAIT;
	struct sockaddr_in saddr_remote;
	socklen_t sktaddrmt_len = sizeof saddr_remote;
	struct request sess_hder;

	while(1)
	{
		if(state == WAIT)
		{
			if(!req_null(&sess_hder))
				req_del(&sess_hder);
			req_init(&sess_hder, recvstra(socketfd, &saddr_remote, &sktaddrmt_len));

			printf("filename: %s\n", req_filename(&sess_hder));
			printf("xfermode: %s\n", req_mode(&sess_hder));

			if(req_oftype(&sess_hder, OPC_RRQ))
				state = SEND;
			else if(req_oftype(&sess_hder, OPC_WRQ))
				state = RECV;
			else
				state = INVALID;
		}
		else if(state != INVALID)
		{
			printf("Entering send/receive mode\n");
			break;
		}
		else
		{
			fprintf(stderr, "Attempted invalid state transition\n");
			break;
		}
	}

	if(!req_null(&sess_hder))
		req_del(&sess_hder);

	return 0;
}

char *recvstr(int sfd)
{
	return recvstra(sfd, NULL, 0);
}

char *recvstra(int sfd, struct sockaddr_in *rmt_saddr, socklen_t *rsaddr_len)
{
	// Listen for an incoming message and note its length:
	ssize_t msg_len;
	if((msg_len = recvfrom(sfd, NULL, 0, MSG_TRUNC|MSG_PEEK, (struct sockaddr *)rmt_saddr, rsaddr_len)) < 0) // TODO Handle shutdown
		handle_error("recvfrom()");

	if(msg_len == 0) // Client closed connection
		return NULL;
	++msg_len; // Increment length to leave room for a NULL-terminator

	// Read the message:
	char *msg = malloc(msg_len);
	memset(msg, 0, msg_len);
	if(recv(sfd, msg, msg_len, 0) <= 0) // TODO Handle multiple clients
		handle_error("recv()");
	return msg;
}

void req_init(struct request *req, char *msg)
{
	req->message = msg;
	req->fname_len = strlen(req_filename(req));

	char *loc = req_mode(req);
	while(*loc != '\0')
	{
		*loc = tolower(*loc);
		++loc;
	}
}

bool req_oftype(struct request *const req, const char *const typeconst)
{
	return strncmp(req->message, typeconst, 2) == 0;
}

char *req_filename(struct request *req)
{
	return req->message+OFFSET_FILENAME;
}

char *req_mode(struct request *req)
{
	return req_filename(req)+req->fname_len+OFFSET_TRSFMODE;
}

bool req_null(struct request *const req)
{
	return !req->message;
}

bool req_del(struct request *req)
{
	if(req_null(req))
	{
		free(req->message);
		req->message = NULL;
		req->fname_len = 0;
		return 1;
	}
	return 0;
}

void handle_error(const char *desc)
{
	perror(desc);
	exit(errno);
}
