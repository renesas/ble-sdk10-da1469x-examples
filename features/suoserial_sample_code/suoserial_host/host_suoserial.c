/**
 ****************************************************************************************
 *
 * @file host_usb_updater.c
 *
 * @brief Host tool for SUOUART
 *
 * Copyright (C) 2016-2022 Renesas Electronics Corporation and/or its affiliates
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 ****************************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define HOST_USB_UPDATER_VERSION 2
//*************
//** Windows **
//*************
//
// Build with:
//      "C:\Program Files (x86)\Dialog Semiconductor\SmartSnippets\CDT\Other\MinGW\bin\gcc.exe" -o host_usb host_usb_updater.c
// or:
//      C:\DiaSemi\SmartSnippetsStudio\Tools\mingw64_targeting32\bin\gcc.exe -o host_usb host_usb_updater.c
//
// Note calling .exe file "host_usb_updater.exe" is forcing admin privileges for the file so using "host_usb.exe" instead
//
// Run example:
//      host_usb.exe 24 <path_to_img>/pxp_reporter.1.0.0.1.img -verbose
//
//***********
//** Linux **
//***********
//
// Build with: "gcc -o host_usb_updater host_usb_updater.c"
//
// Run example:
//      sudo ./host_usb_updater.exe /dev/ttyACM0 ../../../../../projects/dk_apps/demos/pxp_reporter/Release_QSPI_SUOUSB/pxp_reporter.1.0.0.1.img -verbose
//

//These are just to make the view in my editor accurate :-)
//#define _WIN32
//#define __linux__       1

//Bring in changes from MTP tester - delay
//This is the delay after open, which is arguably only required for Windows.
//The delay is for cases where data is lost after open due to apparent failure of usbser.sys to
//respect DSR line from target, so early host TX was lost by target.
//Status: Enabled again, but only for Windows
//        Was disabled (8bc0c1e7), presumably for Linux, but not tested on Windows?
//        I suspect different host may not show issue, or only the MTP tester
//        is thorough enough.
#ifdef _WIN32
#define HOST_USB_UPDATER_SERIAL_PORT_MODS2
#endif

//replicate output to a logfile 'host_usb_updater.log'
//#define HOST_USB_UPDATER_LOG
bool isVerbose = false;

#ifdef _WIN32

#include <windows.h>
#include <winbase.h>
#include <conio.h> /* for getch(), non-portable */

HANDLE hComm;
DWORD read_byte(char *c);

