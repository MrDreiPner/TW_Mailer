#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <charconv>
#include <array>

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543

///////////////////////////////////////////////////////////////////////////////

int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;

///////////////////////////////////////////////////////////////////////////////

void *clientCommunication(void *data);
void signalHandler(int sig);

///////////////////////////////////////////////////////////////////////////////

int main(void)
{
   socklen_t addrlen;
   struct sockaddr_in address, cliaddress;
   int reuseValue = 1;

   ////////////////////////////////////////////////////////////////////////////
   // SIGNAL HANDLER
   // SIGINT (Interrup: ctrl+c)
   // https://man7.org/linux/man-pages/man2/signal.2.html
   if (signal(SIGINT, signalHandler) == SIG_ERR)
   {
      perror("signal can not be registered");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A SOCKET
   // https://man7.org/linux/man-pages/man2/socket.2.html
   // https://man7.org/linux/man-pages/man7/ip.7.html
   // https://man7.org/linux/man-pages/man7/tcp.7.html
   // IPv4, TCP (connection oriented), IP (same as client)
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error"); // errno set by socket()
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // SET SOCKET OPTIONS
   // https://man7.org/linux/man-pages/man2/setsockopt.2.html
   // https://man7.org/linux/man-pages/man7/socket.7.html
   // socket, level, optname, optvalue, optlen
   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEADDR,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reuseAddr");
      return EXIT_FAILURE;
   }

   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEPORT,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reusePort");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // INIT ADDRESS
   // Attention: network byte order => big endian
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(PORT);

   ////////////////////////////////////////////////////////////////////////////
   // ASSIGN AN ADDRESS WITH PORT TO SOCKET
   if (bind(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1)
   {
      perror("bind error");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // ALLOW CONNECTION ESTABLISHING
   // Socket, Backlog (= count of waiting connections allowed)
   if (listen(create_socket, 5) == -1)
   {
      perror("listen error");
      return EXIT_FAILURE;
   }

   while (!abortRequested)
   {
      /////////////////////////////////////////////////////////////////////////
      // ignore errors here... because only information message
      // https://linux.die.net/man/3/printf
      printf("Waiting for connections...\n");

      /////////////////////////////////////////////////////////////////////////
      // ACCEPTS CONNECTION SETUP
      // blocking, might have an accept-error on ctrl+c
      addrlen = sizeof(struct sockaddr_in);
      if ((new_socket = accept(create_socket,
                               (struct sockaddr *)&cliaddress,
                               &addrlen)) == -1)
      {
         if (abortRequested)
         {
            perror("accept error after aborted");
         }
         else
         {
            perror("accept error");
         }
         break;
      }

      /////////////////////////////////////////////////////////////////////////
      // START CLIENT
      // ignore printf error handling
      printf("Client connected from %s:%d...\n",
             inet_ntoa(cliaddress.sin_addr),
             ntohs(cliaddress.sin_port));
      clientCommunication(&new_socket); // returnValue can be ignored
      new_socket = -1;
   }

   // frees the descriptor
   if (create_socket != -1)
   {
      if (shutdown(create_socket, SHUT_RDWR) == -1)
      {
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

bool receiveMsgErrHandling(int size){
    if (size == -1)
      {
         if (abortRequested)
         {
            perror("recv error after aborted");
            return true;
         }
         else
         {
            perror("recv error");
            return true;
         }
      }

      if (size == 0)
      {
         printf("Client closed remote socket\n"); // ignore error
         return true;
      }
      return false;
}

// used to flush the buffer to prevent faulty access
void cleanBuffer (char* buffer){
   int buffSize = strlen(buffer);
   for(int i = 0; i<buffSize; i++){
      buffer[i] = '\0';
   }
}

void *clientCommunication(void *data)
{
   char buffer[BUF];
   int size;
   int *current_socket = (int *)data;

   ////////////////////////////////////////////////////////////////////////////
   // SEND welcome message
   strcpy(buffer, "Welcome to myserver!\r\nPlease enter your commands...\r\n");
   if (send(*current_socket, buffer, strlen(buffer), 0) == -1)
   {
      perror("send failed");
      return NULL;
   }
   cleanBuffer(buffer);
   do
   {
      /////////////////////////////////////////////////////////////////////////
      // RECEIVE
      size = recv(*current_socket, buffer, BUF - 1, 0);
      if(receiveMsgErrHandling(size)){ //returns true if an error has occured and ends the loop
        break;
      }
      // remove ugly debug message, because of the sent newline of client
      if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
      {
         size -= 2;
      }
      else if (buffer[size - 1] == '\n')
      {
         --size;
      }

      buffer[size] = '\0';
    printf("Message received: %s\n", buffer); 

    if(strcmp(buffer, "SEND") == 0){
         int state = 0;
         bool messageIncomplete = true;
         std::string UIDstr = "UID";
         char sender[8];
         char receiver[8];
         char subject[80];
         char message[BUF];
         printf("Waiting for data\n");
         while(messageIncomplete){
            size = recv(*current_socket, buffer, BUF - 1, 0);
            /////////////////////////////////////
            if(receiveMsgErrHandling(size)){ //returns true if an error has occured and ends the loop
                break;
            }
            // remove ugly debug message, because of the sent newline of client
            if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
            {
                size -= 2;
            }
            else if (buffer[size - 1] == '\n')
            {
                --size;
            }
            //////////////////////////////////////
            switch(state){
               case 0:  
                  strcpy(sender, buffer);
                  state++;
               break;
               case 1:  
                  strcpy(receiver, buffer);
                  state++;
               break;
               case 2:  
                  strcpy(subject, buffer);
                  state++;
               break;
               default:
                  if(message[0] == '\0')
                  strcpy(message, buffer);
                  else
                  strcat(message, buffer);
                  state++;
               break;
            }

            if(buffer[0] == '.'){
               printf("SEND message completed\n");
               std::ofstream file;
               std::ifstream checkFile;
               std::string line;
               int UID = 1;
               const char* type = "REC_";
               const char* txt =".txt";
               char* filename{ new char[strlen(type)+ strlen(receiver) + strlen(txt) + 1] };
               filename = strcpy(filename, type);
               filename = strcat(filename, receiver);
               filename = strcat(filename, txt);
               printf("Composed filename: %s\n", filename); 
               checkFile.open(filename);
               if(checkFile.is_open()){
                  if(checkFile.peek() != EOF){
                     while(getline (checkFile, line)){
                        if(line.substr(0,2) != UIDstr.substr(0,2)){
                        std::cout << line << std::endl;
                        }
                        if(line.substr(0,2) == UIDstr.substr(0,2)){
                           UID++;
                        }
                     }
                  }
               }
               else if(UID != 1){
                  std::cout << "Unable to open file " << filename << std::endl;
               }
               file.open(filename, std::ios_base::app);
               file << "\nUID: " << UID << "\nSender: " << sender << "\nReceiver: " << receiver << "\nSubject: " << subject << "\nMessage: " << message;
               file.close();
               messageIncomplete = false;
            }
            buffer[size] = '\0';
            printf("Message received in SEND command: %s\n", buffer); 
            cleanBuffer(buffer);
         }
      }
      if(strcmp(buffer, "LIST") == 0){
         std::ifstream file;
         std::string line;
         std::string allSubjects;
         std::string subjectStr = "Subject";
         int msgCount = 0;
         std::string msgCounterString;
         cleanBuffer(buffer);
         bool waiting = true;
         printf("Waiting for data\n");
         while(waiting){
            size = recv(*current_socket, buffer, BUF - 1, 0);
            /////////////////////////////////////
            if(receiveMsgErrHandling(size)){ //returns true if an error has occured and ends the loop
                  break;
            }
            // remove ugly debug message, because of the sent newline of client
            if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
            {
                  size -= 2;
            }
            else if (buffer[size - 1] == '\n')
            {
                  --size;
            }
            printf("Message received in LIST command: %s\n", buffer); 
            const char* type = "REC_";
            const char* txt =".txt";
            char* filename{ new char[strlen(type)+ strlen(buffer) + strlen(txt) + 1] };
            filename = strcpy(filename, type);
            filename = strcat(filename, buffer);
            filename = strcat(filename, txt);
            printf("Composed filename: %s\n", filename); 
            file.open(filename);
            if(file.is_open()){
               if(file.peek() != EOF){
                  while(getline (file, line)){
                     if(line.substr(0,6) == subjectStr.substr(0,6)){
                        line.erase(0,8);
                        msgCounterString = std::to_string(msgCount);
                        allSubjects = msgCounterString + ": " + line + "\n";
                        msgCount++;
                     }
                  }
               }
            }
            else{
               std::cout << "Unable to open file " << filename << std::endl;
            }
            if(msgCount > 0){
               msgCounterString = std::to_string(msgCount);
               allSubjects = msgCounterString + ": " + line + "\n";
               int size = allSubjects.size();
               char package[size+1];
               strcpy(package, allSubjects.c_str());
               if (send(*current_socket, package, size, 0) == -1)
               {
                  perror("send answer failed");
                  return NULL;
               }
               waiting = false;
            }
            else{
               if (send(*current_socket, "0", 2, 0) == -1)
               {
                  perror("send answer failed");
                  return NULL;
               }
               waiting = false;
            }
            cleanBuffer(buffer);
         }
         
      }
      if(strcmp(buffer, "READ") == 0){
         
      }

    //ignore error

      if (send(*current_socket, "OK", 3, 0) == -1)
      {
         perror("send answer failed");
         return NULL;
      }
   } while (strcmp(buffer, "quit") != 0 && !abortRequested);

   // closes/frees the descriptor if not already
   if (*current_socket != -1)
   {
      if (shutdown(*current_socket, SHUT_RDWR) == -1)
      {
         perror("shutdown new_socket");
      }
      if (close(*current_socket) == -1)
      {
         perror("close new_socket");
      }
      *current_socket = -1;
   }

   return NULL;
}

void signalHandler(int sig)
{
   if (sig == SIGINT)
   {
      printf("abort Requested... "); // ignore error
      abortRequested = 1;
      /////////////////////////////////////////////////////////////////////////
      // With shutdown() one can initiate normal TCP close sequence ignoring
      // the reference count.
      // https://beej.us/guide/bgnet/html/#close-and-shutdownget-outta-my-face
      // https://linux.die.net/man/3/shutdown
      if (new_socket != -1)
      {
         if (shutdown(new_socket, SHUT_RDWR) == -1)
         {
            perror("shutdown new_socket");
         }
         if (close(new_socket) == -1)
         {
            perror("close new_socket");
         }
         new_socket = -1;
      }

      if (create_socket != -1)
      {
         if (shutdown(create_socket, SHUT_RDWR) == -1)
         {
            perror("shutdown create_socket");
         }
         if (close(create_socket) == -1)
         {
            perror("close create_socket");
         }
         create_socket = -1;
      }
   }
   else
   {
      exit(sig);
   }
}
