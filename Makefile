CFLAGS := -pthread

default: tftp tftpd

tftp_protoc.o: tftp_protoc.h tftp_protoc.c
tftp: tftp.c tftp_protoc.o
tftpd: tftpd.c tftp_protoc.o

clean:
	rm -f tftp tftpd tftp_protoc.o
