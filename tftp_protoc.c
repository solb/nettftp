// Simple TFTP protocol library implementation.
// Author: Sol Boucher <slb1566@rit.edu>

#include "tftp_protoc.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

const in_port_t PORT_PRIVILEGED = 69;
const in_port_t PORT_UNPRIVILEGED = 1069;
const size_t DATA_LEN = 512;

const uint16_t OPC_RRQ = 1;
const uint16_t OPC_WRQ = 2;
const uint16_t OPC_DAT = 3;
const uint16_t OPC_ACK = 4;
const uint16_t OPC_ERR = 5;
const char *const MODE_ASCII = "netascii";
const char *const MODE_OCTET = "octet";
const uint16_t ERR_UNKNOWN = 0;
const uint16_t ERR_NOTFOUND = 1;
const uint16_t ERR_ACCESSDENIED = 2;
const uint16_t ERR_DISKFULL = 3;
const uint16_t ERR_ILLEGALOPER = 4;
const uint16_t ERR_UNKNOWNTID = 5;
const uint16_t ERR_CLOBBER = 6;
const uint16_t ERR_UNKNOWNUSER = 7;

// Opens a UDP socket and binds it to the specified port.
// Accepts: port number or 0 to choose an arbitrary ephemeral port
// Returns: file descriptor or -1 in case of an error
int openudp(uint16_t port)
{
	// Open UDP socket over IP:
	int socketfd;
	if((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) // TODO IPv6 support
		handle_error("socket()");

	// Bind to interface port:
	struct sockaddr_in saddr_local;
	saddr_local.sin_family = AF_INET;
	saddr_local.sin_port = htons(port);
	saddr_local.sin_addr.s_addr = INADDR_ANY;
	if(bind(socketfd, (struct sockaddr *)&saddr_local, sizeof saddr_local))
		return -1;

	return socketfd;
}

// Listens for a datagram arriving on the specified socket.
// Accepts: socket file descriptor
// Returns: caller-owned buffer
void *recvpkt(int sfd)
{
	return recvpkta(sfd, NULL);
}

// Listens on socket for incoming datagram and reveals its source address.
// Accepts: file descriptor, pointer to socket address structure or NULL
// Returns: caller-owned buffer
void *recvpkta(int sfd, struct sockaddr_in *rmt_saddr)
{
	recvpktal(sfd, NULL, rmt_saddr);
}

// Listens on socket for incoming datagram and reveals its length and source address.
// Accepts: file descriptor, pointer to length or NULL, pointer to address or NULL
// Returns: caller-owned buffer
void *recvpktal(int sfd, size_t *len_out, struct sockaddr_in *rmt_saddr)
{
	ssize_t msg_len;
	socklen_t rsaddr_len = sizeof(struct sockaddr_in);

	// Listen for an incoming message and note its length:
	if((msg_len = recvfrom(sfd, NULL, 0, MSG_TRUNC|MSG_PEEK, (struct sockaddr *)rmt_saddr, rmt_saddr ? &rsaddr_len : NULL)) < 0)
		handle_error("recvfrom()");

	// Check whether remote terminated the connection:
	if(msg_len == 0)
		return NULL;

	// Read the message:
	void *msg = malloc(msg_len);
	if(recv(sfd, msg, msg_len, 0) <= 0)
		handle_error("recv()");

	if(len_out)
		*len_out = msg_len;
	return msg;
}

// Sends a file over a network socket.
// Accepts: socket file descriptor, file descriptor, pointer to destination address
void sendfile(int sfd, int fd, struct sockaddr_in *dest)
{
	uint16_t *buf = malloc(4+DATA_LEN);
	buf[0] = OPC_DAT;
	buf[1] = 0; // Block ID
	int len = DATA_LEN;

	for(buf[1] = 0; len == DATA_LEN; ++buf[1])
	{
		len = read(fd, buf+2, DATA_LEN);
		sendto(sfd, buf, 4+len, 0, (struct sockaddr *)dest, sizeof(struct sockaddr_in));

		// Make sure something (hopefully an ACK) arrives:
		uint16_t *resp = recvpkt(sfd);
		free(resp);
	}
}

// Receives a file over a network socket.
// Accepts: socket file descriptor, file descriptor
// Returns: NULL or a human-readable error message
const char *recvfile(int sfd, int fd)
{
	size_t msg_len;
	struct sockaddr_in rsa;

	do
	{
		uint16_t *inc = recvpktal(sfd, &msg_len, &rsa);

		if(iserr(inc))
		{
			const char *desc = strerr(inc);
			free(inc);
			return desc;
		}

		if(msg_len > 4)
			write(fd, inc+2, msg_len-4);
		sendack(sfd, inc[1], &rsa);

		free(inc);
	}
	while(msg_len == 4+DATA_LEN);

	return NULL;
}

// Acknowledges receipt of a block of the caller's choice.
// Accepts: socket file descriptor, block number, pointer to destination
void sendack(int sfd, uint16_t blknum, struct sockaddr_in *dest)
{
	uint16_t ack[2];
	ack[0] = OPC_ACK;
	ack[1] = blknum;
	sendto(sfd, ack, sizeof ack, 0, (struct sockaddr *)dest, sizeof(struct sockaddr_in));
}

// Sends an error datagram appropriate for the value of the errno variable.
// Accepts: socket file descriptor, destination address pointer
void diagerrno(int sfd, struct sockaddr_in *dest)
{
	switch(errno)
	{
		case ENOENT:
			senderr(sfd, ERR_NOTFOUND, dest);
			break;
		case EACCES:
		case EROFS:
			senderr(sfd, ERR_ACCESSDENIED, dest);
			break;
		case EFBIG:
		case ENOSPC:
			senderr(sfd, ERR_DISKFULL, dest);
			break;
		case EEXIST:
			senderr(sfd, ERR_CLOBBER, dest);
			break;
		default:
			senderr(sfd, ERR_UNKNOWN, dest);
	}
}

// Transmit the specified error code accross the network.
// Accepts: socket file descriptor, error code, pointer to destination
void senderr(int sfd, uint16_t ercode, struct sockaddr_in *dest)
{
	uint8_t err[5];
	*(uint16_t *)err = OPC_ERR;
	*(uint16_t *)(err+2) = ercode;
	err[4] = 0;
	sendto(sfd, err, sizeof err, 0, (struct sockaddr *)dest, sizeof(struct sockaddr_in));
}

// Determines whether the given datagram is an error response.
// Accepts: the packet
// Returns: the answer
int iserr(void *payload)
{
	return *(uint16_t *)payload == OPC_ERR;
}

// Converts an error datagram to a human-readable complaint.
// Accepts: the packet
// Returns: the demystification
const char *strerr(void *payload)
{
	uint16_t code = ((uint16_t *)payload)[1];
	if(code == ERR_UNKNOWN)
		return "Unknown error";
	else if(code == ERR_NOTFOUND)
		return "File not found";
	else if(code == ERR_ACCESSDENIED)
		return "Access denied";
	else if(code == ERR_DISKFULL)
		return "Disk full";
	else if(code == ERR_ILLEGALOPER)
		return "Illegal operation";
	else if(code == ERR_UNKNOWNTID)
		return "Unrecognized transfer";
	else if(code == ERR_CLOBBER)
		return "File already exists";
	else if(code == ERR_UNKNOWNUSER)
		return "Unknown user";
	else
		return "Inexcusable error";
}

// Bails out of the program, printing an error based on the given context and errno.
// Accepts: the context of the problem
void handle_error(const char *desc)
{
	int errcode = errno;
	perror(desc);
	exit(errcode);
}
