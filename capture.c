#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static unsigned int lepton_image[240][80];

void print_image()
                    {
			int sum = 0 ;
                        for(int i= 0 ; i<240 ; i++)
                        {
                            for(int j =0 ;j<80 ;j++)
                            {
                        	sum = sum+lepton_image[i ][j];  
			    // printf("%x," ,lepton_image[i][j]);
                            }
  //                          printf("\r\n");
                        }
//			printf("sum %d\r\n" , sum);
//                        printf("\r\n") ;
                        
                    }


static void pabort(const char *s)
{
    perror(s);
    abort();
}

static const char *device = "/dev/spidev0.0";
static uint8_t mode = SPI_CPOL | SPI_CPHA;
static uint8_t bits = 8;
static uint32_t speed = 20000000;//16000000;
static uint16_t delay =  65535;
static uint8_t status_bits = 0;

int8_t last_packet = -1;

#define VOSPI_FRAME_SIZE (164)
#define LEP_SPI_BUFFER (118080) //(118080)39360
/* modify /boot/cmdline.txt to include spidev.bufsiz=131072 */

static uint8_t rx_buf[LEP_SPI_BUFFER] = {0};
//static unsigned int lepton_image[240][80];

static void save_pgm_file(int image_index)
{
    int i;
    int j;
    unsigned int maxval = 0;
    unsigned int minval = UINT_MAX;
    char image_name[32];
//    int image_index = 0;

    do {
        sprintf(image_name, "./images/IMG_%.4d.pgm", image_index);
        image_index += 1;
        if (image_index > 9999) 
        {
            image_index = 0;
            break;
        }

    } while (access(image_name, F_OK) == 0);

    FILE *f = fopen(image_name, "w+");
    if (f == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }

    printf("Calculating min/max values for proper scaling...\n");
    for(i = 0; i < 240; i++)
    {
        
        for(j = 0; j < 80; j++)
        {
            if (lepton_image[i][j] > maxval) {
                maxval = lepton_image[i][j];
            }
            if (lepton_image[i][j] < minval) {
                minval = lepton_image[i][j];
            }
        }
    }
  //  printf("maxval = %u\n",maxval);
//    printf("minval = %u\n",minval);
    
    fprintf(f,"P2\n160 120\n%u\n",maxval-minval);
    for(i=0; i < 240; i += 2)
    {
        /* first 80 pixels in row */
        for(j = 0; j < 80; j++)
        {
            fprintf(f,"%d ", lepton_image[i][j] - minval);
        }

        /* second 80 pixels in row */
        for(j = 0; j < 80; j++)
        {
            fprintf(f,"%d ", lepton_image[i + 1][j] - minval);
        }        
        fprintf(f,"\n");
    }
    fprintf(f,"\n\n");

    fclose(f);

    //launch image viewer
    //execlp("gpicview", image_name, NULL);
}

int transfer(int fd)
{
    int ret;
    int i;
    int ip;
    uint8_t packet_number = 0;
    uint8_t segment = 0;
    uint8_t current_segment = 0;
    int packet = 0;
    int state = 0;  //set to 1 when a valid segment is found
    int pixel = 0;
    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)NULL,
        .rx_buf = (unsigned long)rx_buf,
        .len = LEP_SPI_BUFFER,
        .delay_usecs = delay,
        .speed_hz = speed,
        .bits_per_word = bits
    };    
    
    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1) {
        pabort("can't read spi data");
    }
    
    for(ip = 0; ip < (ret / VOSPI_FRAME_SIZE); ip++) {
        packet = ip * VOSPI_FRAME_SIZE;

        //check for invalid packet number
        if((rx_buf[packet] & 0x0f) == 0x0f) {
            state = 0;
            continue;
        }

        packet_number = rx_buf[packet + 1];
        
        if(packet_number > 0 && state == 0) {
            continue;
        }
        
        if(state == 1 && packet_number == 0) {
            state = 0;  //reset for new segment
        }
        
        //look for the start of a segment
        if(state == 0 && packet_number == 0 && (packet + (20 * VOSPI_FRAME_SIZE)) < ret) {
            segment = (rx_buf[packet + (20 * VOSPI_FRAME_SIZE)] & 0x70) >> 4;
            if(segment > 0 && segment < 5 && rx_buf[packet + (20 * VOSPI_FRAME_SIZE) + 1] == 20) {
                state = 1;
                current_segment = segment;
    //            printf("new segment: %x \n", segment);
            } 
        }
        
        if(!state) {
            continue;
        }

        for(i = 4; i < VOSPI_FRAME_SIZE; i+=2)
        {
            pixel = packet_number + ((current_segment - 1) * 60);
            lepton_image[pixel][(i - 4) / 2] = (rx_buf[packet + i] << 8 | rx_buf[packet + (i + 1)]);
        }
        
        if(packet_number == 59) {
            //set the segment status bit
            status_bits |= ( 0x01 << (current_segment - 1));
        }        
    }
    
    return status_bits;
}
 
int main(int argc, char *argv[])
{
    int ret = 0;
    int fd;
	int index = 1 ;

   struct timeval stop, start;
	gettimeofday(&start, NULL);
//    while(index <10){
    fd = open(device, O_RDWR);
    if (fd < 0)
    {
        pabort("can't open device");
    }

    ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if (ret == -1)
    {
        pabort("can't set spi mode");
    }

    ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
    if (ret == -1)
    {
        pabort("can't get spi mode");
    }

    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1)
    {
        pabort("can't set bits per word");
    }

    ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1)
    {
        pabort("can't get bits per word");
    }

    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1)
    {
        pabort("can't set max speed hz");
    }

    ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret == -1)
    {
        pabort("can't get max speed hz");
    }

    printf("spi mode: %d\n", mode);
    printf("bits per word: %d\n", bits);
    printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);
	//int index = 0 ;
   while(index < 10 ){
   //printf("capture\r\n");
    while(status_bits != 0x0f) { transfer(fd); }

	save_pgm_file(4) ;
//	sleep(8);
	printf("cap index %d\r\n" , index);
	//sleep(1);
	print_image() ;
	index++ ;
//	close(fd);
         status_bits = 0;
	}
	close(fd);
	gettimeofday(&stop, NULL);
printf("took %lu ms\n", ((stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec)/1000); 
    return ret;
}
