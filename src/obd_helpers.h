#ifndef OBD_HELPERS_H
#define OBD_HELPERS_H

#define SET_LINE_BRK "atl1\r"
#define DEVICE_INFO "ati\r"
#define DEVICE_HARD_RESET "atz\r"
// #define PID_SPEED "010D\r"
#define PID_SPEED "010D1\r" /* add 1 on the end to the set maximum */
                            /* expected number of responses        */
                            /* and so reduce the answer time       */

#include <time.h>
#include <fcntl.h>   // File control definitions
#include <stdio.h>
#include <unistd.h>  // UNIX standard definitions 
#include <string.h>
#include <stdlib.h>
#include <termios.h> // POSIX Terminal control definitions
#include <inttypes.h>

void get_port(int *fd, char **device) {
    /*------------------------------- Opening the Serial Port ---------------------------------*/

    *fd = open(*device, O_RDWR | O_NOCTTY); /* O_RDWR   - Read/Write access to serial port     */
                                            /* O_NOCTTY - No terminal will control the process */
                                            /* Open in blocking mode, read will wait           */
    if(*fd < 0) {     
        fprintf(stderr, "\n  Error! Cannot open %s \n", *device);
        exit(1);
    } 
    printf("\n %s Opened Successfully! \n", *device);
}

void serial_setup(int *fd, size_t vmin, size_t vtime) {
    /*---------- Setting the Attributes of the serial port using termios structure ------------*/
    
    struct termios SerialPortSettings;

    tcgetattr(*fd, &SerialPortSettings); // Get the current attributes of the Serial port

    // Control options
    cfsetispeed(&SerialPortSettings, B38400);
    cfsetospeed(&SerialPortSettings, B38400);

    // 8N1 - no parity mode
    SerialPortSettings.c_cflag &= ~PARENB;     // Disables the Parity Enable bit(PARENB), So No Parity
    SerialPortSettings.c_cflag &= ~CSTOPB;     // CSTOPB = 2 Stop bits, here it is cleared so 1 Stop bit
    SerialPortSettings.c_cflag &= ~CSIZE;      // Clears the mask for setting the data size
    SerialPortSettings.c_cflag |=  CS8;        // Set the data bits = 8
    //
    SerialPortSettings.c_cflag &= ~CRTSCTS; // No Hardware flow Control
    SerialPortSettings.c_cflag |= CREAD;
    SerialPortSettings.c_cflag |= CLOCAL;
    SerialPortSettings.c_cflag &= ~HUPCL;
    // Setting blocking params
    SerialPortSettings.c_cc[VMIN] = vmin;   // character count 0-255
    SerialPortSettings.c_cc[VTIME] = vtime; // in deciseconds (0.1 sec.); if == 0 - Wait indefinetly

    // Line options
    SerialPortSettings.c_lflag |= ECHOE;
    SerialPortSettings.c_lflag &= ~(ICANON | ECHO | ISIG);
    // SerialPortSettings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  // Non Cannonical mode

    // Input options
    // SerialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY); // Disable XON/XOFF flow control both i/p and o/p
    SerialPortSettings.c_iflag |= IXON;
    SerialPortSettings.c_iflag &= ~(IXOFF | IXANY);
    SerialPortSettings.c_iflag |= IGNPAR;
    SerialPortSettings.c_iflag |= BRKINT;
    SerialPortSettings.c_iflag &= ~ICRNL;

    // Output options
    SerialPortSettings.c_oflag &= ~OPOST; // No Output Processing
    SerialPortSettings.c_oflag &= ~ONLCR;

    // Set the attributes to the termios structure
    if((tcsetattr(*fd, TCSANOW, &SerialPortSettings)) != 0) { 
        fprintf(stderr, "\n  ERROR ! in Setting attributes");
        exit(1);
    } else {
        printf("\n  Parameters set:\n    BaudRate = 38400 \n    StopBits = 1 \n    Parity = none \n    VMIN = %zu \n    VTIME = %zu \n", vmin, vtime);
    }
}

void slice_str(char *buffer, const char *str, size_t start, size_t end) {
    /*--------------- Writes a slice of the input string ---------------*/
    size_t j = 0;
    for ( size_t i = start; i < end; ++i ) {
        buffer[j++] = str[i];
    }
    buffer[j] = 0;
}

int safe_copy_buffer(char *buffer, const char *str, size_t start, size_t end) {
    /*--------------- Creates new char array with cleaned answer ---------------*/
    int j = 0;
    for ( int i=start; i < end; i++) {
        unsigned char ch = str[i];
        if ( !((ch == '\0') || (ch == '\r')) ) {
            buffer[j++] = str[i];
        }
    }
    buffer[j++] = '\0';
    return j;
}

size_t elm_talk(int *fd, char *buff, size_t buff_size, char *command, int delay) {
    /*---------- Send command to the device and wait for the answer ---------*/
    size_t bytes_read = 0;
    size_t command_len = strlen(command);

    // Discards old data in the rx buffer
    tcflush(*fd, TCIFLUSH); 

    // send control command to elm327
    write(*fd, command, sizeof(command));

    if ( delay ) {
        long int sleep = delay * 1000000L;
        usleep(sleep); // sleep time in us
    }

    // get answer and write to buff
    char answer[buff_size];
    bytes_read = read(*fd, answer, buff_size);
    bytes_read -= 3; // remove `>` prompt
    bytes_read = safe_copy_buffer(buff, answer, command_len, bytes_read);

    // // DEBUG
    // printf("\nInside elm read:\n %s\n %s \n", answer, buff);

    return bytes_read;
}

int answer_check(char *answer, char *cmp, size_t bytes_read) {
    size_t check_size = strlen(cmp);
    if ( bytes_read < check_size ) {
        return -1;
    }
    char check_str[check_size];
    slice_str(check_str, answer, 0, check_size);
    int check = strcmp(check_str, cmp);

    // // DEBUG
    // printf("\nIn comparison function: %zu; %d\n Answer:\n '%s' \n '%s' (%zu)\n '%s' (%zu)\n", 
    //        check_size, check, 
    //        answer, check_str, strlen(check_str), cmp, strlen(cmp));

    if (check != 0) {
        fprintf(stderr, "Elm bad response!");
        exit(1);
    }
    return check;
}

unsigned long get_time() {
    /*----------------- Returns current time in microseconds -----------------*/
    unsigned long us;
    time_t s;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    s = spec.tv_sec;
    us = (unsigned long)(spec.tv_nsec / 1000.0);
    if (us > 999999) {
        s++;
        us = 0;
    }
    us += s*1000000UL;
    return us;
}

int set_elm(int *fd, char *command, size_t buff_size) {
    /*----------------------- Set some elm property, like AT S0 -------------------------------*/
    char answer[buff_size];
    size_t bytes_read = elm_talk(fd, answer, buff_size, command, 1);
    int check = answer_check(answer, "OK", 2);
    return check;
}

int16_t get_vehicle_speed(char *answer) {
    /*---------- Converts last byte of answer; hex-->dec; speed range: 0...255 km\h  ----------*/
    char hexstring[2];
    size_t answer_size = strlen(answer);
    if ( answer_size <= 1 ) {
        return INT16_MIN;
    }
    size_t start = answer_size - 2;
    start = start >= 0 ? start : 0;
    slice_str(hexstring, answer, start, answer_size);
    int16_t speed = (int16_t)strtol(hexstring, NULL, 16);
    return speed;
}

#endif
