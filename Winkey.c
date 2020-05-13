#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>

/*
 * This file provides the functions
 *
 * void close_winkey_port()
 * int open_winkey_port(const char *path)
 * void send_winkey(const char *msg)
 * void set_winkey_speed(int speed)
 *
 * If path == "uh-wkey" in open_winkey_port, a microHam device is used
 * rather than a serial port
 *
 */


/*
 * These functions are now provided by hamlib in src/uhnew.c
 */
extern int  uh_open_wkey();
extern int  uh_close_wkey();

static int wkey_fd;
static int uh_fd_wkey=-10;

static int wkey_version=0;

static void writeWkey(const unsigned char *buf, size_t len)
{
    write(wkey_fd, buf, len);
}

static int readWkey(unsigned char *buf, size_t len)
{
    return read(wkey_fd, buf, len);
}

// timeout is in milli-secs here
static int selectWkey(int timeout)
{
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(wkey_fd, &fds);
    tv.tv_sec=timeout/1000;
    tv.tv_usec=1000 * (timeout - 1000*tv.tv_sec);
    return select(wkey_fd+1, &fds, NULL, NULL, &tv);
}

// close serial port
void close_winkey_port()
{
    unsigned char buf[2];

    if (wkey_fd < 0) return;
    // Clear buffer
    buf[0]=0x0A;
    writeWkey(buf, 1);
    // Host close command
    buf[0]=0x00;
    buf[1]=0x03;
    writeWkey(buf, 2);

    if (wkey_fd == uh_fd_wkey) uh_close_wkey();  else close(wkey_fd);
    wkey_fd = -1;
    wkey_version=0;
}
    
// open serial port
int open_winkey_port(const char *path)
{
    int fd;
    struct termios tty;
    unsigned char buf[10];
    int ret;
    struct timeval tv;
    fd_set fds;

   
    if (!strcmp(path,"uh-wkey")) {
	// use microHam router
	fd=uh_open_wkey();
	if (fd < 0) return 0;
	uh_fd_wkey=fd;
    } else {
	// use serial port
	fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) return 0;

	tcflush(fd, TCIFLUSH);
	tcgetattr(fd, &tty);
	tty.c_cflag &= ~CSIZE; tty.c_cflag |= CS8;		// 8 data bits
	tty.c_cflag |= CSTOPB;				// 2 stop bits
	tty.c_cflag &= ~CRTSCTS;				// no hardware handshake
	tty.c_cflag &= ~PARENB;				// parity
	tty.c_cflag |= (CLOCAL | CREAD);			// enable receiver
	tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);	// raw input
	tty.c_oflag &= ~OPOST;				// raw output
	tty.c_iflag &= ~IXON;				// no software handshake
	tty.c_iflag &= ~ICRNL;				// no input CR/NL translation

	cfsetispeed(&tty, B1200);				// input  speed 1200 baud
	cfsetospeed(&tty, B1200);				// output speed 1200 baud

	tcsetattr (fd, TCSANOW, &tty);			// set parameters

    }
    wkey_fd=fd;
    while (readWkey(buf, 1) >0);
	
    // init host mode
    buf[0]=0;
    buf[1]=2;
    writeWkey(buf, 2);
    // test for returned value, wait up to 1000 milli-secs
    ret=selectWkey(1000);
    if (ret != 1) {
	fprintf(stderr,"WinKey did not respond within 1 sec\n");
	close_winkey_port();
	return 0;
    }

    buf[0]=0;
    readWkey(buf, 1);
    fprintf(stderr,"WinKey version number: %d\n", (int) buf[0]);
    wkey_version=buf[0];
    //
    // disable pushbutton reporting, so we always get the status byte
    // so we do not have to distinguish between WK1 and WK2
    //
    buf[0]=0;
    buf[1]=10;
    writeWkey(buf, 2);
    //
    // Initial speed: 20 wpm
    //
    buf[0]=0x02;
    buf[1]=20;
    writeWkey(buf, 2);
    //
    // Standard weighting
    //
    buf[0]=0x03;
    buf[1]=50;
    writeWkey(buf, 2);
    //
    // set PTT lead-in and lead-out
    //
    // In many situations, you will need a much smaller lead-in
    // This one is for SDRs with a "slow" PA connected
    //
    // The lead-out is fixed to 500 msec but with scale inversely
    // with the speed
    //
    buf[0]=4;
    buf[1]=10;    // 100 msec lead-in
    buf[2]=40;    // 500 msec lead-out (for 20 wpm)
    writeWkey(buf, 3);
    //
    // set SpeedPot range: 5..30 WPM
    //
    buf[0]=5;
    buf[1]=5;
    buf[2]=25;
    buf[3]=0;
    writeWkey(buf, 3);
    //
    // clear buffer
    //
    buf[0]=0x0A;
    writeWkey(buf, 1);
    //
    // turn off Farnsworth
    //
    buf[0]=0x0D;
    buf[1]=0;
    writeWkey(buf, 2);
    return 1;
}

void send_winkey(const char *msg)
{
    int i;
    size_t len = strlen(msg);
    struct timeval tv;
    unsigned char buf[1];
    int ready=0;

    // Wait if buffer is more than 2/3 full
    for (;;) {
	// Request status byte
	buf[0]=0x15;
        writeWkey(buf, 1);
	// read winkey, if it is a status byte, look for XON bit
        if (selectWkey(100) > 0) {
            while(readWkey(buf, 1) > 0) {
		if ((buf[0] & 0xC1) == 0xC0) ready=1;
	    }
        }
        if (ready) {
	    writeWkey((const unsigned char *)msg, len);
	    return;
	}
    }
}

//
// Speed == 0 means "turn to speed pot"
//
void set_winkey_speed(int speed)
{
    unsigned char buf[3];
    if (speed > 50) speed=50;
    if (speed < 5 && speed != 0)  speed=5;
    // change speed
    buf[0]=0x02;
    buf[1]=speed;
    writeWkey(buf, 2);
//
//  adjust the lead-out time to the CW speed
//  take 500 msec for 20 wpm and scale accordingly
//  (e.g. 10 wpm gives 1000 msec lead-out and 40 wpm gives 250 msec lead-out)
//
//  This avoids "relay chatter" when switching to qrs
//
    if (speed >= 5) {
      buf[0]=4;
      buf[1]=10;           // 100 msec lead-in
      buf[2]=1000/speed;   // lead-out adjusted to speed
    }
}
