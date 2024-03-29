// #include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
// #include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
// #include <bits/stdc++.h>
#include <iostream>
#include <sys/stat.h>
// #include <sys/types.h>
// #include <sstream>
#include <fstream>
// #include <charconv>
// #include <array>
#include <filesystem>
#include <ldap.h>
// #include <vector>
// #include <pthread.h>
#include <dirent.h>
// #include <ctime>
// #include <iomanip>


///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define BL_TIMEOUT 60

///////////////////////////////////////////////////////////////////////////////

int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;
const std::string path = "Blacklist/";
pthread_mutex_t lock;

///////////////////////////////////////////////////////////////////////////////

void signalHandler(int sig);
void *clientCommunication(void *data);
void blacklistEntry(char* ipAddr);
bool blacklisted(char* ipAddr);
void *blacklistUpkeep(void*);
int sendERR(int current_socket);
int sendOK(int current_socket);

struct arg_struct{
   int socket;
   char *dataStore;
   struct sockaddr_in* pV4Addr;
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
   pthread_t *blWorkerThread = new pthread_t(); //Thread to make upkeep of blacklist
   if(pthread_create(blWorkerThread, NULL, blacklistUpkeep, NULL) == 0){
      pthread_detach(*blWorkerThread);
      std::cout <<"Server is thread number: " << pthread_self() << std::endl;
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
         else{ //Create Thread for each new Client connection
            printf("Client connected from %s:%d...\n", inet_ntoa(cliaddress.sin_addr), ntohs(cliaddress.sin_port));
            args.pV4Addr = (struct sockaddr_in*)&cliaddress;
            args.socket = new_socket;
            pthread_t *newThread = new pthread_t();
            if(pthread_create(newThread, NULL, clientCommunication, (void *)&args) == 0){
               pthread_detach(*newThread);
               std::cout <<"Server is thread number: " << pthread_self() << std::endl;
            }
            //shutdown(new_socket, SHUT_RDWR);
         }
      //}
   }
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
   signed int failedLoginCount = 3;
   bool verified = false;
   char username[9];
   arg_struct *args = (arg_struct *) data;
   int current_socket = args->socket;
   char* storageLocation = args->dataStore;
   struct in_addr ipAddr = args->pV4Addr->sin_addr;
   char clientIPstr[INET_ADDRSTRLEN];
   inet_ntop( AF_INET, &ipAddr, clientIPstr, INET_ADDRSTRLEN );
   // std::cout <<"This is thread number: " << pthread_self() << std::endl;
   // std::cout <<"passed storage location = " << storageLocation << std::endl;
   // std::cout <<"passed clientIPadress = " << clientIPstr << std::endl;

