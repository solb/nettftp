CFLAGS := -pthread

default: tftp tftpd
debug:
	$(MAKE) --no-print-directory CFLAGS="${CFLAGS} -DDEBUG -ggdb" clean default

tftp_protoc.o: tftp_protoc.c tftp_protoc.h
tftp: tftp.c tftp_protoc.o
tftpd: tftpd.c tftp_protoc.o

clean:
	rm -f tftp tftpd tftp_protoc.o
