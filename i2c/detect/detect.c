/* ------------------------------------------------------------------------- */
/* detect -- look who's there. Gets address acks from all devices on the bus.*/
/*		It should not change any values in the peripherals.	     */
/* ------------------------------------------------------------------------- */
static char *rcsid="$Id: detect.c,v 2.2 1996/11/17 22:03:06 i2c Exp $";

#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <linux/delay.h>
#include "i2c.h"

#define Write(a,b,c)	err(1,write(a,b,c))
#define Read(a,b,c)	err(0,read(a,b,c))

void err(int wr, int d){
	if (d==-1){
		printf("%s error: %s !\n",(wr)?"Write":"Read",strerror(errno));
	}
}
#define ERR(x) printf("return value: %d\n", (x) );

int main(int argc, char *argv[]){
	int i,f;
	char b[40],c;
	char *device = "/dev/i2c0";
	
	/* parse options */
	opterr=1;
	while ( (c=getopt(argc,argv,"hH?d:")) != -1) {
		switch (c){
			case 'd':
				if (optarg)
					device = optarg;
				break;
			case 'h':
			case 'H':
			case '?':
			default:
				printf("%s tries to detect devices on the i2c-bus. Usage:\n",argv[0]);
				printf("-d device device to use\n");
				return 0;
		}
	}
	f=open(device,O_RDWR);
	if (f<0){
		perror("detect");
		exit(1);
	}
	ERR( ioctl(f,I2C_UDELAY,10) );
	ERR( ioctl(f,I2C_MDELAY,0) );
//      ERR( ioctl(f,I2C_RETRIES,3) );
         	
	for (i=0;i<128;i++){
		ioctl(f,I2C_SLAVE,i);
		if (0>read (f,b,0) ) {
			putchar('.');
		} else {
			printf("\n%d worked! \n",i);
		}
		usleep(100);
		fflush(stdout);
	}
	putchar('\n');
	return close(f);
}