   ////////////////////////////////////////////////////////////////////////////
   // SEND welcome message
   strcpy(buffer, "Welcome to myserver!\r\nPlease enter your commands...\r\n");
   if (send(current_socket, buffer, strlen(buffer), 0) == -1){
      perror("send failed");
      return NULL;
   }
   cleanBuffer(buffer);
   do{ //Handle Client Commands
      /////////////////////////////////////////////////////////////////////////
      // RECEIVE
      cleanBuffer(buffer);
      printf("Waiting for input\n");
      size = recv(current_socket, buffer, BUF - 1, 0);
      if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
         break;
      buffer[size] = '\0';
      printf("Message received: %s\n", buffer);
      //LOGIN 
      if(strcmp(buffer, "LOGIN") == 0){      //LOGIN Command Handling
         const char *ldapUri = "ldap://ldap.technikum-wien.at:389";
         const int ldapVersion = LDAP_VERSION3;
         cleanBuffer(buffer);
         size = recv(current_socket, buffer, BUF - 1, 0);   //receive username
         if(receiveMsgErrHandling(size))  //returns true if an error has occured and ends the loop
            break;
         buffer[size] = '\0';
         char ldapBindUser[256];
         strcpy(username, buffer);
         sprintf(ldapBindUser, "uid=%s,ou=people,dc=technikum-wien,dc=at", username);
         printf("user set to: %s\n", ldapBindUser);
         cleanBuffer(buffer);
         size = recv(current_socket, buffer, BUF - 1, 0);   //receive password
         if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
            break;
         buffer[size] = '\0';
         char ldapBindPassword[256];
         strcpy(ldapBindPassword, buffer);
         // // ("pw taken over from commandline\n");
         LDAP *ldapHandle;
         int rc = ldap_initialize(&ldapHandle, ldapUri);    // setup LDAP connection
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
         rc = ldap_start_tls_s(        //start secure connection
         ldapHandle,
         NULL,
         NULL);
         if (rc != LDAP_SUCCESS){
            fprintf(stderr, "ldap_start_tls_s(): %s\n", ldap_err2string(rc));
            ldap_unbind_ext_s(ldapHandle, NULL, NULL);
            continue;
         }
         BerValue bindCredentials;     //bind credentials
         bindCredentials.bv_val = (char *)ldapBindPassword;
         bindCredentials.bv_len = strlen(ldapBindPassword);
         BerValue *servercredp;        //server's credentials
         rc = ldap_sasl_bind_s(
            ldapHandle,
            ldapBindUser,
            LDAP_SASL_SIMPLE,
            &bindCredentials,
            NULL,
            NULL,
            &servercredp);
         if(blacklisted(clientIPstr)){    //if IP is blacklisted, block user from Login
            if(send(current_socket, "ERR - Blacklist", 16, 0) == -1){
               perror("send answer failed");
               return NULL;
            }
            continue;
         }
         else if (rc != LDAP_SUCCESS){    //Send ERR if Wrong Credentials
            fprintf(stderr, "LDAP bind error: %s\n", ldap_err2string(rc));
            failedLoginCount--;
            send(current_socket, "ERR", 4, 0);
            ldap_unbind_ext_s(ldapHandle, NULL, NULL);
            if(failedLoginCount == 0){
               blacklistEntry(clientIPstr);
               failedLoginCount = 3;
               printf("entry made to blacklist\n");
            }
            continue;
         }
         if (sendOK(current_socket)){ //Send OK if Login successful
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
                        // printf("if . i should not be here --> content is %s", buffer);
                        strcat(message, buffer);
                     }
                     else
                        state++;
                     break;
               } //Putting Received data into .txt file
               printf("Message received in SEND command: %s\n", buffer);
               if(state > 2){
                  printf("SEND message completed\n");
                  std::ofstream file;
                  mkdir(storageLocation,0777);
                  char* path{ new char[strlen(storageLocation) + 1 + strlen(sender)] };   //compose Path for Sender Folder
                  path = strcpy(path, storageLocation);
                  path = strcat(path, "/");
                  path = strcat(path, sender);
                  mkdir(path,0777);
                  char* subPath{ new char[strlen(path) + 1 + strlen(receiver)] };         //compose Path for receiver Folder
                  subPath = strcpy(subPath, path);
                  subPath = strcat(subPath, "/");
                  subPath = strcat(subPath, receiver);
                  mkdir(subPath,0777);
                  const char* txt =".txt\0";
                  char* filename{ new char[strlen(subPath) + 1 + strlen(subject) + strlen(txt) + 1] };   //compose Path for Message File
                  filename = strcpy(filename, subPath);
                  filename = strcat(filename, "/");
                  filename = strcat(filename, subject);
                  filename = strcat(filename, txt);
                  printf("Composed filename: %s\n", filename); 
                  struct stat buffer;
                  int numerator = 0;
                  while(stat(filename, &buffer) == 0){      //composes Path for Message if Message with same Subject already exists
                     numerator++;
                     std::string numStr = std::to_string(numerator);
                     filename = strcpy(filename, subPath);
                     filename = strcat(filename, "/");
                     filename = strcat(filename, subject);
                     filename = strcat(filename, numStr.c_str());
                     filename = strcat(filename, txt);
                     printf("Alternate Composed filename: %s\n", filename);
                  }
                  file.open(filename, std::ios_base::app);     //fill file with message
                  file << "Sender: " << sender << "\nReceiver: " << receiver << "\nSubject: " << subject << "\nMessage: \n" << message << std::endl;
                  file.close();
                  messageIncomplete = false;
                  if (sendOK(current_socket)){
                     perror("send answer failed");
                     return NULL;
                  }
                  delete[] path;
                  delete[] subPath;
                  delete[] filename;
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

      }
      if(strcmp(buffer, "LIST") == 0){       //LIST Command Handling
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
               if(strcmp(buffer, "OUT") == 0){
                  char* directory{ new char[strlen(storageLocation) + 1 + strlen(username) + 1 + 1] };
                  directory = strcpy(directory, storageLocation);
                  directory = strcat(directory, "/");
                  directory = strcat(directory, username);
                  directory = strcat(directory, "/");
                  printf("Composed directory: %s\n", directory);
                  if(std::filesystem::exists(directory)){ //Starting to look in user folders
                     struct dirent *dir;
                     DIR *openDIR = opendir(directory);
                     for(dir = readdir(openDIR); dir != NULL; dir = readdir(openDIR)){ //iterating through all receiver folders
                        std::string receiver = dir->d_name;
                        std::string type = directory + receiver;
                        if((type.substr(type.find_last_of("/")+1, type.find_last_of('\0')-1) != ".") 
                        && (type.substr(type.find_last_of("/")+1, type.find_last_of('\0')-1) != "..")){  // excluding root
                           printf("Next Composed directory: %s\n", type.c_str());
                           for(const auto & entry : std::filesystem::directory_iterator(type.c_str())){     // reading all entries and enumerating
                              msgCount++;
                              msgCounterString = std::to_string(msgCount);
                              std::string entryString = entry.path();
                              entryString = entryString.substr(entryString.find_last_of("/")+1, entryString.find_last_of('\n')-4);
                              // std::cout << msgCount << ": Receiver: " << receiver << " | Subject: " << entryString + "\n";
                              allSubjects += msgCounterString + ": Receiver: " + receiver + " | Subject: " + entryString + "\n";
                           }
                        }
                     }
                     if(msgCount > 0){ //Send List to Client
                        msgCounterString = std::to_string(msgCount);
                        allSubjects = "Message count: " + msgCounterString + "\n" + allSubjects;
                        int size = allSubjects.size();
                        char package[size+1];
                        strcpy(package, allSubjects.c_str());
                        printf("Messages: %s\n", package);
                        if(send(current_socket, package, size, 0) == -1){
                           perror("send answer failed");
                           return NULL;
                        }
                        cleanBuffer(package);
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
                  else{ //No List to be sent
                     if(send(current_socket, "0", 2, 0) == -1){
                        perror("send answer failed");
                        return NULL;
                     }
                     waiting = false;
                  }
                  delete[] directory;
               }
               else if(strcmp(buffer, "IN") == 0){
                  char* directory{ new char[strlen(storageLocation) + 1  + 1] };
                  directory = strcpy(directory, storageLocation);
                  directory = strcat(directory, "/");
                  printf("Composed directory: %s\n", directory);
                  if(std::filesystem::exists(directory)){ //Starting to look through storage folder
                     struct dirent *dir;
                     DIR *openDIR = opendir(directory);
                     for(dir = readdir(openDIR); dir != NULL; dir = readdir(openDIR)){ //iterating through all sender folders
                        std::string sender = dir->d_name;
                        std::string type = directory + sender;
                        printf("Next Composed directory: %s\n", type.c_str());
                        if((type.substr(type.find_last_of("/")+1, type.find_last_of('\0')-1) != ".") 
                        && (type.substr(type.find_last_of("/")+1, type.find_last_of('\0')-1) != "..")){
                           for(const auto & entry : std::filesystem::directory_iterator(type.c_str())){ //iterating through receiver folders inside sender folders
                              std::string entryString = entry.path();
                              entryString = entryString.substr(entryString.find_last_of("/")+1, entryString.find_last_of('\n')-4);
                              if(strcmp(username,entryString.c_str()) == 0){
                                 std::string subPath = type + "/" + entryString + "/";
                                 for(const auto & subEntry : std::filesystem::directory_iterator(subPath.c_str())){ // reading all entries and enumerating
                                    msgCount++;
                                    msgCounterString = std::to_string(msgCount);
                                    std::string subEntryString = subEntry.path();
                                    subEntryString = subEntryString.substr(subEntryString.find_last_of("/")+1, subEntryString.find_last_of('\n')-4);
                                    // std::cout << "Sender: " << sender << " | Receiver: " << subEntryString + "\n";
                                    allSubjects += msgCounterString + ": Sender: " + sender + " | Subject: " + subEntryString + "\n";
                                 }
                              }
                           }
                        }
                     }
                     if(msgCount > 0){ //Send List to Client
                        msgCounterString = std::to_string(msgCount);
                        allSubjects = "Message count: " + msgCounterString + "\n" + allSubjects;
                        int size = allSubjects.size();
                        char package[size+1];
                        strcpy(package, allSubjects.c_str());
                        printf("Messages: %s\n", package);
                        if(send(current_socket, package, size, 0) == -1){
                           perror("send answer failed");
                           return NULL;
                        }
                        cleanBuffer(package);
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
                  else{ //No List to be sent
                     if(send(current_socket, "0", 2, 0) == -1){
                        perror("send answer failed");
                        return NULL;
                     }
                     waiting = false;
                  }
                  delete[] directory;
               }
               else{
                  if(sendERR(current_socket)){
                     perror("send answer failed");
                     return NULL;
                  }
               }
               //char* directory{ new char[strlen(buffer) + 1 + 1] };
               cleanBuffer(buffer);
            }  
         }
         else{
            if (send(current_socket, "Not logged in", 14, 0) == -1){
               perror("send answer failed");
               return NULL;
            }
         }
      }
      if(strcmp(buffer, "READ") == 0){       //READ Command Handling
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
               if(strcmp(buffer, "OUT") == 0){
                  char* directory{ new char[strlen(storageLocation) + 1 + strlen(username) + 1 + 1] };
                  directory = strcpy(directory, storageLocation);
                  directory = strcat(directory, "/");
                  directory = strcat(directory, username);
                  directory = strcat(directory, "/");
                  printf("Composed directory: %s\n", directory);
                  size = recv(current_socket, buffer, BUF - 1, 0);
                  if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
                     break;
                  buffer[size] = '\0';          
                  int index = atoi(buffer);
                  printf("Number received in READ command: %d\n", index); 
                  if(std::filesystem::exists(directory)){ //Looking for requested Directory
                     bool fileFound = false;
                     struct dirent *dir;
                     DIR *openDIR = opendir(directory);
                     std::string type;
                     std::string entryPath;
                     for(dir = readdir(openDIR); dir != NULL; dir = readdir(openDIR)){ //iterating through receiver folders
                        type = dir->d_name;
                        type = directory + type;
                        if((type.substr(type.find_last_of("/")+1, type.find_last_of('\0')-1) != ".") 
                        && (type.substr(type.find_last_of("/")+1, type.find_last_of('\0')-1) != "..")){
                           printf("Next Composed directory: %s\n", type.c_str());
                           for(const auto & entry : std::filesystem::directory_iterator(type.c_str())){ // reading all entries and enumerating
                              msgCount++;
                              // std::cout << "Index: " << index << " | Message count: " << msgCount << std::endl;
                              if(msgCount == index){
                                 entryPath = entry.path();
                                 printf("File found! Filepath: %s\n",entryPath.c_str());
                                 fileFound = true;
                              }
                              if(fileFound)
                                 break;
                           }
                        }
                        if(fileFound)
                           break;
                     }
                     if(msgCount < index || msgCount > index){
                        // std::cout << "Check Index: " << index << " | Check Message count: " << msgCount << std::endl;
                        if(sendERR(current_socket)){
                           perror("send answer failed");
                           return NULL;
                        }
                     }
                     file.open(entryPath);
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
                        else{
                           printf("Requested message sent!\n");
                        }
                        cleanBuffer(package);
                     }
                     else
                        std::cout << "Unable to open file\n";
                  }
                  delete[] directory;
               }
               else if(strcmp(buffer, "IN") == 0){
                  char* directory{ new char[strlen(storageLocation) + 1 + 1] };
                  directory = strcpy(directory, storageLocation);
                  directory = strcat(directory, "/");
                  printf("Composed directory: %s\n", directory);
                  size = recv(current_socket, buffer, BUF - 1, 0);
                  if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
                     break;
                  buffer[size] = '\0';          
                  int index = atoi(buffer);
                  printf("Number received in READ command: %d\n", index); 
                  if(std::filesystem::exists(directory)){ //Starting to look through storage folder
                     bool fileFound = false;
                     struct dirent *dir;
                     DIR *openDIR = opendir(directory);
                     std::string type;
                     std::string entryPath;
                     for(dir = readdir(openDIR); dir != NULL; dir = readdir(openDIR)){//iterating through all sender folders
                        type = dir->d_name;
                        type = directory + type;
                        printf("Next Composed directory: %s\n", type.c_str());
                        if((type.substr(type.find_last_of("/")+1, type.find_last_of('\0')-1) != ".") 
                        && (type.substr(type.find_last_of("/")+1, type.find_last_of('\0')-1) != "..")){
                           for(const auto & entry : std::filesystem::directory_iterator(type.c_str())){//iterating through receiver folders inside sender folders
                              std::string entryString = entry.path();
                              entryString = entryString.substr(entryString.find_last_of("/")+1, entryString.find_last_of('\n')-4);
                              if(strcmp(username,entryString.c_str()) == 0){
                                 std::string subPath = type + "/" + entryString + "/";
                                 for(const auto & entry : std::filesystem::directory_iterator(subPath.c_str())){
                                    msgCount++;
                                    // std::cout << "Index: " << index << " | Message count: " << msgCount << std::endl;
                                    if(msgCount == index){
                                       entryPath = entry.path();
                                       printf("File found! Filepath: %s\n",entryPath.c_str());
                                       fileFound = true;
                                    }
                                    if(fileFound)
                                       break;
                                 }
                              }
                              if(fileFound)
                                 break;
                           }
                        }
                        if(fileFound)
                           break;
                     }
                     if(msgCount < index || msgCount > index){
                        // std::cout << "Check Index: " << index << " | Check Message count: " << msgCount << std::endl;
                        if(sendERR(current_socket)){
                           perror("send answer failed");
                           return NULL;
                        }
                     }
                     file.open(entryPath);
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
                        else{
                           printf("Requested message sent!\n");
                        }
                        cleanBuffer(package);
                     }
                     else
                        std::cout << "Unable to open file\n";
                  }
                  delete[] directory;
               }
               else{
                  if(sendERR(current_socket)){
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
      }
      if(strcmp(buffer, "DEL") == 0){        //DEL Command Handling
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
               if(strcmp(buffer, "OUT") == 0){
                  char* directory{ new char[strlen(storageLocation) + 1 + strlen(username) + 1 + 1] };
                  directory = strcpy(directory, storageLocation);
                  directory = strcat(directory, "/");
                  directory = strcat(directory, username);
                  directory = strcat(directory, "/");
                  printf("Composed directory: %s\n", directory);
                  size = recv(current_socket, buffer, BUF - 1, 0);
                  if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
                     break;
                  buffer[size] = '\0';          
                  int index = atoi(buffer);
                  printf("Number received in DEL command: %d\n", index); 
                  if(std::filesystem::exists(directory)){ //Looking for requested Directory
                     bool fileFound = false;
                     struct dirent *dir;
                     DIR *openDIR = opendir(directory);
                     std::string type;
                     std::string entryPath;
                     for(dir = readdir(openDIR); dir != NULL; dir = readdir(openDIR)){ //iterating through receiver folders
                        type = dir->d_name;
                        type = directory + type;
                        if((type.substr(type.find_last_of("/")+1, type.find_last_of('\0')-1) != ".") 
                        && (type.substr(type.find_last_of("/")+1, type.find_last_of('\0')-1) != "..")){
                           printf("Next Composed directory: %s\n", type.c_str());
                           for(const auto & entry : std::filesystem::directory_iterator(type.c_str())){ // reading all entries and enumerating
                              msgCount++;
                              // std::cout << "Index: " << index << " | Message count: " << msgCount << std::endl;
                              if(msgCount == index){
                                 entryPath = entry.path();
                                 printf("File found! Filepath: %s\n",entryPath.c_str());
                                 std::filesystem::remove(entryPath);
                                 if(sendOK(current_socket)){
                                    perror("send answer failed");
                                    return NULL;
                                 }
                                 fileFound = true;
                              }
                              if(fileFound)
                                 break;
                           }
                        }
                        if(fileFound)
                           break;
                     }
                     if(msgCount < index || msgCount > index){
                        // std::cout << "Check Index: " << index << " | Check Message count: " << msgCount << std::endl;
                        if(sendERR(current_socket)){
                           perror("send answer failed");
                           return NULL;
                        }
                     }
                  }
                  delete[] directory;
               }
               else if(strcmp(buffer, "IN") == 0){
                  char* directory{ new char[strlen(storageLocation) + 1 + 1] };
                  directory = strcpy(directory, storageLocation);
                  directory = strcat(directory, "/");
                  printf("Composed directory: %s\n", directory);
                  size = recv(current_socket, buffer, BUF - 1, 0);
                  if(receiveMsgErrHandling(size))//returns true if an error has occured and ends the loop
                     break;
                  buffer[size] = '\0';          
                  int index = atoi(buffer);
                  printf("Number received in DEL command: %d\n", index); 
                  if(std::filesystem::exists(directory)){ //Looking for requested Directory
                     bool fileFound = false;
                     struct dirent *dir;
                     DIR *openDIR = opendir(directory);
                     std::string type;
                     std::string entryPath;
                     for(dir = readdir(openDIR); dir != NULL; dir = readdir(openDIR)){//iterating through all sender folders
                        type = dir->d_name;
                        type = directory + type;
                        printf("Next Composed directory: %s\n", type.c_str());
                        if((type.substr(type.find_last_of("/")+1, type.find_last_of('\0')-1) != ".") 
                        && (type.substr(type.find_last_of("/")+1, type.find_last_of('\0')-1) != "..")){
                           for(const auto & entry : std::filesystem::directory_iterator(type.c_str())){//iterating through receiver folders inside sender folders
                              std::string entryString = entry.path();
                              entryString = entryString.substr(entryString.find_last_of("/")+1, entryString.find_last_of('\n')-4);
                              if(strcmp(username,entryString.c_str()) == 0){
                                 std::string subPath = type + "/" + entryString + "/";
                                 for(const auto & entry : std::filesystem::directory_iterator(subPath.c_str())){
                                    msgCount++;
                                    // std::cout << "Index: " << index << " | Message count: " << msgCount << std::endl;
                                    if(msgCount == index){
                                       entryPath = entry.path();
                                       printf("File found! Filepath: %s\n",entryPath.c_str());
                                       std::filesystem::remove(entryPath);
                                       if(sendOK(current_socket)){
                                          perror("send answer failed");
                                          return NULL;
                                       }
                                       fileFound = true;
                                    }
                                    if(fileFound)
                                       break;
                                 }
                              }
                              if(fileFound)
                                 break;
                           }
                        }
                        if(fileFound)
                           break;
                                          }
                     if(msgCount < index || msgCount > index){
                        // std::cout << "Check Index: " << index << " | Check Message count: " << msgCount << std::endl;
                        if(sendERR(current_socket)){
                           perror("send answer failed");
                           return NULL;
                        }
                     }
                  }
                  delete[] directory;
               }
               else{
                  if(sendERR(current_socket)){
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
      }
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

void blacklistEntry(char* ipAddr){     //makes entry in Blacklist after 3 failed LOGIN attempts
   std::time_t timestamp = std::time(nullptr);
   pthread_mutex_lock(&lock);
   char* cpath{new char[path.length()+1]};
   strcpy(cpath, path.c_str());
   mkdir(cpath, 0777);
   cpath = strcat(cpath, ipAddr);
   mkdir(cpath, 0777);
   std::string date = std::asctime(std::localtime(&timestamp));
   char* name{new char[strlen(cpath) + 1 + date.length() + 1]};
   name = strcpy(name,cpath);
   name = strcat(name, "/");
   name = strcat(name, date.c_str());
   std::ofstream file(name);
   file.close();
   delete[] cpath;
   delete[] name;
   pthread_mutex_unlock(&lock);
}

bool blacklisted(char* ipAddr){        //checks if IP is currently blacklisted
   pthread_mutex_lock(&lock);
   char* cpath {new char[path.length()+ strlen(ipAddr) + 1]};
   strcpy(cpath, path.c_str());
   cpath = strcat(cpath, ipAddr);
   if(std::filesystem::exists(cpath)){
      free(cpath);
      pthread_mutex_unlock(&lock);
      return true;
   }
   delete[] cpath;
   pthread_mutex_unlock(&lock);
   return false;
}

void *blacklistUpkeep(void*){          //Checks Blacklist for entries that should be deleted
   while(1){
      sleep(2);
      pthread_mutex_lock(&lock);
      char* bl_path{new char[path.length()]};
      bl_path = strcpy(bl_path, path.c_str());
      if(std::filesystem::exists(bl_path)){ //Looking for requested Directory
         struct dirent *dir;
         DIR *openDIR = opendir(bl_path);
         std::string type;
         std::string entryPath;
         for(dir = readdir(openDIR); dir != NULL; dir = readdir(openDIR)){
            type = dir->d_name;
            std::string ip = type;
            type = bl_path + type;
            if((type.substr(type.find_last_of("/")+1, type.find_last_of('\0')-1) != ".") 
            && (type.substr(type.find_last_of("/")+1, type.find_last_of('\0')-1) != "..")){
               printf("Found IP folder: %s\n", type.c_str());
               for(const auto & entry : std::filesystem::directory_iterator(type.c_str())){     //looking for blacklist files
                  std::string entryString = entry.path();
                  entryString = entryString.substr(entryString.find_last_of("/")+1, entryString.find_last_of('\n'));
                  // std::cout << "Found date " << entryString << "\n";
                  std::stringstream timeString(entryString);
                  std::time_t timestamp;
                  struct std::tm tm;
                  timeString >> std::get_time(&tm, "%a %b %d %H:%M:%S %Y");
                  timestamp = mktime(&tm);
                  std::time_t now = std::time(nullptr);
                  std::localtime(&now);
                  // std::cout << "Calculated difference time_t: " << difftime(now, timestamp) << "\n";
                  if(difftime(now, timestamp) >= BL_TIMEOUT){
                     std::cout << "IP - " << ip << " - has been removed from Blacklist\n";
                     std::filesystem::remove_all(type.c_str());
                  }
               }
            }
         }
      }
      pthread_mutex_unlock(&lock);
      delete[] bl_path;
   }
   return NULL;
}

int sendERR(int current_socket){
   if (send(current_socket, "ERR", 4, 0) == -1){ //Send ERR if Login successful
      perror("send answer failed");
      return 1;
   }
   return 0;
}

int sendOK(int current_socket){
   if (send(current_socket, "OK", 3, 0) == -1){ //Send OK if Login successful
      perror("send answer failed");
      return 1;
   }
   return 0;
}