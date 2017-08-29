#include "LinuxServer.h"
#include "LinuxClient.h"
#include <stdio.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <string>
#include <mutex>
#include <exception>

using namespace jm_networking;

std::mutex printMutex;
int running;

void Print(const std::string& message) {
	std::unique_lock<std::mutex> lock(printMutex);
	std::cout << message.c_str() << std::endl;
}

void ListenClientMessages(LinuxClient* client) {
	while (running) {
		std::pair<int, std::string> message = client->ListenForReceivedMessage(2000);
		if (message.second.empty())
			continue;
		std::string whole_message = "Received from ";
		whole_message += std::to_string(message.first);
		whole_message += ": ";
		whole_message += message.second;
		whole_message += "\n";
		Print(whole_message);
	}
}

void ListenServerMessages(LinuxServer* server) {
	
	while (running) {
		std::pair<int, std::string> message = server->ListenForReceivedMessage(2000);
		if (message.second.empty())
			continue;
		std::string whole_message = "Received from ";
		whole_message += std::to_string(message.first);
		whole_message += ": ";
		whole_message += message.second;
		whole_message += "\n";
		Print(whole_message);
	}
}

void RunAsClient() {
	LinuxClient client;
	int port = 42042;
	std::string ip = "127.0.0.1";
	std::string answer;
	/*std::cout << "(Client) Port?" << std::endl;
	answer = "";
	std::getline(std::cin, answer);
	try {
		port = std::stoi(answer);
	}
	catch (...) {
		std::cout << "Can't parse port\n" << std::endl;
		return ;
	}
	std::cout << "IP?" << std::endl;
	answer = "";
	std::getline(std::cin, answer);
	ip = answer;*/
	// TODO: Ip parse check
	int success = client.ConnectTo(ip, port, 5000);
	if (success < 0) {
		std::cout << "Failed to connect" << std::endl;
		std::getline(std::cin, answer);
	}
	else {
		
		std::cout << "Connected, listening for incoming messages" << std::endl;
		running = 1;
		std::thread listener(ListenClientMessages, &client);
		while (running) {
			usleep(1000000);
			running = 0;
			/*std::getline(std::cin, answer);
			if (answer == "quit") {
				running = 0;
			}
			Print(answer);
			client.SendMessage(0, answer);*/
		}
		listener.join();
	}
	std::cout << "Shutting down" << std::endl;
}

void RunAsServer() {
	LinuxServer server;
	int port;
	std::string answer;
	std::cout << "(Client) Port?" << std::endl;
	answer = "";
	std::getline(std::cin, answer);
	try {
		port = std::stoi(answer);
	}
	catch (...) {
		std::cout << "Can't parse port\n" << std::endl;
		return;
	}
	// TODO: Ip parse check
	server.StartServer(port);
	std::cout << "Started listening for connections" << std::endl;
	running = 1;
	std::thread listener(ListenServerMessages, &server);
	while (running) {
		
		std::getline(std::cin, answer);
		if (answer == "quit") {
			running = 0;
		}
		Print(answer);
		server.SendGlobalMessage(answer);
	}
	listener.join();
	
	std::cout << "Shutting down" << std::endl;
}

int main(int argc, char* argv[])
{
	int running = 1;
	std::string ip;
	setbuf(stdout, NULL);
	while (running) {
		std::cout << "Hello! Client(c) or server(s)?" << std::endl;
		std::string answer;
		std::getline(std::cin, answer);
		if (answer[0] == 'c') {
			int i = 0;
			while (1) {
				RunAsClient();
				std::cout << "RUN: " << i++ << std::endl;
			}
		}
		else if (answer[0] == 's') {
			RunAsServer();

		}
	}

	return 0;
}