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

//send verified Number to Server
void sendNum(int create_socket, char* buffer){
   fgets(buffer, BUF, stdin);
   while(!verify(buffer, "num")){
      printf(">> ");
      fgets(buffer, BUF, stdin);
   }
   int size = strlen(buffer);
   //buffer[size-1] = '\0';
   send(create_socket, buffer, size, 0);
}

int main(int argc, char **argv)
{
   int create_socket;
   char buffer[BUF];
   struct sockaddr_in address;
   int size;
   int isQuit;

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A SOCKET
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // INIT ADDRESS
   memset(&address, 0, sizeof(address)); // init storage with 0
   address.sin_family = AF_INET;         // IPv4
   address.sin_port = htons(PORT);
   if (argc < 2)
   {
      inet_aton("127.0.0.1", &address.sin_addr);
   }
   else
   {
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
   if (size == -1)
   {
      perror("recv error");
   }
   else if (size == 0)
   {
      printf("Server closed remote socket\n"); // ignore error
   }
   else
   {
      buffer[size] = '\0';
      printf("%s", buffer); // ignore error
   }

   do
   {
      printf(">> ");
      if (fgets(buffer, BUF, stdin) != NULL)
      {
         int size = strlen(buffer);
         // remove new-line signs from string at the end
         if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
         {
            size -= 2;
            buffer[size] = 0;
         }
         else if (buffer[size - 1] == '\n')
         {
            --size;
            buffer[size] = 0;
         }
         isQuit = strcmp(buffer, "QUIT") == 0;

         //////////////////////////////////////////////////////////////////////
         // SEND DATA
         // send will fail if connection is closed, but does not set
         // the error of send, but still the count of bytes sent
         if(strcmp(buffer, "SEND") == 0){
            send(create_socket, buffer, size, 0);
            int enterPress = 1;
            //bool delimiterSent = false;
            while(buffer[0] != '.'){
               /*if(enterPress < 2)
               printf("Sender >> ");
               else if(enterPress <= 2)
               printf("Receiver >> ");
               fgets(buffer, BUF, stdin);
               if(enterPress <= 2){
                  while(!verify(buffer, "user")){
                     if(enterPress < 2)
                     printf("Sender >> ");
                     else
                     printf("Receiver >> ");
                     fgets(buffer, BUF, stdin);
                  }    
               }
               if(enterPress == 3){
                  printf("Subject >> ");
                  fgets(buffer, BUF, stdin); 
                  while(strlen(buffer) > 80){
                     std::cout << "Subject line too long. Max 80 characters allowed" << std::endl;
                     printf("Subject >> ");
                     fgets(buffer, BUF, stdin); 
                  }
               }
               if(enterPress > 3){
                  printf("Message >> ");
                  fgets(buffer, BUF, stdin);
               }
               if(!delimiterSent){
                  size = strlen(buffer);
                  send(create_socket, buffer, size, 0);   
               }
               if(buffer[0] == '.') 
                  delimiterSent = true;
               enterPress++;*/
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
                     send(create_socket, buffer, size, 0);
                     enterPress++;
                     break;
                  default:
                     std::cout << "Message >> ";
                     fgets(buffer, BUF, stdin);
                     size = strlen(buffer);
                     send(create_socket, buffer, size, 0);
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
            //sendUser(create_socket, buffer);
         }
         else if(strcmp(buffer, "READ") == 0 || strcmp(buffer, "DEL") == 0){
            send(create_socket, buffer, size, 0);
            sendUser(create_socket, buffer); 
            sendNum(create_socket, buffer);
         }
         if ((send(create_socket, buffer, size, 0)) == -1) 
         {
            perror("send error");
            break;
         }

         //////////////////////////////////////////////////////////////////////
         // RECEIVE FEEDBACK
         size = recv(create_socket, buffer, BUF - 1, 0);
         std::cout << "<< " << buffer << std::endl;
         if (size == -1)
         {
            perror("recv error");
            break;
         }
         else if (size == 0)
         {
            printf("Server closed remote socket\n"); // ignore error
            break;
         }
         else
         {
     // ignore error
            if (strcmp("OK", buffer) != 0)
            {
               fprintf(stderr, "<< Server error occured, abort\n");
               break;
            }
         }
      }
   } while (!isQuit);

   ////////////////////////////////////////////////////////////////////////////
   // CLOSES THE DESCRIPTOR
   if (create_socket != -1)
   {
      if (shutdown(create_socket, SHUT_RDWR) == -1)
      {
         // invalid in case the server is gone already
         perror("shutdown create_socket"); 
      }
      if (close(create_socket) == -1)
      {
         perror("close create_socket");
      }
      create_socket = -1;
   }

   return EXIT_SUCCESS;
}