#ifdef HOST_USB_UPDATER_LOG
FILE *verbose_output = NULL;
char buf[256 + 1];
void printf_log(const char *fmt, ...)
{
        int len;
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf) + 1, fmt, ap);
        va_end(ap);

        //Adjust to CRLF for ease in Windows
        len = strlen(buf);
        if (buf[len-1]=='\n') {
                buf[len-1] = '\r';
                buf[len] = '\n';
                buf[len+1] = 0;
        }

        printf("%s", buf);
        if (verbose_output != NULL)
        fprintf(verbose_output, "%s", buf);
}
#define printf_err printf_log
#define printf_verbose(format, args...) \
        do { \
                if (isVerbose) { \
                        printf_log(format,##args); \
                } \
        } while(0)
#else
#define printf_err printf
#define printf_verbose(format, args...) \
        do { \
                if (isVerbose) { \
                        printf(format,##args); \
                } \
        } while(0)
#endif

HANDLE open_serial_port(char *port)
{
        HANDLE hCommTmp;
        char comportstr[16];
        int comportnum;
        comportnum = atoi(port);
        sprintf(comportstr, "\\\\.\\COM%d", comportnum);

        return CreateFile(comportstr,                  //port name
                GENERIC_READ | GENERIC_WRITE,   //Read/Write
                0,                              //No Sharing
                NULL,                           //No Security
                OPEN_EXISTING,                  //Open existing port only
                0,                              //Non Overlapped I/O
                NULL);                          //Null for Comm Devices
}

void close_serial_port(HANDLE hComm)
{
        CloseHandle(hComm);
}

int configure_serial(void)
{
        //https://msdn.microsoft.com/en-us/library/ms810467.aspx

        DCB dcb = { 0 }; // Initializing DCB structure
        dcb.DCBlength = sizeof(dcb);

        if (!GetCommState(hComm, &dcb)) {
                printf_err("HOST=[Error when getting serial parameters]\n");
                return -1;
        }

        //line set-up
        dcb.BaudRate = CBR_115200;
        dcb.ByteSize = 8;
        dcb.StopBits = ONESTOPBIT;
        dcb.Parity = NOPARITY;

        //flow control
        dcb.fDtrControl = DTR_CONTROL_DISABLE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fRtsControl = RTS_CONTROL_DISABLE;
        dcb.fOutxCtsFlow = FALSE;    //doesn't exist on CDC spec (uses NACK?), so what's to monitor?

        //etc
        dcb.fBinary = TRUE;
        dcb.fDsrSensitivity = FALSE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
        dcb.fErrorChar = FALSE;
        dcb.fNull = FALSE;
        dcb.fAbortOnError = FALSE;
        dcb.wReserved = 0;
        dcb.XonLim = 1;

        if (!SetCommState(hComm, &dcb)) {
                printf_err("HOST=[Error when setting serial parameters]\n");
                return -1;
        }

        return 0;
}

int set_serial_timeouts(void)
{
        COMMTIMEOUTS timeouts = { 0 };  //all in millisecs

        //Specifies the maximum time interval between arrival of two bytes.
        //If the arrival time exceeds these limits the ReadFile() function returns.
        timeouts.ReadIntervalTimeout = 50;

        //Used to calculate the total time-out period for read operations.
        //For each read operation, this value is added to the product of the
        //ReadTotalTimeoutMultiplier member and the requested number of bytes.
        timeouts.ReadTotalTimeoutConstant = 50;

        //Used to calculate the total time-out period for read operations.
        //For each read operation, this value is multiplied by the requested number of bytes to be read.
        timeouts.ReadTotalTimeoutMultiplier = 10;

        //Similar to ReadTotalTimeoutConstant but for write operation.
        timeouts.WriteTotalTimeoutConstant = 50;

        //Similar to ReadTotalTimeoutMultiplier but for write operation.
        timeouts.WriteTotalTimeoutMultiplier = 10;

        SetCommTimeouts(hComm, &timeouts);
        if (!SetCommTimeouts(hComm, &timeouts)) {
                printf_err("HOST=[Error setting timeout parameters]\n");
                return -1;
        }
        return 0;
}

void diagnose_serial_state(void)
{
        DCB dcb;
        COMSTAT comStat;
        DWORD dwErrors;
        DWORD dwModemStatus;

        if (GetLastError() == ERROR_IO_PENDING) {
                printf_verbose("HOST=[GetLastError()==ERROR_IO_PENDING]\n");
        } else {
                printf_verbose("HOST=[GetLastError()==%d]\n", GetLastError());
        }

        FillMemory(&dcb, sizeof(dcb), 0);
        if (!GetCommState(hComm, &dcb)) {    // get current DCB
                printf_verbose("HOST=[Error from GetCommState]\n");
        } else {
                printf_verbose("HOST=[GetCommState.fOutxCtsFlow = %d]\n",
                        (dcb.fOutxCtsFlow == TRUE) ? 1 : 0);
                printf_verbose("HOST=[GetCommState.fOutxDsrFlow = %d]\n",
                        (dcb.fOutxDsrFlow == TRUE) ? 1 : 0);
                printf_verbose("HOST=[GetCommState.fOutX        = %d]\n", (dcb.fOutX == TRUE) ? 1 : 0);
        }

        if (!ClearCommError(hComm, &dwErrors, &comStat)) {
                printf_verbose("HOST=[Error from ClearCommError]\n");
        } else {
                if (dwErrors & CE_DNS) {
                        printf_verbose("HOST=[ClearCommError.CE_DNS      = true]\n");
                }
                if (dwErrors & CE_IOE) {
                        printf_verbose("HOST=[ClearCommError.CE_IOE      = true]\n");
                }
                if (dwErrors & CE_OOP) {
                        printf_verbose("HOST=[ClearCommError.CE_OOP      = true]\n");
                }
                if (dwErrors & CE_PTO) {
                        printf_verbose("HOST=[ClearCommError.CE_PTO      = true]\n");
                }
                if (dwErrors & CE_MODE) {
                        printf_verbose("HOST=[ClearCommError.CE_MODE     = true]\n");
                }
                if (dwErrors & CE_BREAK) {
                        printf_verbose("HOST=[ClearCommError.CE_BREAK    = true]\n");
                }
                if (dwErrors & CE_FRAME) {
                        printf_verbose("HOST=[ClearCommError.CE_FRAME    = true]\n");
                }
                if (dwErrors & CE_RXOVER) {
                        printf_verbose("HOST=[ClearCommError.CE_RXOVER   = true]\n");
                }
                if (dwErrors & CE_TXFULL) {
                        printf_verbose("HOST=[ClearCommError.CE_TXFULL   = true]\n");
                }
                if (dwErrors & CE_OVERRUN) {
                        printf_verbose("HOST=[ClearCommError.CE_OVERRUN  = true]\n");
                }
                if (dwErrors & CE_RXPARITY) {
                        printf_verbose("HOST=[ClearCommError.CE_RXPARITY = true]\n");
                }
                if (comStat.fCtsHold) {
                        printf_verbose("HOST=[comStat.fCtsHold           = true]\n");
                }
                if (comStat.fDsrHold) {
                        printf_verbose("HOST=[comStat.fDsrHold           = true]\n");
                }
                if (comStat.fRlsdHold) {
                        printf_verbose("HOST=[comStat.fRlsdHold          = true]\n");
                }
                if (comStat.fXoffHold) {
                        printf_verbose("HOST=[comStat.fXoffHold          = true]\n");
                }
                if (comStat.fXoffSent) {
                        printf_verbose("HOST=[comStat.fXoffSent          = true]\n");
                }
                if (comStat.fEof) {
                        printf_verbose("HOST=[comStat.fEof               = true]\n");
                }
                if (comStat.fTxim) {
                        printf_verbose("HOST=[comStat.fTxim              = true]\n");
                }
                if (comStat.cbInQue) {
                        printf_verbose("HOST=[comStat.cbInQue            = true]\n");
                }
                if (comStat.cbOutQue) {
                        printf_verbose("HOST=[comStat.cbOutQue           = true]\n");
                }

                if (comStat.cbInQue) {
                        char c;

                        while (read_byte(&c)) {
                                printf_verbose("HOST=[comStat.cbInQue = true, so flushing - [%02x]]\n", c);
                        }
                }
        }

        if (!GetCommModemStatus(hComm, &dwModemStatus)) {
                printf_verbose("HOST=[Error from GetCommModemStatus]\n");
        }
        else {
                if (dwModemStatus & MS_CTS_ON) {
                        printf_verbose("HOST=[GetCommModemStatus.MS_CTS_ON      = true]\n");
                }
                if (dwModemStatus & MS_DSR_ON) {
                        printf_verbose("HOST=[GetCommModemStatus.MS_DSR_ON      = true]\n");
                }
                if (dwModemStatus & MS_RING_ON) {
                        printf_verbose("HOST=[GetCommModemStatus.MS_RING_ON     = true]\n");
                }
                if (dwModemStatus & MS_RLSD_ON) {
                        printf_verbose("HOST=[GetCommModemStatus.MS_RLSD_ON     = true]\n");
                }
        }
}

#elif __linux__ || __APPLE__

#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

int hComm;
#define INVALID_HANDLE_VALUE -1
typedef unsigned int DWORD;
#define Sleep(x)        usleep(x*1000)

DWORD read_byte(char *c);

#ifdef HOST_USB_UPDATER_LOG
FILE *verbose_output = NULL;
char buf[256 + 1];
void printf_log(const char *fmt, ...)
{
        int len;
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf) + 1, fmt, ap);
        va_end(ap);

        //Adjust to CRLF for ease in Windows
        len = strlen(buf);
        if (buf[len-1]=='\n') {
                buf[len-1] = '\r';
                buf[len] = '\n';
                buf[len+1] = 0;
        }

        printf("%s", buf);
        if (verbose_output != NULL) {
                fprintf(verbose_output, "%s", buf);
        }
}
#define printf_err printf_log
#define printf_verbose(format, args...) \
        do { \
                if (isVerbose) { \
                        printf_log(format,##args); \
                } \
        } while(0)
#else
#define printf_err printf
#define printf_verbose(format, args...) \
        do { \
                if (isVerbose) { \
                        printf(format,##args); \
                } \
        } while(0)
#endif

//e.g. portname = "/dev/ttyUSB1"
int open_serial_port(char *portname)
{
        int fd = open (portname, O_RDWR | O_NOCTTY | O_SYNC);
        if (fd < 0) {
                printf_err("error %d opening %s: %s", errno, portname, strerror (errno));
                return INVALID_HANDLE_VALUE;
        }
        return fd;
}

void segfault_sigaction(int signal, siginfo_t *si, void *arg)
{
        exit(0);
}

void close_serial_port(int hComm)
{
        //Because last action of firmware update is a reset of the target,
        //device and serial port can disappear before this close operation.
        //So, install a handler to catch the resulting segfault quietly.
        struct sigaction sa;

        memset(&sa, 0, sizeof(sigaction));
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = segfault_sigaction;
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &sa, NULL);

        if (hComm) {
                close(hComm);
        }
}

int configure_serial(void)
{
        struct termios tty;

        if (tcgetattr(hComm, &tty) < 0) {
                printf_err("Error from tcgetattr: %s\n", strerror(errno));
                return -1;
        }

        cfsetospeed(&tty, (speed_t)B115200);
        cfsetispeed(&tty, (speed_t)B115200);

        tty.c_cflag |= (CLOCAL | CREAD); /* ignore modem controls */
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8; /* 8-bit characters */
        tty.c_cflag &= ~PARENB; /* no parity bit */
        tty.c_cflag &= ~CSTOPB; /* only need 1 stop bit */
//      tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */
        tty.c_cflag |= CRTSCTS; /* hardware flow control */

        /* setup for non-canonical mode */
        tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
#if __APPLE__
        tty.c_iflag &= ~(INPCK | IXOFF | IMAXBEL);
#else
        tty.c_iflag &= ~(INPCK | IXOFF | IUCLC | IMAXBEL);

#endif
        tty.c_iflag |= (INPCK);

        tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        tty.c_lflag &= ~(ECHOE | ECHOK | ECHOCTL | ECHOPRT | ECHOKE | FLUSHO | PENDIN | TOSTOP);
        tty.c_lflag |= (NOFLSH);

        tty.c_oflag &= ~OPOST; //chooses RAW output - all other oflag option ignored in this case

        if (tcsetattr(hComm, TCSANOW, &tty) != 0) {
                printf_err("Error from tcsetattr: %s\n", strerror(errno));
                return -1;
        }
        return 0;
}

int set_serial_timeouts(void)
{
        struct termios tty;

        if (tcgetattr(hComm, &tty) < 0) {
                printf_err("Error from tcgetattr: %s\n", strerror(errno));
                return -1;
        }

        tty.c_cc[VMIN] = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;// 0.5 second read timeout

        if (tcsetattr(hComm, TCSANOW, &tty) != 0) {
                printf_err("Error from tcsetattr: %s\n", strerror(errno));
                return -1;
        }
        return 0;
}

bool WriteFile(int hComm,       // Handle to the Serial port
        char *buff,             // Data to be written to the port
        int len,                //No of bytes to write
        int *dNoOfBytesWritten, //Bytes written
        void *ignore)
{
        uint32_t wrote = write (hComm, buff, len);

        if (wrote > 0) {
                *dNoOfBytesWritten = wrote;

                fsync(hComm); //ensure target receives to generate expected response!

                return true;
        } else {
                *dNoOfBytesWritten = 0;
                return false;
        }
}

bool ReadFile( int hComm,       //Handle of the Serial port
        char *buff,
        int len,                //Size of buff
        int *NoBytesRead,       //Number of bytes read
        void *ignore)
{
        int n = read(hComm, buff, len);

        if (n > 0) {
                *NoBytesRead = n;
                return true;
        } else if (n==0) {
                printf_verbose("Readfile: 0 bytes, errno=[%s]\n", strerror(errno));
                *NoBytesRead = 0;
                return false;
        } else {
                *NoBytesRead = 0;
                printf_verbose("Readfile: ERROR=%d=[%s]\n", n, strerror(errno));
                return false;
        }
}

#else
#error "not supported"
#endif

DWORD write_byte(char c)
{
        char lpBuffer;  // = "A";
        DWORD dNoOFBytestoWrite;        // No of bytes to write into the port
        DWORD dNoOfBytesWritten = 0;    // No of bytes written to the port
        dNoOFBytestoWrite = sizeof(lpBuffer);

        lpBuffer = c;

        if (!WriteFile(hComm,           // Handle to the Serial port
                &lpBuffer,              // Data to be written to the port
                1,                      //No of bytes to write
                &dNoOfBytesWritten,     //Bytes written
                NULL)) {
                printf_verbose("HOST=[write_byte: Error from WriteFile]\n");
        }

        return dNoOfBytesWritten;
}

DWORD write_buff(char *buff, DWORD len)
{
        int attempts = 0;
        DWORD dNoOfBytesWritten = 0;    // No of bytes written to the port

        do {
                if (!WriteFile(hComm,         // Handle to the Serial port
                        buff,                // Data to be written to the port
                        len,                 //No of bytes to write
                        &dNoOfBytesWritten,  //Bytes written
                        NULL)) {
                        printf_verbose("HOST=[write_buff: Error from WriteFile]\n");
                        break;
                }
                if (attempts) {
                        printf_verbose("HOST=[write_buff: RETRY]\n");
                }
                attempts++;
        } while ((len != dNoOfBytesWritten) && (attempts < 3));

        return dNoOfBytesWritten;
}

DWORD read_byte(char *c)
{
        char TempChar;  //Temporary character used for reading
        DWORD NoBytesRead = 0;
        if (!ReadFile(hComm,            //Handle of the Serial port
                &TempChar,              //Temporary character
                sizeof(TempChar),       //Size of TempChar
                &NoBytesRead,           //Number of bytes read
                NULL)) {
                printf_verbose("HOST=[read_byte: Error from ReadFile]\n");
                return 0;
        } else if (NoBytesRead == 1) {
                *c = TempChar;  //Store Tempchar into buffer
                return 1;
        }

        return 0;
}

DWORD read_line(char *buff, DWORD len)
{
        memset(buff, 0, len);
        char TempChar; //Temporary character used for reading
        DWORD NoBytesRead = 0;
        int i = 0;

        do {
                if (!ReadFile(hComm,            //Handle of the Serial port
                        &TempChar,              //Temporary character
                        sizeof(TempChar),       //Size of TempChar
                        &NoBytesRead,           //Number of bytes read
                        NULL)) {
                        printf_verbose("HOST=[read_line: Error from ReadFile]\n");
                } else if (NoBytesRead == 1) {
                        if ((TempChar != '\n') && (TempChar != '\r')) {
                                buff[i] = TempChar;           // Store Tempchar into buffer
                                i++;
                        } else if (i == 0) {
                                TempChar = 0; //avoid exit over CRLF detritus at start of line
                        }
                }
        } while ((NoBytesRead > 0) && (i < len) && (TempChar != '\n') && (TempChar != '\r'));

        return i;
}

// Write command to buffer and reads back expected echo from CLI on target to flush
// Params:
//      buff:        pointer to string to issue to target
// Return:
//      true if ok
bool write_command_flush_echo(char *buff)
{
        bool ret = true;
        DWORD len = strlen(buff);
        DWORD dNoOfBytesWritten = 0;     // No of bytes written to the port

        if (!WriteFile(hComm,            // Handle to the Serial port
                buff,                // Data to be written to the port
                len,                 //No of bytes to write
                &dNoOfBytesWritten,  //Bytes written
                NULL)) {
                printf_err("HOST=[write_command_flush_echo: Error from WriteFile]\n");
                ret = false;
        } else {
                //read back echo from target
                char TempChar;
                DWORD NoBytesRead;
                int i = 0;

                do {
                        if (!ReadFile(hComm,     //Handle of the Serial port
                                &TempChar,       //Temporary character
                                sizeof(TempChar),       //Size of TempChar
                                &NoBytesRead,    //Number of bytes read
                                NULL)) {
                                printf_err(
                                        "HOST=[write_command_flush_echo: Error from ReadFile]\n");
                                ret = false;
                        } else if (NoBytesRead == 1) {
                                i++;
                        }
                } while ((i < len) && ret);
        }
        return ret;
}

// Issues command and outputs response(s) by
// Params:
//      cmdline:        pointer to string to issue to target
// Return:
//      true if successful, false if response to command was
bool issue_command(char *cmdline, char *okresponse, uint32_t okbufflen)
{
        char buff[60];
        DWORD len;
        char c;
        int n = 0, m = 0;
        bool ret = true;

        if (okresponse)
                okresponse[0] = 0;      //in case there's nothing back!

        //Try to get the CLI prompt
        m = 0;
        do {
                if (m > 0)
                        write_byte('\n');
                n = 0;
                do { //do two retries without sending a CR, to try and get prompt sent at port open, and consume any prefixed CR/LF
                        if (1 == read_byte(&c)) {
                                if (c == '>') {
                                        break;
                                } else if ((c != 10) && (c != 13))
                                        printf_verbose("COMMAND: Don't yet have the CLI prompt [%c] (0x%02x)\n", c, c);
                        } else {
                                printf_verbose("COMMAND: Don't yet have the CLI prompt\n");
                        }
                        n++;
                } while ((n < 2) && (c != '>'));
                m++;
        } while ((m < 3) && (c != '>'));

        if ((m == 4) && (n == 3)) {
                printf_verbose("HOST=[issue_command: ERROR - Failed to get the CLI prompt]\n");
        } else {
                //test a command
                ret = write_command_flush_echo(cmdline);
                if (ret == false) {
                        return ret;
                }

                ret = write_command_flush_echo("\n");
                if (ret == false) {
                        return ret;
                }

                printf_verbose("HOST=[issue_command: INFO - sent: %s]\n", cmdline);
                do {
                        len = read_line(buff, 59);
                        if(len == 0) {
                                return false;
                        }

                        buff[len] = 0;

                        if (0 == strncmp(buff, "INFO ", 5)) {
                                printf_verbose("DATA=[%s]\n", buff);
                        } else if (0 == strncmp(buff, "ERROR", 5)) {
                                printf_err("FAIL=[%s]\n", buff);
                                len = 0;
                                ret = false;
                        } else if (0 == strncmp(buff, "OK", 2)) {
                                printf_verbose("PASS=[%s]\n", buff);
                                len = 0;
                                if (okresponse && (buff[2] == ' ') && (buff[3] != 0)) {
                                        //i.e. 'O', 'K', ' ', value to test
                                        strncpy(okresponse, &buff[3], okbufflen);
                                }
                        }

                } while (0 != len);

        }
        return ret;
}

// Retry reading a response from the SUOTO bootloader
// Params:
//      retrycount:     iterations of 100ms delay
//      buff:           pointer to buffer to hold response in (60 bytes please)
//      len:            pointer to DWORD to put response length into
// Return:
//      true if error
bool wait_response(int retrycount, char *buff, DWORD *len)
{
        bool error = false;
        int retries = retrycount;

        *len = 0;
        buff[0] = 0;

        do {
                //Sleep(50);
                *len = read_line(buff, 59);
                if (*len) {
                        buff[*len] = 0;
                }
                retries--;
        } while ((0 == *len) && retries);

        if (!retries) {
                error = true;
        }

        return error;
}

// Issues command and confirms expected response
// Params:
//      command:        pointer to string to issue to target
//      buff:           pointer to buffer to hold response in (60 bytes please)
//      len:            pointer to DWORD to put response length into
// Return:
//      true if error or response incorrect
bool issue_command_get_ok(char *command, char *buff, DWORD *len)
{
        DWORD wrote;
        bool error = false;

        printf_verbose("issue_command_get_ok:COMMAND: %s\n", command);

        wrote = write_buff(command, strlen(command));
        error = (strlen(command) == wrote) ? false : true;

        if (error) {
                printf_err("issue_command_get_ok: sent:%ld wrote:%d\n", strlen(command), wrote);
                return true;
        }

        printf_verbose("issue_command_get_ok: WAIT\n");

        error = wait_response(100, buff, len);
        if (!error) {
                if (!(0 == strncmp(buff, "OK", 2))) {
                        printf_err("issue_command_get_ok: NO, got [%s]\n", buff);
                        error = true;
                } else {
                        printf_verbose("issue_command_get_ok: YES, got [%s]\n", buff); //\n");
                }
        }

        return error;
}

// Confirms expected response - abort SUOUART process if mismatch
// Params:
//      response:       pointer to string expected in response from target
//      buff:           pointer to buffer to hold response in (60 bytes please)
//      len:            pointer to DWORD to put response length into
// Return:
//      true if error, or response incorrect and process aborted
bool wait_for_specific_response_or_abort(char *response, int retrycount, char *buff, DWORD *len)
{
        unsigned char *suouart_mem_dev_abort = "SUOSERIAL_MEM_DEV 0 4 000000FF\n"; //{ 0xFF, 0x00, 0x00, 0x00 };

        bool error = wait_response(retrycount, buff, len);
        if (!error) {
                if (0 == strncmp(buff, response, strlen(response))) {
                } else {
                        printf_err("wait_for_specific_response_or_abort: FAIL=[%s]\n", buff);
                        write_buff(suouart_mem_dev_abort, strlen(suouart_mem_dev_abort));
                        error = true;
                }
        } else {
                printf_verbose("wait_for_specific_response_or_abort: TIMEOUT\n");
                write_buff(suouart_mem_dev_abort, strlen(suouart_mem_dev_abort));
        }
        return error;
}

/**
 * Brief:  Given a hex nibble, returns ASCII character value
 * Param:  Value 0x0 to 0x0F
 * Return: '0'-'9', or 'A'-'F')
 */
static inline uint8_t nibble2asciibyte(uint8_t c)
{
        return (c < 0xA) ? (c + '0') : (c - 0xA + 'A');
}

bool do_firmware_update(unsigned char *imagebuf, uint32_t size, uint32_t suouartbuffsz)
{
        printf_verbose("do_firmware_update with size=%d and suoserialtbuffsz=%d\n", size, suouartbuffsz);
        uint32_t wrote;
        uint32_t xfered = 0;
        bool error = false;
        char buff[60];
        DWORD len;

        char *strbuff;
        char *hexbuff;

        uint32_t chunksz = (suouartbuffsz / 2); // For every byte of code, we transfer 2 bytes over the serial port

        /*
         Table 3: SUOSERIAL_MEM_DEV definition for SUOUART mode
         Byte    Description
         3       This is the Most Significant Byte. Values:
         0x00 to 0x11: Reserved
         0x12: Image is stored in I2C EEPROM
         0x13: Image is stored in SPI FLASH
         0x14 to 0xFC: Reserved
         0xFD: SUOUART reboot command. Reboot Immediately
         0xFE: SUOUART end command. Indicates that image transfer has been completed..
         0xFF: SUOUART abort command. Return to normal application.
         2       0x00
         1       0x00
         0       If byte #3 is 0x12 or 0x13 then it is the image bank:
         0x00: oldest
         0x01: Image #1
         0x02: Image #1
         Otherwise it must be equal to 0x00
         */
        //LSB first though...
        unsigned char *suouart_mem_dev_start = "SUOSERIAL_MEM_DEV 0 4 00000013\n";
        unsigned char *suouart_mem_dev_reset = "SUOSERIAL_MEM_DEV 0 4 000000FD\n"; //{ 0xFD, 0x00, 0x00, 0x00 };
        unsigned char *suouart_mem_dev_end = "SUOSERIAL_MEM_DEV 0 4 000000FE\n"; //{ 0xFE, 0x00, 0x00, 0x00 };
        unsigned char *suouart_mem_dev_abort = "SUOSERIAL_MEM_DEV 0 4 000000FF\n"; //{ 0xFF, 0x00, 0x00, 0x00 };
        unsigned char suouart_patch_len[60]; //i.e. "SUOSERIAL_PATCH_LEN 0 2 xxxx\n", will fabricate
        unsigned char *suouart_write_status_ena_ntfy = "SUOSERIAL_WRITE_STATUS 0 2 0100\n";

        if (size < 64) {   //ensure big enough to have a header! (user not insane)
                return true;
        }

        if ((suouartbuffsz / 2) < 64) {    //ensure header can fit in single buffer (target not insane)
                return true;
        }

        strbuff = malloc(100 + (chunksz * 2));    //for complete CLI command to write data
        hexbuff = malloc((chunksz * 2) + 1);      //for hex conversion bit

        if (!strbuff || !hexbuff) {
                printf_err("out of memory!\n");
                if (strbuff) {
                        free(strbuff);
                }
                if (hexbuff) {
                        free(hexbuff);
                }
                return true;
        }

        //START UPDATE PROCESS
        printf_verbose("do_firmware_update: start work\n");

        error = issue_command_get_ok(suouart_write_status_ena_ntfy, buff, &len);
        if (error) {
                printf_err("Failure to get ok command \n");
                return error;
        }
        //SUOSERIAL_MEM_DEV        Initiator defines the Memory type (SPI or EEPROM) and the bank selection
        error = (strlen(suouart_mem_dev_start)
                == write_buff(suouart_mem_dev_start, strlen(suouart_mem_dev_start))) ? false : true;
        if (error) {
                printf_err("Failure at write_buff\n");
                return error;
        }
        //Wait for SUOSERIAL_SERV_STATUS=SUOSERIAL_IMG_STARTED
        error = wait_for_specific_response_or_abort("INFO SUOSERIAL_IMG_STARTED", 300, buff, &len);
        if (error) {
                printf_err("Failure at INFO SUOSERIAL_IMG_STARTED\n");
                return error;
        }
        //Wait for the SUOSERIAL_PATCH_DATA response OK
        error = wait_for_specific_response_or_abort("OK", 300, buff, &len);
        if (error) {
               printf_err("Failure at SUOSERIAL_PATCH_DATA response\n");
               return error;
        }

        //Set size of blocks for main bulk of update
        if ((size - xfered) > suouartbuffsz) {
                //SUOSERIAL_PATCH_LEN   Initiator defines the length of the Block size to be applied
                //                      Receiver stores the transmitted Length in a temporary variable
                sprintf(suouart_patch_len, "SUOSERIAL_PATCH_LEN 0 2 %02x%02x\n", (chunksz & 0xFF),
                        ((chunksz >> 8) & 0xFF));
                error = issue_command_get_ok(suouart_patch_len, buff, &len);
                if (error) {
                       printf_err("Error in issue_command_get_ok\n");
                       return error;
                }
        }

        printf_verbose("do_firmware_update: sending %d bytes in %d blocks\n", size, chunksz);
        //Perform main bulk of update
        while ((size - xfered) > chunksz) {
                int n, m;
                int retries;

                retries = 30; //3 second timeout for the write of each block
                
                printf_verbose("do_firmware_update: send %d byte block @ %d\n",
                                chunksz, xfered);
                //SUOSERIAL_PATCH_DATA * X
                for (m = 0; m < chunksz; m++) {
                        uint8_t c = imagebuf[xfered + m];
                        hexbuff[(m * 2) + 0] = nibble2asciibyte(c >> 4);
                        hexbuff[(m * 2) + 1] = nibble2asciibyte(c & 0xF);
                }
                hexbuff[chunksz * 2] = 0;
                sprintf(strbuff, "SUOSERIAL_PATCH_DATA 0 %d %s\n", chunksz, hexbuff);
                error = (strlen(strbuff) == write_buff(strbuff, strlen(strbuff))) ? false : true;
                if (error) {
                        break;
                }
                xfered += chunksz;
                error = wait_for_specific_response_or_abort("INFO SUOSERIAL_CMP_OK", 300, buff, &len);
                if (error) {
                        break;
                }
        }

        printf_verbose("do_firmware_update: end %d byte block processing - %s - remainder:%d\n",
                        suouartbuffsz, (error ? "ERROR" : "OK"), (size - xfered));

        if (error) {
                printf_err("Error in do_firmware_update\n");
                return error;;
        }
        //Set new block size to remainder
        if (size - xfered) {
                printf_verbose("do_firmware_update: set up block size %d\n", (size - xfered));

                //SUOSERIAL_PATCH_LEN   Initiator defines the length of the Last Block size (if different) to be applied
                //                      Receiver stores the new block size
                sprintf(suouart_patch_len, "SUOSERIAL_PATCH_LEN 0 2 %02x%02x\n",
                        (((size - xfered)) & 0xFF), ((((size - xfered)) >> 8) & 0xFF));

                error = issue_command_get_ok(suouart_patch_len, buff, &len);
                if (error) {
                        printf_err("do_firmware_update: set up block size %d - %s\n",
                                (size - xfered), (error ? "ERROR" : "OK"));
                        return error;
                } else {
                        printf_verbose("do_firmware_update: set up block size %d - %s\n",
                                 (size - xfered), (error ? "ERROR" : "OK"));
                }
        }

        //Perform transfer of remainder
        if (size - xfered) {
                int n, m;
                int retries;

                retries = 30; //3 second timeout for the write of each block
                printf_verbose("do_firmware_update: send %d byte block @ %d\n", (size - xfered), xfered);

                //Do all the chunksz byte packets we can
                while (!error && ((size - xfered) >= chunksz)) {
                        //SUOSERIAL_PATCH_DATA * Y
                        for (m = 0; m < chunksz; m++) {
                                uint8_t c = imagebuf[xfered + m];
                                hexbuff[(m * 2) + 0] = nibble2asciibyte(c >> 4);
                                hexbuff[(m * 2) + 1] = nibble2asciibyte(c & 0xF);
                        }
                        hexbuff[chunksz * 2] = 0;
                        sprintf(strbuff, "SUOSERIAL_PATCH_DATA 0 %d %s\n", chunksz, hexbuff);
                        error = (strlen(strbuff) == write_buff(strbuff, strlen(strbuff))) ? false : true;
                        xfered += chunksz;
                }
                //Then deal with any <chunksz bytes remainder
                if (!error && (size - xfered)) {
                        uint32_t rlen = size - xfered;

                        //SUOUART_PATCH_DATA * Y
                        for (m = 0; m < rlen; m++) {
                                uint8_t c = imagebuf[xfered + m];
                                hexbuff[(m * 2) + 0] = nibble2asciibyte(c >> 4);
                                hexbuff[(m * 2) + 1] = nibble2asciibyte(c & 0xF);
                        }
                        hexbuff[rlen * 2] = 0;
                        sprintf(strbuff, "SUOSERIAL_PATCH_DATA 0 %d %s\n", rlen, hexbuff);
                        error = (strlen(strbuff) == write_buff(strbuff, strlen(strbuff))) ? false : true;
                        xfered += rlen;
                }

                if (error) {
                        printf_err("Error in do_firmware_update\n");
                        return error;;
                }
                //Wait for SUOSERIAL_SERV_STATUS=OK
                error = wait_for_specific_response_or_abort("INFO SUOSERIAL_CMP_OK", 300, buff, &len);
        }

        printf_verbose("do_firmware_update: end last block processing - %s - remainder:%d\n",
                        (error ? "ERROR" : "OK"), (size - xfered));

        if (error) {
                printf_err("Error in do_firmware_update\n");
                return error;
        }
        //SUOSERIAL_MEM_INFO    Initiator requests the total number of bytes received by receiver
        //                      4 Bytes of Data
        //                      (Total number of received bytes)
        //
        //Check size matches, of not abort update
        error = issue_command_get_ok("SUOSERIAL_READ_MEMINFO 0 1 00\n", buff, &len);
        if (!error) {
                int length = atoi(&buff[3]); //i.e. 'O', 'K', ' ', value to test

                if (length != size) {
                        printf_err("ERROR from SUOSERIAL_READ_MEMINFO\n");
                        printf_err("LEN?=[%d]\n", length);
                        printf_err("RAW?=[%s]\n", buff);

                        write_buff(suouart_mem_dev_abort, strlen(suouart_mem_dev_abort));
                        error = true;
                } else {
                        printf_verbose("do_firmware_update: SUOSERIAL_READ_MEMINFO size ok %d %d [%s]\n",
                                        size, length, buff);
                }
        } else {
                printf_err("do_firmware_update: SUOSERIAL_READ_MEMINFO error\n");
                return error;
        }

        //SUOSERIAL_MEM_DEV - End of transfer (0xFE000000)
        printf_verbose("do_firmware_update: send suoserial_mem_dev_end\n");

        error = (strlen(suouart_mem_dev_end)
                == write_buff(suouart_mem_dev_end, strlen(suouart_mem_dev_end))) ? false : true;
        if (error) {
                printf_err("do_firmware_update: suoserial_mem_dev_end error\n");
                return error;
        }
        //Wait for SUOSERIAL_SERV_STATUS=OK - Receiver verifies image checksum and writes image header
        printf_verbose("do_firmware_update: wait SUOSERIAL_CMP_OK\n");
        error = wait_for_specific_response_or_abort("INFO SUOSERIAL_CMP_OK", 300, buff, &len);
        if (error) {
                printf_err("do_firmware_update: SUOSERIAL_CMP_OK error\n");
                return error;
        }
        //SUOSERIAL_MEM_DEV - System Reboot Command
        printf_verbose("do_firmware_update: send suoserial_mem_dev_reset\n");
        error = (strlen(suouart_mem_dev_reset) == write_buff(suouart_mem_dev_reset,
                        strlen(suouart_mem_dev_reset))) ? false : true;
        if (error) {
                printf_err("do_firmware_update: suoserial_mem_dev_reset error\n");
        }

        printf_verbose("do_firmware_update: STOP\n");

        if (strbuff) {
                free(strbuff);
        }
        if (hexbuff) {
                free(hexbuff);
        }

        return error;
}

int main(int argc, char **argv)
{
        int exitCode = 1;
        int comportnum;
        struct stat st; //"error: storage size of 'st' isn't known" ??
        unsigned char *buf;
        int res;
        size_t actual;
        char clibuff[60];
        bool error;

        printf_err("HOST_USB_UPDATER_VERSION = %d \n", HOST_USB_UPDATER_VERSION);

        if (argc < 3) {
                printf_err("usage: %s <comport> <image_file.img> [-verbose]\n", argv[0]);
                return exitCode;
        }
#ifdef HOST_USB_UPDATER_LOG
        verbose_output = fopen("host_usb_updater.log","wb");
#endif
        if (argc > 3) {
                isVerbose = (strcmp(argv[3], "-verbose") == 0);
        }

        hComm = open_serial_port(argv[1]);
        if (hComm == INVALID_HANDLE_VALUE) {
                printf_err("Error in opening serial port\n");
                goto RESULT;
        }

        printf_verbose("opening serial port successful\n");

#ifdef HOST_USB_UPDATER_SERIAL_PORT_MODS2
        Sleep(1000);    //delay because Windows usbser.sys driver doesn't handle DSR
#endif
        if (0 > configure_serial()) {
                close_serial_port(hComm);
                exit(1);
        }
        if (0 > set_serial_timeouts()) {
                close_serial_port(hComm);
                exit(1);
        }

        printf_verbose("=== Try loading firmware image ===\n");

        res = stat(argv[2], &st);
        if (res == -1) {
                printf_err("ERROR: state\n");
                goto RESULT;
        }

        buf = malloc(st.st_size);

        if (buf == 0) {
                printf_err("ERROR: malloc / image size\n");
                goto RESULT;
        }

        FILE *fp = fopen(argv[2], "rb");
        if (fp == 0) {
                printf_err("ERROR: fopen\n");
                goto RESULT;
        }

        actual = fread(buf, 1, st.st_size, fp);
        fclose(fp);
        if (st.st_size != actual) {
                printf_err("ERROR: fread (req:%ld actual:%ld)\n", st.st_size, actual);
                goto RESULT;
        }

        printf_verbose("=== Try to perform USB firmware update ===\n");

        //get size of SUOUART buffer
        if (issue_command("getsuoserialbuffsz", clibuff, 60)) {
                uint32_t suouartbuffsz = atoi(clibuff);
                //Allocate same as SUOSERIAL buffer as working buffer for data transfer
                //We send hex, but it will be translated into binary into this working buffer
                //The target will make sure the CLI buffer is big enough for holding enough
                //hex data in a command to be converted to fill this buffer
                //e.g. "SUOSERIAL_PATCH_DATA 0 %d %s", suouartbuffsz, hexstringbuff
                sprintf(clibuff, "alloc %d", (suouartbuffsz / 2));

                if (issue_command(clibuff, NULL, 0)) {
                        if (issue_command("fwupdate", NULL, 0)) {
                                exitCode = do_firmware_update(buf, st.st_size, suouartbuffsz) ? 1 : 0;
                        } else {
                                printf_err("ERROR: fwupdate\n");
                        }
                } else {
                        printf_err("ERROR: alloc\n");
                }
        } else {
                printf_err("ERROR: getsuoserialbuffsz HERE???\n");
        }

        free(buf);

#ifdef _WIN32
        for (int i=5; i>=0; i--){
            printf("Closing window in %d sec\r", i);
            Sleep(1000);
        }
#endif
        close_serial_port(hComm);

RESULT:
        printf_err("\nResult:%s\n", (exitCode == 0) ? "Pass" : "Fail");
#ifdef HOST_USB_UPDATER_LOG
        if (verbose_output != NULL) {
                fclose(verbose_output);
        }
#endif


        return exitCode;
}
