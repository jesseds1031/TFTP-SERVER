#include <iostream>
#include <arpa/inet.h>
#include <fstream>
#include <errno.h> // for retrieving the error number.
#include <netinet/in.h>
#include <signal.h> // for the signal handler registration.
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strerror function.
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "../tftp/tftp.cpp"
#include <pthread.h>
#include <thread>
#include <arpa/inet.h>

#define SERV_UDP_PORT 51870
#define SERV_HOST_ADDR "10.158.82.34"
#define MAX_DATA_SIZE 512
#define DATA_OFFSET 4
#define TIMEOUT 3
#define MAX_TIMEOUTS 10
// thread variables
pthread_t threads [100];
 int threadno = 0, fd;

 
using namespace std;

//track timeouts
int timeoutCounter = 0;


// function mainly for testing packet contents
void printArray(char arr[])
{
    for (int i = 0; i < sizeof(arr); i++)
    {
        cout << arr[i] << ",";
    }
}

// timeout handler
void handle_timeout(int signum)
{
    timeoutCounter++;
    printf("timeout occurred, resending packet...\n");
    cout << "timeout #: " << timeoutCounter << endl;
}

// to register timeout handler
int register_handler()
{
    //printf("running register_handle");
    int r_value = 0;
    // register handler function
    r_value = ((intptr_t) signal( SIGALRM, handle_timeout));
    //ensure register success
    if (r_value == ((intptr_t) SIG_ERR))
    {
        printf("Error registering functon handle_timeout");
        printf("signal() error: %s.\n", strerror(errno));
        return -1;
    }
    //disable the restart of system call
    r_value = siginterrupt(SIGALRM, 1);
    if (r_value == -1)
    {
        printf("invalid sig number. \n");
        return -1;
    }
    return 0;
}


