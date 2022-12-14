#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <regex>
#include <ldap.h>

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543

///////////////////////////////////////////////////////////////////////////////

//function to verify inputs
bool verify(char* input, std::string version){
   int size = strlen(input);
   input[size-1] = '\0';
   std::string check = input;
   const std::regex patternNum("[0-9]+");
   const std::regex patternUser("[a-z0-9]{1,8}");
   if(version == "user"){
      if(regex_match(check, patternUser))
         return true;
      std::cout << ">> Username not valid" << std::endl;
   }
   else{
      if(regex_match(check, patternNum))
         return true;
      std::cout << ">> Invalid input" << std::endl;
   }
   return false;
}

//send verified Username to Server
void sendUser(int create_socket, char* buffer){
   fgets(buffer, BUF, stdin);
   while(!verify(buffer, "user")){
      printf(">> ");
      fgets(buffer, BUF, stdin);
   }
   int size = strlen(buffer);
   //buffer[size] = '\0';
   send(create_socket, buffer, size, 0);
}

int main(int argc, char **argv){
   int create_socket;
   char buffer[BUF];
   struct sockaddr_in address;
   int size;
   int isQuit;
   ////////////////////////////////////////////////////////////////////////////
   // CREATE A SOCKET
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1){
      perror("Socket error");
      return EXIT_FAILURE;
   }
   ////////////////////////////////////////////////////////////////////////////
   // INIT ADDRESS
   memset(&address, 0, sizeof(address)); // init storage with 0
   address.sin_family = AF_INET;         // IPv4
   address.sin_port = htons(PORT);
   if (argc < 2){
      inet_aton("127.0.0.1", &address.sin_addr);
   }
   else{
      inet_aton(argv[1], &address.sin_addr);
   }
   ////////////////////////////////////////////////////////////////////////////
   // CREATE A CONNECTION
   if (connect(create_socket,
               (struct sockaddr *)&address,
               sizeof(address)) == -1)
   {
      perror("Connect error - no server available");
      return EXIT_FAILURE;
   }
   // ignore return value of printf
   printf("Connection with server (%s) established\n",
          inet_ntoa(address.sin_addr));
   ////////////////////////////////////////////////////////////////////////////
   // RECEIVE DATA
   size = recv(create_socket, buffer, BUF - 1, 0);
   buffer[size] = '\0';
   if (size == -1){
      perror("recv error");
   }
   else if (size == 0){
      printf("Server closed remote socket\n"); // ignore error
   }
   else{
      buffer[size] = '\0';
      printf("%s", buffer); // ignore error
   }

   do{
      printf(">> ");
      if (fgets(buffer, BUF, stdin) != NULL){
         int size = strlen(buffer);
         // remove new-line signs from string at the end
         if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n'){
            size -= 2;
            buffer[size] = 0;
         }
         else if (buffer[size - 1] == '\n'){
            --size;
            buffer[size] = 0;
         }
         isQuit = strcmp(buffer, "QUIT") == 0;
         int tmpSize = strlen(buffer);
         char tmpBuffer[tmpSize];
         strcpy(tmpBuffer, buffer);
         //////////////////////////////////////////////////////////////////////
         // SENDING DATA
         // send will fail if connection is closed, but does not set
         // the error of send, but still the count of bytes sent
         if(strcmp(buffer, "LOGIN") == 0){
            send(create_socket, buffer, size, 0);
            printf("LDAP Username >> ");
            fgets(buffer, BUF, stdin);
            size = strlen(buffer);
            buffer[size-1] = '\0';
            send(create_socket, buffer, size, 0);
            printf("Password >> ");
            fgets(buffer, BUF, stdin);
            size = strlen(buffer);
            buffer[size-1] = '\0';
            send(create_socket, buffer, size, 0);
         }
         else if(strcmp(buffer, "SEND") == 0){
            send(create_socket, buffer, size, 0);
            int enterPress = 1;
            while(buffer[0] != '.'){
               switch(enterPress){
                  case 1: 
                     std::cout << "Sender >> ";
                     sendUser(create_socket, buffer);
                     enterPress++;
                     break;
                  case 2: 
                     std::cout << "Receiver >> ";
                     sendUser(create_socket, buffer);
                     enterPress++;
                     break; 
                  case 3: 
                     std::cout << "Subject >> ";
                     fgets(buffer, BUF, stdin); 
                     while(strlen(buffer) > 80){
                        std::cout << "Subject line too long. Max 80 characters allowed" << std::endl;
                        printf("Subject >> ");
                        fgets(buffer, BUF, stdin); 
                     }
                     size = strlen(buffer);
                     buffer[size-1] = '\0';
                     send(create_socket, buffer, size, 0);
                     enterPress++;
                     break;
                  default:
                     std::cout << "Message >> ";
                     fgets(buffer, BUF, stdin);
                     size = strlen(buffer);
                     send(create_socket, buffer, size, 0);
                     ////////// LONG MESSAGE NON FUNCTIONAL ////////////
                     /*std::cout << "Message >> ";
                     fgets(buffer, 5000, stdin);
                     size = strlen(buffer);
                     buffer[size-1] = '\0';
                     if(size > BUF){
                        char tempBuffer[BUF] = "\0";
                        int bottom = 0;
                        bool tooBig = true;
                        send(create_socket, "LONG_TRANSMISSION", 18, 0);
                        while(tooBig){
                           strncpy(tempBuffer, &buffer[bottom], BUF);
                           tempBuffer[BUF] = '\0';
                           size -= BUF;
                           bottom += BUF;
                           if(size < BUF){
                              if (send(create_socket, tempBuffer, size, 0) == -1){
                                 perror("send failed");
                                 return 0;
                              }
                              if (send(create_socket, "END_TRANSMISSION", 17, 0) == -1){
                                 perror("send failed");
                                 return 0;
                              }
                              tooBig = false;
                              break;
                           }
                           else{
                              send(create_socket, tempBuffer, BUF, 0);
                              send(create_socket, "KEEP_TRANSMISSION", 18, 0);
                           }
                        }
                     }
                     else{
                        printf("Start SHORT_TRANSMISSION\n");  
                        if (send(create_socket, "SHORT_TRANSMISSION", 19, 0) == -1){
                           perror("ST send failed");
                           return 0;
                        }
                        std::cout << "Send message: " << buffer << std::endl;
                        if (send(create_socket, buffer, size, 0) == -1){
                           perror("PKG send failed");
                           return 0;
                        }
                        printf("Start END_TRANSMISSION\n");             ///////////// LAST END TRANSMISSION DID NOT GET SENT OR RECEIVED
                        if (send(create_socket, "END_TRANSMISSION", 17, 0) == -1){
                           perror("ET send failed");
                           return 0;
                        }
                     }*/
                     break;
               }
            }
         }
         else if(strcmp(buffer, "LIST") == 0){
            send(create_socket, buffer, size, 0);
            printf("Username >> ");
            fgets(buffer, BUF, stdin);
            size = strlen(buffer);
            buffer[size-1] = '\0';
            send(create_socket, buffer, size, 0);
         }
         else if(strcmp(buffer, "READ") == 0 || strcmp(buffer, "DEL") == 0){
            send(create_socket, buffer, size, 0);
            for(int i = 0; i < 2; i++){
               i == 0 ? printf("Username >> ") : printf("Number >> ");
               fgets(buffer, BUF, stdin);
               size = strlen(buffer);
               buffer[size-1] = '\0';
               send(create_socket, buffer, size, 0);
            }
         }
         //////////////////////////////////////////////////////////////////////
         // RECEIVE FEEDBACK
         if(strcmp(tmpBuffer, "SEND") == 0 || strcmp(tmpBuffer, "LIST") == 0 || 
            strcmp(tmpBuffer, "READ") == 0 || strcmp(tmpBuffer, "DEL") == 0){
            size = recv(create_socket, buffer, BUF - 1, 0);
            buffer[size] = '\0';
            std::cout << "<< " << buffer << std::endl;
            if (size == -1){
               perror("recv error");
               break;
            }
            else if (size == 0){
               printf("Server closed remote socket\n"); // ignore error
               break;
            }
         }
         else if(strcmp(tmpBuffer, "QUIT") != 0){
            std::cout << "Unkown command" << std::endl;
         }
      }
   } while (!isQuit);

   ////////////////////////////////////////////////////////////////////////////
   // CLOSES THE DESCRIPTOR
   if (create_socket != -1){
      if (shutdown(create_socket, SHUT_RDWR) == -1){
         // invalid in case the server is gone already
         perror("shutdown create_socket"); 
      }
      if (close(create_socket) == -1){
         perror("close create_socket");
      }
      create_socket = -1;
   }
   return EXIT_SUCCESS;
}

