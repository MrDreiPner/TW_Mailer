#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <bits/stdc++.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sstream>
#include <fstream>
#include <charconv>
#include <array>
#include <filesystem>
#include <ldap.h>
#include <vector>
#include <pthread.h>

//#include "TWM_Session.h"


///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
//#define PORT 6543

///////////////////////////////////////////////////////////////////////////////

int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;
std::vector<pthread_t*> threads;

///////////////////////////////////////////////////////////////////////////////

void signalHandler(int sig);
void *clientCommunication(void *data);

struct arg_struct{
   int socket;
   char *dataStore;
}typedef arg_struct;
///////////////////////////////////////////////////////////////////////////////

int main(int argc , char *argv[]){
   socklen_t addrlen;
   struct sockaddr_in address, cliaddress;
   int reuseValue = 1;
   arg_struct args;
   if(argc < 3){
      perror("Too few arguments");
      return EXIT_FAILURE;
   }
   int PORT = std::strtol(argv[1], nullptr, 10);
   args.dataStore = new char[sizeof(argv[2])];
   args.dataStore = argv[2];
   std::cout << "passed PORT:" << PORT << "\nFile location = " << args.dataStore << std::endl;
   ////////////////////////////////////////////////////////////////////////////
   // SIGNAL HANDLER
   if (signal(SIGINT, signalHandler) == SIG_ERR){
      perror("signal can not be registered");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A SOCKET
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1){
      perror("Socket error"); // errno set by socket()
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // SET SOCKET OPTIONS
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
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(PORT);

   ////////////////////////////////////////////////////////////////////////////
   // ASSIGN AN ADDRESS WITH PORT TO SOCKET
   if (bind(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1){
      perror("bind error");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // ALLOW CONNECTION ESTABLISHING
   if (listen(create_socket, 5) == -1){
      perror("listen error");
      return EXIT_FAILURE;
   }

   while (!abortRequested){
      /////////////////////////////////////////////////////////////////////////
      // ignore errors here... because only information message
      printf("Waiting for connections...\n");

      /////////////////////////////////////////////////////////////////////////
      // ACCEPTS CONNECTION SETUP
      addrlen = sizeof(struct sockaddr_in);
      //while(1){
         if ((new_socket = accept(create_socket,(struct sockaddr *)&cliaddress,&addrlen)) == -1)
         {
            if (abortRequested){
               perror("accept error after aborted");
               break;
            }
            else{
               perror("accept error");
               break;
            }
         }
         else{
            printf("Client connected from %s:%d...\n", inet_ntoa(cliaddress.sin_addr), ntohs(cliaddress.sin_port));
            args.socket = new_socket;
            pthread_t *newThread = new pthread_t();
            if(pthread_create(newThread, NULL, clientCommunication, (void *)&args) == 0){
               threads.push_back(newThread);
               pthread_detach(*newThread);
               
               std::cout <<"Server is thread numba: " << pthread_self() << std::endl;
            }
            //shutdown(new_socket, SHUT_RDWR);
         }
      //}


      /////////////////////////////////////////////////////////////////////////
      // START CLIENT
      // ignore printf error handling
      // printf("Client connected from %s:%d...\n",
      //        inet_ntoa(cliaddress.sin_addr),
      //        ntohs(cliaddress.sin_port));
      // clientCommunication(&new_socket); // returnValue can be ignored
      // new_socket = -1;
   }

   // frees the descriptor
   // if (create_socket != -1){
   //    if (shutdown(create_socket, SHUT_RDWR) == -1)
   //       perror("shutdown create_socket");
   //    if (close(create_socket) == -1)
   //       perror("close create_socket");
   //    create_socket = -1;
   // }
   return EXIT_SUCCESS;
}

bool receiveMsgErrHandling(int size){
   if (size == -1){
      if (abortRequested){
         perror("recv error after aborted");
         return true;
      }
      else{
         perror("recv error");
         return true;
      }
   }
   if (size == 0){
      printf("Client closed remote socket\n"); // ignore error
      return true;
   }
   return false;
}

// used to flush the buffer to prevent faulty access
void cleanBuffer (char* buffer){
   int buffSize = strlen(buffer);
   for(int i = 0; i<=buffSize; i++){
      buffer[i] = '\0';
   }
}

void *clientCommunication(void* data){
   char buffer[BUF];
   int size;
   int failedLoginCount = 3;
   bool verified = false;
   char username[9];
   arg_struct *args = (arg_struct *) data;
   int current_socket = args->socket;
   char* storageLocation = args->dataStore;

   ////////////////////////////////////////////////////////////////////////////
   // SEND welcome message
   std::cout <<"This is thread numba: " << pthread_self() << std::endl;
   std::cout <<"passed storage location = " << storageLocation << std::endl;
   strcpy(buffer, "Welcome to myserver!\r\nPlease enter your commands...\r\n");
   if (send(current_socket, buffer, strlen(buffer), 0) == -1){
      perror("send failed");
      return NULL;
   }
   cleanBuffer(buffer);
   do{
      /////////////////////////////////////////////////////////////////////////
      // RECEIVE
      cleanBuffer(buffer);
      size = recv(current_socket, buffer, BUF - 1, 0);
      if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
         break;
      buffer[size] = '\0';
      printf("Message received: %s\n", buffer);
      //LOGIN 
      if(strcmp(buffer, "LOGIN") == 0){
         const char *ldapUri = "ldap://ldap.technikum-wien.at:389";
         const int ldapVersion = LDAP_VERSION3;
         //receive username
         cleanBuffer(buffer);
         size = recv(current_socket, buffer, BUF - 1, 0);
         if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
            break;
         buffer[size] = '\0';
         char ldapBindUser[256];
         strcpy(username, buffer);
         sprintf(ldapBindUser, "uid=%s,ou=people,dc=technikum-wien,dc=at", username);
         printf("user set to: %s\n", ldapBindUser);
         //receive password
         cleanBuffer(buffer);
         size = recv(current_socket, buffer, BUF - 1, 0);
         if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
            break;
         buffer[size] = '\0';
         char ldapBindPassword[256];
         strcpy(ldapBindPassword, buffer);
         printf("pw taken over from commandline\n");
         // setup LDAP connection
         LDAP *ldapHandle;
         int rc = ldap_initialize(&ldapHandle, ldapUri);
         if (rc != LDAP_SUCCESS){
            fprintf(stderr, "ldap_init failed\n");
            continue;
         }
         printf("connected to LDAP server %s\n", ldapUri);
         rc = ldap_set_option(
            ldapHandle,
            LDAP_OPT_PROTOCOL_VERSION, // OPTION
            &ldapVersion);             // IN-Value
         if (rc != LDAP_OPT_SUCCESS){
            fprintf(stderr, "ldap_set_option(PROTOCOL_VERSION): %s\n", ldap_err2string(rc));
            ldap_unbind_ext_s(ldapHandle, NULL, NULL);
            continue;
         }
         //start secure connection
         rc = ldap_start_tls_s(
         ldapHandle,
         NULL,
         NULL);
         if (rc != LDAP_SUCCESS){
            fprintf(stderr, "ldap_start_tls_s(): %s\n", ldap_err2string(rc));
            ldap_unbind_ext_s(ldapHandle, NULL, NULL);
            continue;
         }
         //bind credentials
         BerValue bindCredentials;
         bindCredentials.bv_val = (char *)ldapBindPassword;
         bindCredentials.bv_len = strlen(ldapBindPassword);
         BerValue *servercredp; // server's credentials
         rc = ldap_sasl_bind_s(
            ldapHandle,
            ldapBindUser,
            LDAP_SASL_SIMPLE,
            &bindCredentials,
            NULL,
            NULL,
            &servercredp);
         if (rc != LDAP_SUCCESS){
            fprintf(stderr, "LDAP bind error: %s\n", ldap_err2string(rc));
            failedLoginCount--;
            if(failedLoginCount == 0){
               //IMPLEMENT BLACKLIST
            }
            send(current_socket, "ERR", 4, 0);
            ldap_unbind_ext_s(ldapHandle, NULL, NULL);
            continue;
         }
         if (send(current_socket, "OK", 3, 0) == -1){
            perror("send answer failed");
            return NULL;
         }
         verified = true;
      }
      if(strcmp(buffer, "SEND") == 0){       //SEND Command Handling
         if(verified){
            int state = 0;
            bool messageIncomplete = true;
            char sender[9];
            strcpy(sender, username);
            char receiver[9] = "\0";
            char subject[81] = "\0";
            char message[BUF+1] = "\0";
            printf("Waiting for data\n");
            while(messageIncomplete){
               cleanBuffer(buffer);
               size = recv(current_socket, buffer, BUF - 1, 0);
               if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
                  break;
               buffer[size] = '\0';
               switch(state){
                  case 0:  //Receive Receiver
                     strcpy(receiver, buffer);
                     state++;
                     break;
                  case 1:  //Receive Subject
                     strcpy(subject, buffer);
                     state++;
                     break;
                  case 2:  //Receive Message
                     if(strcmp(message,"\0") == 0 && !(buffer[0] == '.'))
                        strcpy(message, buffer);
                     else if(!(buffer[0] == '.')){
                        //printf("if . i should not be here --> content is %s", buffer);
                        strcat(message, buffer);
                     }
                     else
                        state++;
                     ////////// LONG MESSAGE NON FUNCTIONAL
                     /*bool longTransmission = false;
                     if(strcmp(buffer,"LONG_TRANSMISSION") == 0){
                        printf("Start LONG_TRANSMISSION\n");
                        longTransmission = true;
                        while(longTransmission){
                           size = recv(*current_socket, buffer, BUF - 1, 0);
                           if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
                              break;
                           buffer[size] = '\0';
                           if(strcmp(message,"\0") == 0 && !(buffer[0] == '.'))
                              strcpy(message, buffer);
                           else if(!(buffer[0] == '.')){
                              strcat(message, buffer);
                           }
                           else{
                              state++;
                           }

                           size = recv(*current_socket, buffer, BUF - 1, 0);
                           if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
                              break;
                           buffer[size] = '\0';
                           std::cout << buffer << std::endl;
                           if(strcmp(buffer,"END_TRANSMISSION") == 0){
                              longTransmission = false;
                           }
                        }
                     }
                     else{   
                        printf("Start SHORT_TRANSMISSION\n");  
                        size = recv(*current_socket, buffer, BUF - 1, 0);
                        if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
                           break;
                        buffer[size] = '\0';
                        printf("Message received in SHORT_SEND: %s\n", buffer);
                        if(strcmp(message,"\0") == 0 && !(buffer[0] == '.'))
                           strcpy(message, buffer);
                        else if(!(buffer[0] == '.')){
                           strcat(message, buffer);
                        }
                        else if(buffer[0] == '.'){
                           state++;
                        }
                        printf("Current message: %s\n", message);
                        cleanBuffer(buffer);
                        size = recv(*current_socket, buffer, BUF, 0);
                        if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
                           break;
                        buffer[size] = '\0';
                        printf("END_TRANSMISSION = %s\n", buffer); ///////////// LAST END TRANSMISSION DID NOT GET SENT OR RECEIVED
                        cleanBuffer(buffer);
                     }*/
                     break;
               } //Putting Received data into .txt file
               printf("Message received in SEND command: %s\n", buffer);
               if(state > 2){
                  printf("SEND message completed\n");
                  std::ofstream file;
                  size = strlen(receiver);
                  char path[size];
                  strcpy(path,receiver);
                  mkdir(path,0777);
                  const char* txt =".txt\0";
                  char* filename{ new char[strlen(path) + 1 + strlen(subject) + strlen(txt) + 1] };
                  filename = strcpy(filename, path);
                  filename = strcat(filename, "/");
                  filename = strcat(filename, subject);
                  filename = strcat(filename, txt);
                  printf("Composed filename: %s\n", filename); 
                  file.open(filename, std::ios_base::app);
                  file << "Sender: " << sender << "\nReceiver: " << receiver << "\nSubject: " << subject << "\nMessage: \n" << message << std::endl;
                  file.close();
                  messageIncomplete = false;
                  if (send(current_socket, "OK", 3, 0) == -1){
                     perror("send answer failed");
                     return NULL;
                  }
               }
               cleanBuffer(buffer);
            } 
         }
         else{
            if (send(current_socket, "Not logged in", 14, 0) == -1){
               perror("send answer failed");
               return NULL;
            }
         }

      } //LIST Command Handling
      if(strcmp(buffer, "LIST") == 0){
         if(verified){
            cleanBuffer(buffer);
            std::ifstream file;
            std::string allSubjects = "\0";
            int msgCount = 0;
            std::string msgCounterString = "\0";
            bool waiting = true;
            printf("Waiting for data\n");
            while(waiting){
               size = recv(current_socket, buffer, BUF - 1, 0);
               if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
                  break;
               buffer[size] = '\0';
               printf("Message received in LIST command: %s\n", buffer); 
               char* directory{ new char[strlen(buffer) + 1 + 1] };
               directory = strcpy(directory, buffer);
               directory = strcat(directory, "/");
               cleanBuffer(buffer);
               if(std::filesystem::exists(directory)){ //Looking for requested Directory
                  for(const auto & entry : std::filesystem::directory_iterator(directory)){
                     msgCount++;
                     msgCounterString = std::to_string(msgCount);
                     std::string entryString = entry.path();
                     entryString = entryString.substr(entryString.find_last_of("/")+1, entryString.find_last_of('\n')-4);
                     std::cout << msgCount << ": " << entryString + "\n";
                     allSubjects += msgCounterString + ":" + entryString;
                  }
                  if(msgCount > 0){ //Send List to Client
                     msgCounterString = std::to_string(msgCount);
                     allSubjects = "Message count: " + msgCounterString + "\n" + allSubjects;
                     int size = allSubjects.size();
                     char package[size+1];
                     strcpy(package, allSubjects.c_str());
                     if(send(current_socket, package, size, 0) == -1){
                        perror("send answer failed");
                        return NULL;
                     }
                     cleanBuffer(package);
                  }
                  else{ //No List to be sent
                     if(send(current_socket, "0", 2, 0) == -1){
                        perror("send answer failed");
                        return NULL;
                     }
                  }
                  waiting = false;
               }
               else{ //No List to be sent
                  if(send(current_socket, "0", 2, 0) == -1){
                     perror("send answer failed");
                     return NULL;
                  }
                  waiting = false;
               }
            }  
         }
         else{
            if (send(current_socket, "Not logged in", 14, 0) == -1){
               perror("send answer failed");
               return NULL;
            }
         }
      } //READ Command Handling
      if(strcmp(buffer, "READ") == 0){
         if(verified){
            cleanBuffer(buffer);
            std::ifstream file;
            std::string line = "\0";
            std::string allText = "\0";
            std::string msgCounterString = "\0";
            int msgCount = 0;
            bool waiting = true;
            printf("Waiting for data\n");
            while(waiting){
               size = recv(current_socket, buffer, BUF - 1, 0);
               if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
                  break;
               buffer[size] = '\0';
               printf("Message received in READ command: %s\n", buffer); 
               size = strlen(buffer);
               char username[size];
               strcpy(username, buffer);
               char* directory{ new char[strlen(username) + 1 + 1] };
               directory = strcpy(directory, username);
               directory = strcat(directory, "/");
               cleanBuffer(buffer);
               size = recv(current_socket, buffer, BUF - 1, 0);
               if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
                  break;
               buffer[size] = '\0';          
               int index = atoi(buffer);
               printf("Number received in READ command: %d\n", index); 
               cleanBuffer(buffer);
               std::string entryString = "\0";
               if(std::filesystem::exists(directory)){ //Looking for requested Directory
                  for(const auto & entry : std::filesystem::directory_iterator(directory)){
                     msgCount++;
                     if(msgCount == index){
                        entryString = entry.path();
                        printf("File found!\n");
                        break;
                     }
                  }
                  if(msgCount < index){
                     if(send(current_socket, "ERR", 5, 0) == -1){
                        perror("send answer failed");
                        return NULL;
                     }
                  }
                  file.open(entryString);
                  if(file.is_open()){
                     while(getline(file, line)){
                        allText += line + "\n";
                     }
                     printf("Copying file content!\n");
                     allText = "OK\n" + allText;
                     int size = allText.size();
                     char package[size+1];
                     strcpy(package, allText.c_str()); //Send requested Message to Client
                     if(send(current_socket, package, size, 0) == -1){
                        perror("send answer failed");
                        return NULL;
                     }
                     cleanBuffer(package);
                  }
                  else
                     std::cout << "Unable to open file\n";
               }
               else{
                  if(send(current_socket, "ERR", 5, 0) == -1){
                     perror("send answer failed");
                     return NULL;
                  }
               }
               waiting = false;
            }
         }
         else{
            if (send(current_socket, "Not logged in", 14, 0) == -1){
               perror("send answer failed");
               return NULL;
            }
         } 
      } //DEL Command Handling
      if(strcmp(buffer, "DEL") == 0){
         if(verified){
            cleanBuffer(buffer);
            std::ifstream file;
            std::string line = "\0";
            std::string allText = "\0";
            int msgCount = 0;
            std::string msgCounterString = "\0";
            bool waiting = true;
            printf("Waiting for data\n");
            while(waiting){
               size = recv(current_socket, buffer, BUF - 1, 0);
               if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
                  break;
               buffer[size] = '\0';
               printf("Message received in DEL command: %s\n", buffer); 
               size = strlen(buffer);
               char username[size];
               strcpy(username, buffer);
               char* directory{ new char[strlen(username) + 1 + 1] };
               directory = strcpy(directory, username);
               directory = strcat(directory, "/");
               cleanBuffer(buffer);
               size = recv(current_socket, buffer, BUF - 1, 0); 
               if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
                  break;
               buffer[size] = '\0';  
               int index = atoi(buffer);
               printf("Number received in DEL command: %d\n", index); 
               cleanBuffer(buffer);
               std::string entryString = "\0";
               if(std::filesystem::exists(directory)){
                  for(const auto & entry : std::filesystem::directory_iterator(directory)){
                     msgCount++;
                     if(msgCount == index){ //remove file if found
                        entryString = entry.path();
                        std::filesystem::remove(entryString); 
                        if (send(current_socket, "OK", 3, 0) == -1){
                           perror("send answer failed");
                           return NULL;
                        }
                        break;
                     }
                  }
                  if(msgCount < index){
                     if(send(current_socket, "ERR", 4, 0) == -1){
                        perror("send answer failed");
                        return NULL;
                     }
                  } 
               
               }
               else{
                  if(send(current_socket, "ERR", 4, 0) == -1){
                     perror("send answer failed");
                     return 0;
                  }
               }
               waiting = false;
            }
         }
         else{
            if (send(current_socket, "Not logged in", 14, 0) == -1){
               perror("send answer failed");
               return NULL;
            }
         } 
      }
    //ignore error
   } while (strcmp(buffer, "QUIT") != 0 && !abortRequested);
   cleanBuffer(buffer);
   // closes/frees the descriptor if not already
   if(current_socket != -1){
      if(shutdown(current_socket, SHUT_RDWR) == -1)
         perror("shutdown new_socket");
      if(close(current_socket) == -1)
         perror("close new_socket");
      current_socket = -1;
   }
   return NULL;
}

void signalHandler(int sig){
   if(sig == SIGINT){
      printf("abort Requested... "); // ignore error
      abortRequested = 1;
      /////////////////////////////////////////////////////////////////////////
      // With shutdown() one can initiate normal TCP close sequence ignoring
      // the reference count.
      if (new_socket != -1){
         if (shutdown(new_socket, SHUT_RDWR) == -1)
            perror("shutdown new_socket");
         if (close(new_socket) == -1)
            perror("close new_socket");
         new_socket = -1;
      }
      if (create_socket != -1){
         if (shutdown(create_socket, SHUT_RDWR) == -1)
            perror("shutdown create_socket");
         if (close(create_socket) == -1)
            perror("close create_socket");
         create_socket = -1;
      }
   }
   else
      exit(sig);
}