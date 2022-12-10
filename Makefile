all: twMailer TWM_Client TWM_Server

twMailer: twMailer.cpp
	g++ -std=c++17 -Wall -Werror -o twMailer twMailer.cpp

TWM_Client: TWM_Client.cpp
	g++ -std=c++17 -Wall -Werror -o TWM_Client TWM_Client.cpp

TWM_Server: TWM_Server.cpp
	g++ -std=c++17 -Wall -Werror -o TWM_Server TWM_Server.cpp

clean:
	rm -f twMailer TWM_Client TWM_Server