// Server handles WRQ from the client
bool handleWRQ(int sockfd, TFTP &tftpServer, ofstream &outputFile, string filename, sockaddr_in cli_addr, socklen_t clilen)
{
    int n;
    unsigned short dataNum = 1;
    unsigned short ackNum = 0;

    for (;;)
    {
        // for timeout testing
        sleep(1);
        // check if timeouts is 10 or more
        if (timeoutCounter >= MAX_TIMEOUTS)
        {
            printf("Max timeouts exceeded, closing connection.\n");
            timeoutCounter = 0;
            return false;
        }


        bool sendSuccess = tftpServer.sendAckPacket(ackNum, sockfd, cli_addr, clilen);
        if (!sendSuccess)
        {
            cerr << "Cannot Send Ack Packet: " << ackNum << endl;
            exit(1);
        }
        cout << "ackNum #: " << ackNum << endl;

        //timeout alarm
        cout << "setting a timeout alarm" << endl;
        int x = alarm(TIMEOUT);
        printf("Waiting to receive Data from client\n");

        bool recvSuccess = tftpServer.recvPacket(sockfd, MAX_DATA_SIZE, cli_addr, clilen);
        if (!recvSuccess)
        {
            if (errno == EINTR)
            {
                printf("timeout triggered!\n");
                continue;
            }
        }
        else 
        {
            printf("data received from client, clearing timeout alarm\n");
            alarm(0);
            //reset counter
            timeoutCounter = 0;
        }
        if (tftpServer.buffer[1] != OP_CODE_DATA)
        {
            cout << "Packet is not a data tftpServer" << endl;
            exit(3);
        }
        cout << "Data Packet #: " << dataNum << endl;

        // if (dataNum != (ackNum + 1))
        // {
        //     cout << "Data Block # and Ack # are not consistent" << endl;
        //     exit(2);
        // }
        dataNum++;
        ackNum++;
        string s(reinterpret_cast<char *>(tftpServer.buffer), sizeof(tftpServer.buffer));
        unsigned short lastbyte = tftpServer.buffer[MAX_DATA_SIZE - 1];
        if (sendSuccess && recvSuccess)
        {
            tftpServer.writeToFile(outputFile, s);
            if (lastbyte == '\0')
            {
                bool sendSuccess = tftpServer.sendAckPacket(ackNum, sockfd, cli_addr, clilen);
                if (!sendSuccess)
                {
                    cerr << "Cannot Send Ack Packet: " << ackNum << endl;
                    exit(1);
                }
                break;
            }
        }
    }
    printf("write request complete.");
    return true;
}
// function to take care all of all parts of read requests
bool handleRRQ(int sockfd, TFTP &tftpServer, ifstream &inputFile, string filename, sockaddr_in cli_addr, socklen_t clilen)
{
    int n;
    unsigned short dataNum = 1;
    unsigned short ackNum = 0;

    bool isTimeout = false;

    int i = 0;
    char file[MAX_DATA_SIZE];
    if (inputFile.is_open())
    {
        while (!inputFile.eof())
        {

            inputFile >> file[i];
            
            if (i == 511)
            {
                // check if timeouts is 10 or more
                if (timeoutCounter >= MAX_TIMEOUTS)
                {
                    printf("Max timeouts exceeded, closing connection.\n");
                    timeoutCounter = 0;
                    return false;
                }

                // tftpServer.createDataPacket(file, filename, dataNum);
                bool sendSuccess = tftpServer.sendDataPacket(sockfd, file, filename, dataNum, cli_addr, clilen);
                if (!sendSuccess)
                {
                    cerr << "Cannot Send Data Packet: " << dataNum << endl;
                    exit(2);
                }
                cout << "Data tftpServer with block #: " << dataNum << " sent" << endl;
                i = 0;
                bzero(file, sizeof(file));


                //timeout alarm
                cout << "setting a timeout alarm" << endl;
                int x = alarm(TIMEOUT);
                printf("Waiting to receive ack from client\n");


                // recive ack from client
                bool recvSuccess = tftpServer.recvPacket(sockfd, MAX_DATA_SIZE, cli_addr, clilen);
                if (!recvSuccess)
                {
                    if (errno == EINTR)
                    {
                        printf("timeout triggered!\n");
                        isTimeout = true;
                        continue;
                    }
                }
                else 
                {
                    printf("ack received from client, clearing timeout alarm\n");
                    isTimeout = false;
                    alarm(0);
                    //reset counter
                    timeoutCounter = 0;
                }

                if (tftpServer.buffer[1] != OP_CODE_ACK)
                {
                    cout << "Packet is not Ack tftpServer" << endl;
                    exit(3);
                }
                ackNum++;
                if (dataNum != ackNum)
                {
                    cout << "Data Block # != Ack Block #" << endl;
                    exit(3);
                }
                dataNum++;
                cout << "Client sent Ack #: " << ackNum << endl;
            }
            else
            {
                i++;
            }
        }

        bool sendSuccess = tftpServer.sendDataPacket(sockfd, file, filename, dataNum, cli_addr, clilen);
        if (!sendSuccess)
        {
            cerr << "Cannot Send Data Packet: " << dataNum << endl;
            exit(2);
        }
        cout << "Data tftpServer with block #: " << dataNum << " sent" << endl;
        i = 0;
        bzero(file, sizeof(file));
        // recive ack from client
        bool recvSuccess = tftpServer.recvPacket(sockfd, MAX_DATA_SIZE, cli_addr, clilen);
        if (!recvSuccess)
        {
            cerr << "Cannot Recieve Ack #: " << ackNum << endl;
            exit(2);
        }

        if (tftpServer.buffer[1] != OP_CODE_ACK)
        {
            cout << "Packet is not Ack tftpServer" << endl;
            exit(3);
        }
        ackNum++;
        if (dataNum != ackNum)
        {
            cout << "Data Block # != Ack Block #" << endl;
            exit(3);
        }

        //receive all backed up acks
        // while (ackNum !=)
        // {
        //     ackNum++;
        //     cout << "Client sent Ack #: " << ackNum << endl;
        // }


        inputFile.close();
        cout << "Send Completed" << endl;
        return true;

        
    }
}

