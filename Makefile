all: twMailer

twMailer: twMailer.cpp
	g++ -std=c++17 -Wall -Werror -o twMailer twMailer.cpp

clean:
	rm -f twMailer