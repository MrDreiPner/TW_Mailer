all: TWM_Client TWM_Server

TWM_Client: TWM_Client.cpp
	g++ -std=c++17 -Wall -Werror -o TWM_Client TWM_Client.cpp

TWM_Server: TWM_Server.cpp
	g++ -std=c++17 -Wall -Werror -o TWM_Server TWM_Server.cpp -pthread -lldap -llber

TWM_Session: TWM_Session.cpp
	g++ -std=c++17 -Wall -Werror -o TWM_Session TWM_Session.cpp


clean:
	rm -f TWM_Client TWM_Server