// core server logic
int main(int argc, char *argv[])
{
    TFTP tftpServer = TFTP();
    // for commandline port option
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;


    cout << "argc: " << argc << endl;
    if (argc == 3)
    {
        int newUDPPort = atoi(argv[2]);
        sockfd = tftpServer.createSocket(sockfd, serv_addr, cli_addr, newUDPPort, "server");
        cout << "Using Port #: " << newUDPPort << " and Sockfd: " << sockfd << endl;
    }
    else
    {
        sockfd = tftpServer.createSocket(sockfd, serv_addr, cli_addr, SERV_UDP_PORT, "server");
        cout << "Using Port #: " << SERV_UDP_PORT << " and Sockfd: " << sockfd << endl;
    }

    tftpServer.clearPacket();
    int n;
    

    if (register_handler() != 0)
    {
        printf("could not register timeout");
    }

    

    cout << "Waiting for connections..." << endl;

    for (;;)
    {

        socklen_t clilen = sizeof(struct sockaddr);

        /* Receive data on socket sockfd, up to a maximum of MAXMESG       */
        n = recvfrom(sockfd, tftpServer.buffer, MAX_DATA_SIZE, 0, (struct sockaddr *)&cli_addr, &clilen);
        string s(reinterpret_cast<char *>(tftpServer.buffer), sizeof(tftpServer.buffer));

        //cout << "s: " << s << endl;

        // string s(reinterpret_cast<char *>(tftpServer.buffer), sizeof(tftpServer.buffer));
        // RRQ
        if (tftpServer.buffer[1] == OP_CODE_RRQ)
        {
            // cout << s << endl;
            string filename = tftpServer.getFileName();
            cout << "Client Request to Read File: " << filename << endl;

            const char* name = filename.c_str();

             // check permissions
            if (access(name, R_OK) != 0)
            {
                cout << "Invalid permissions for this file. Sending error packet" << endl;
                tftpServer.sendErrorPacket(sockfd, cli_addr, clilen, 2);                
                continue;
            }

            ifstream inputFile;
            // does it exist?
            inputFile.open(name);
            cout << "name: " << name << endl;
            if (!inputFile.is_open())
            {
                // file not found
                cout << "File not found. Sending error packet" << endl;
                tftpServer.sendErrorPacket(sockfd, cli_addr, clilen, 1);
                continue;
            }

            inputFile >> noskipws;
            if (!inputFile)
            {
                cerr << "Cannot Read File" << endl;
                exit(0);
            }

            pid_t child = fork();

            if (child == 0)
            {
                sockfd = tftpServer.createSocket(sockfd, serv_addr, cli_addr, SERV_UDP_PORT, "server");
                cout << "Using Port #: " << SERV_UDP_PORT << " and Sockfd: " << sockfd << endl;
                handleRRQ(sockfd, tftpServer, inputFile, filename, cli_addr, clilen);
            }
            
            //std::thread t(handleRRQ, sockfd, tftpServer, inputFile, filename, cli_addr, clilen);
            //t.join();
            // server should continue and wait for new requests
        }
        // WRQ
        else if (tftpServer.buffer[1] == OP_CODE_WRQ)
        {

            string filename = tftpServer.getFileName();
            cout << "Client Request to Write File: " << filename << endl;

            // check if file already exists
            ifstream inputFile;
            inputFile.open(filename);
            if (inputFile.is_open())
            {
                // file already exists

                cout << "error: File already exists, do you wish to overwrite: " << filename 
                    << "? enter y for yes, n for no." << endl;
                
                char temp;
                cin >> temp;

                if (temp == 'n')
                {
                    cout << "exiting now." << endl;
                    exit(0);
                }
                inputFile.close();

            }
            else
            {
                inputFile.close();
            }

            
            ofstream outputFile;
            outputFile << noskipws;
            outputFile.open(filename.c_str());
            if (!outputFile)
            {
                cerr << "Cannot Open File for Writing" << endl;
            }

            pid_t child = fork();

            if (child == 0)
            {
                sockfd = tftpServer.createSocket(sockfd, serv_addr, cli_addr, SERV_UDP_PORT, "server");
                cout << "Using Port #: " << SERV_UDP_PORT << " and Sockfd: " << sockfd << endl;
                handleWRQ(sockfd, tftpServer, outputFile, filename, cli_addr, clilen);
            }
            
            // server should continue and wait for new requests
        }
    }
}
