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
#include "../tftp/tftp.h"

#define SERV_UDP_PORT 51851
#define SERV_HOST_ADDR "10.158.82.34"
#define MAX_DATA_SIZE 512
#define DATA_OFFSET 4
using namespace std;

const string RRQ = "-r";
const string WRQ = "-w";


int main(int argc, char* argv[])
{
    tftp packet = tftp();
    
    // for commandline port option
    bool isDefaultPort = true;
    int newUDPPort;

    if (argc == 3)
    {
        newUDPPort = atoi(argv[2]);
        cout << newUDPPort << endl;
        isDefaultPort = false;
    }

    int sockfd;
    struct sockaddr_in serv_addr, cli_addr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        cout << "Cannot create socket" << endl;
        exit(1);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);

    // new port option
    if (!isDefaultPort)
    {
        serv_addr.sin_port = htons(newUDPPort);
        cout << "Using Port #: " << newUDPPort << endl;
    }
    else
    {
        serv_addr.sin_port = htons(SERV_UDP_PORT);
        cout << "Using Default Port #: " << SERV_UDP_PORT << endl;
    }
    
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        cout << "Cannot create socket" << endl;
        exit(2);
    }

    /* Temporary variables, counters and buffers.                      */

    /* Main echo server loop. Note that it never terminates, as there  */
    /* is no way for UDP to know when the data are finished.           */

    packet.clearPacket();
    int n;
    socklen_t clilen;

    ifstream file;

    cout << "Socket is working with: " << sockfd << endl;
    for (;;)
    {
        clilen = sizeof(struct sockaddr);
        /* Receive data on socket sockfd, up to a maximum of MAXMESG       */
        n = recvfrom(sockfd, packet.buffer, MAX_DATA_SIZE, 0, (struct sockaddr *)&cli_addr, &clilen);


        if (n != sizeof(packet.buffer))
        {
            cout << "Do not recieve" << endl;
        }

        if (n < 0)
        {
            cout << "Recieve Error" << endl;
        }

        // for (int i = 0; i < MAX_DATA_SIZE; i++) {
        //     cout << mesg[i] << endl;
        // }
        // read request
        //cout << "mesg[1] = " << packet.buffer[1] << endl;
        if (packet.buffer[1] == 1)
        {
        
            string s(reinterpret_cast<char *>(packet.buffer), sizeof(packet.buffer));
            cout << "this is read request" << endl;
            string fileName = packet.getFileName();
            const char* name = fileName.c_str();

             // check permissions
            if (access(name, R_OK) != 0)
            {
                packet.createErrorPacket(2);
                sendto(sockfd, packet.buffer, sizeof(packet.buffer), 0, (struct sockaddr *)&cli_addr, clilen) != sizeof(packet.buffer);
                
                exit(0);
            }

            // does it exist?
            file.open(fileName);
            if (!file.is_open())
            {
                // file not found
                packet.createErrorPacket(1);
                sendto(sockfd, packet.buffer, sizeof(packet.buffer), 0, (struct sockaddr *)&cli_addr, clilen) != sizeof(packet.buffer);
                exit(0);
            }

            // for (int i = 0; i < 15; i++)
            // {
            //     printf("0x%X,", mesg[i]);
            // }

            //packet.createDataPacket(filename);
            if (sendto(sockfd, packet.buffer, sizeof(packet.buffer), 0, (struct sockaddr *)&cli_addr, clilen) != sizeof(packet.buffer))
            {
                cout << "Recv  error" << endl;
                exit(3);
            }
        }
        else if (packet.buffer[1] == 2)
        {
            // count acks
            int ackCounter = 0;
            string s(reinterpret_cast<char *>(packet.buffer), sizeof(packet.buffer));
            string fileName = packet.getFileName();

            //vector of packets
            string packets;

            cout << "this is a write request" << endl;

            // check if file already exists

            file.open(fileName);
            if (file.is_open())
            {
                // file not found
                packet.createErrorPacket(6);
                sendto(sockfd, packet.buffer, sizeof(packet.buffer), 0, (struct sockaddr *)&cli_addr, clilen) != sizeof(packet.buffer);
                exit(0);
            }
            else
            {
                file.close();
                
            }

            // go until all packets are recieved and written
            for (;;)
            {
                packet.createACK(ackCounter);
                cout << "sending ack num: " << ackCounter << endl;
                ackCounter++;
                if (sendto(sockfd, packet.buffer, sizeof(packet.buffer), 0, (struct sockaddr *)&cli_addr, clilen) != sizeof(packet.buffer))
                {
                    cout << "Recv  error" << endl;
                    exit(3);
                }

                packet.clearPacket();


                n = recvfrom(sockfd, packet.buffer, MAX_DATA_SIZE, 0, (struct sockaddr *)&cli_addr, &clilen);
                string st(reinterpret_cast<char *>(packet.buffer), sizeof(packet.buffer));
                

                if (packet.buffer[1] == 3)
                {
                    //add to packet vector
                    packets += st;
                    cout << "packets: " << packets << endl;
                    cout << "st: " << st << endl;
                }
                else
                {
                    break;
                }
                
                if (n < 0)
                {
                    cout << "Recieve Error" << endl;
                }
                
            }
            cout << packets << endl;

            
            packet.writeFile(fileName, packets);
  
            
        }
        // createDataPacket("testfile.txt", "octet", buffer);
    }

    
    // string s(reinterpret_cast<char *>(buffer), sizeof(buffer));
    // cout << s << endl;
    // sendPacket(pcli_addr, sockfd, buffer);

    return 0;
}