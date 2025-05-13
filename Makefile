# Makefile for Distributed Log File Analysis System

CXX = g++

# Targets
SERVER_SRC = server/server.cpp
CLIENT_SRC = client/client.cpp

SERVER_OUT = server_app
CLIENT_OUT = client_app

all: $(SERVER_OUT) $(CLIENT_OUT)

$(SERVER_OUT): $(SERVER_SRC)
	$(CXX) -pthread -o $@ $<

$(CLIENT_OUT): $(CLIENT_SRC)
	$(CXX)  -o $@ $<

clean:
	rm -f $(SERVER_OUT) $(CLIENT_OUT)

.PHONY: all clean