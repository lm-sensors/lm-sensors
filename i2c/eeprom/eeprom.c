/* ------------------------------------------------------------------------- */
/* eeprom -- read eeprom contents					     */
/* 	revamped by Gerd Knorr						     */
/* ------------------------------------------------------------------------- */
static char *rcsid="$Id: eeprom.c,v 1.1 1996/07/23 20:32:12 i2c Exp $";
/*
 * $Log: eeprom.c,v $
 * Revision 1.1  1996/07/23 20:32:12  i2c
 * Initial revision
 *
 * Revision 1.2  1996/07/06 17:20:38  i2c
 * rewrote loop
 *
 * Revision 1.1  1996/03/15 08:55:53  root
 * Initial revision
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#include "i2c.h"

char *dev="/dev/i2c0";
int  wr=0, adr=0xa0;



void usage(char *name)
{
    fprintf(stderr,"This is a i2c EEPROM tool\n");
    fprintf(stderr,"  read  data: %s [ options ] > file\n",name);
    fprintf(stderr,"  write data: %s [ options ] -w < file\n",name);
    fprintf(stderr,"\n");
    fprintf(stderr,"File format is a hex dump.  Without \"-w\" switch\n");
    fprintf(stderr,"only parsing (and printing) of the data is done,\n");
    fprintf(stderr,"writing is skipped.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"options:\n");
    fprintf(stderr,"  -d device  device to use         [%s]\n",dev);
    fprintf(stderr,"  -a addr    set slave address     [0x%02x]\n",adr);
    fprintf(stderr,"\n");
    exit(1);
}

void dump_buf(unsigned char *buf)
{
    int i,j;
    
    for (i = 0; i < 256; i += 16) {
        printf("%04x  ",i);
        for (j = i; j < i+16; j++) {
            if (!(j%4))
                printf(" ");
            printf("%02x ",buf[j]);
        }
        printf("  ");
        for (j = i; j < i+16; j++)
            printf("%c",isalnum(buf[j]) ? buf[j] : '.');
        printf("\n");
    }
}

int parse_buf(unsigned char *buf)
{
    int  i,j,n,pos,count;
    char line[100];
    
    for (i = 0; i < 256; i += 16) {
        if (NULL == fgets(line,99,stdin)) {
            fprintf(stderr,"unexpected EOF\n");
            return -1;
        }
        if (1 != sscanf(line,"%x%n",&n,&pos)) {
            fprintf(stderr,"addr parse error (%d)\n",i>>4);
            return -1;
        }
        if (n != i) {
            fprintf(stderr,"addr mismatch\n");
            return -1;
        }
        for (j = i; j < i+16; j++) {
            if (1 != sscanf(line+pos,"%x%n",&n,&count)) {
                fprintf(stderr,"value parse error\n");
                return -1;
            }
            buf[j] = n;
            pos += count;
        }
    }
    return 0;
}

int
read_buf(int fd, unsigned char *buf)
{
    int i,n=8; 
    unsigned char addr;
    
    for (i = 0; i < 256; i += n) {
        addr = i;
        if (-1 == (write(fd,&addr,1))) {
            fprintf(stderr,"write addr %s: %s\n",dev,strerror(errno));
            exit(1);
        }
        if (-1 == (read(fd,buf+i,n))) {
            fprintf(stderr,"read data %s: %s\n",dev,strerror(errno));
            exit(1);
        }
    }
}
int
write_buf(int fd, unsigned char *buf)
{
    int i,j,n = 8; 
    unsigned char tmp[17];
    
    for (i = 0; i < 256; i += n) {
        tmp[0] = i;
        for (j = 0; j < n; j++)
            tmp[j+1] = buf[i+j];
        if (-1 == (write(fd,tmp,n+1))) {
            fprintf(stderr," write data %s: %s\n",dev,strerror(errno));
            exit(1);
        }
        fprintf(stderr,"*");
        usleep(100000); /* 0.1 sec */
    }
}

int main(int argc, char *argv[])
{
    int            f,c,rescue;
    unsigned char  buf[256];
    
    /* parse options */
    opterr=1;
    while ( (c=getopt(argc,argv,"hrwa:d:")) != -1) {
        switch (c){
	case 'r':
		rescue=1;
		break;
        case 'w':
            wr=1;
            break;
        case 'd':
            dev = optarg;
            break;
        case  'a':
            adr = strtol(optarg,NULL,0);
            break;
        case 'h':
        default:
            usage(argv[0]);
        }
    }

    if (-1 == (f = open(dev,O_RDWR))) {
        fprintf(stderr,"open %s: %s\n",dev,strerror(errno));
        exit(1);
    }
    ioctl(f,I2C_SLAVE,adr>>1);
    memset(buf,0,256);

    if (isatty(fileno(stdin))) {
        /* read */
        read_buf(f,buf);
        dump_buf(buf);
    } else {
        /* write */
        if (-1 == parse_buf(buf))
            exit(1);
        dump_buf(buf);
        if (wr) {
            fprintf(stderr,"writing to eeprom now... ");
            write_buf(f,buf);
            fprintf(stderr," ok\n");
        }
    }
    
    close(f);
    exit(0);
}
