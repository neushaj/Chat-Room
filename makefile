all: client.out
client.out: client.o main_client.o server
	g++ -std=c++11 client.o main_client.o -o client.out

client.o: client.cpp message.h client.h
	g++ -std=c++11 -c client.cpp

main_client.o: main_client.cpp client.h
	g++ -std=c++11 -c main_client.cpp

server:
	g++ -std=c++11 -c main_server.cpp -o main_server.o
	g++ -std=c++11 -c server.cpp -o server.o
	g++ -std=c++11 -pthread server.o main_server.o -o server.out

clean:
	rm *.o
	rm *.out
