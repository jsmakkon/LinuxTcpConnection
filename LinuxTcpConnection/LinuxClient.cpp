#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include "LinuxClient.h"
namespace jm_networking {

	LinuxClient::LinuxClient() : sendHandler_(CALLBACK) {
		this->connected_ = 0;
		this->running_ = 0;

		this->sendHandler_.AddMessageCallback(&LinuxClient::SendToSocket,this);
	}

	LinuxClient::~LinuxClient() {
		this->running_ = 0;
		this->receiverThread.join();
	}
	// Graceful closing of socket, TODO
	void LinuxClient::CloseSocket() {
		struct timeval timeout;
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(this->socket_, &readSet);
		timeout.tv_sec = 0;
		timeout.tv_usec = 200000; // 0.2 sec
		char discard_buffer[1024];

		int retValue = select(this->socket_ + 1, &readSet, NULL, NULL, &timeout);
		if (retValue == 0) // Timeout
		{
			printf("CloseSocket Timeout");
		}
		else if (retValue == -1) // Error
		{
			// TODO: Handle error
			printf("CloseSocket error");
			return;
		}
		else { // Some input from socket
			printf("CloseSocket something to read");
			long read_amount = read(this->socket_, discard_buffer, 1024);
		}

		if (shutdown(this->socket_, SHUT_RDWR) < 0) {
			printf("Shutdown error: %d", errno);
		}

		if (close(this->socket_) < 0) {
			printf("Close error: %d", errno);
		}
	}

	void LinuxClient::ListenForMessages() {
		struct timeval timeout;
		int retValue;
		long read_amount;
		fd_set readSet;
		char buffer[1025]; // TODO: dynamic buffer size

		while (this->running_) {
			// Set timeout
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;

			FD_ZERO(&readSet);
			FD_SET(this->socket_, &readSet);

			// Start selecting
			retValue = select(this->socket_ + 1, &readSet, NULL, NULL, &timeout);

			if (retValue == 0) // Timeout
			{
				// TODO: keepalive check here
				continue;
			}
			else if (retValue == -1) // Error
			{
				// TODO: Handle error
				return;
			}
			else { // Some input from socket
				read_amount = read(this->socket_, buffer, 1024);
				//Check if it was for closing , and also read the incoming message
				if (read_amount == 0)
				{
					// Server disconnected us, close the socket
					printf("Socket ");
					this->running_ = 0;
				}
				else if (read_amount < 0) {
					// TODO: Error handling
				}
				else
				{
					//set the string terminating NULL byte on the end of the data read
					buffer[read_amount] = '\0';
					std::string message(buffer);
					this->AddReceivedMessage(0, message);
				}
			}
		}
		this->CloseSocket();
	}
	
	int LinuxClient::ConnectTo(std::string ip, int port, int timeout_millis)
	{
		int retStatus = 0;
		fd_set fdset;
		socklen_t len;
		int valopt;
		struct timeval timeout;
		this->socket_ = socket(AF_INET, SOCK_STREAM, 0);

		if (this->socket_ < 0) {
			printf("Creating server socket failed: %d\n", errno);
			return -1;
		}

		int status = fcntl(this->socket_, F_SETFL, fcntl(this->socket_, F_GETFL, 0) | O_NONBLOCK);

		if (status == -1) {
			printf("Fcntl failed: %d\n", errno);
			return -1;
		}

		struct sockaddr_in servAddr;

		servAddr.sin_family = AF_INET;
		servAddr.sin_addr.s_addr = inet_addr(ip.c_str());
		servAddr.sin_port = htons((uint16_t)port);

		retStatus = connect(this->socket_, (struct sockaddr *)&servAddr, sizeof(servAddr));

		if (retStatus < 0) {
			if (errno == EINPROGRESS) {

				timeout.tv_sec = timeout_millis / 1000;
				timeout.tv_usec = (timeout_millis % 1000)*1000;
				FD_ZERO(&fdset);
				FD_SET(this->socket_, &fdset);
				retStatus = select(this->socket_ + 1, NULL, &fdset, NULL, &timeout);
				if (retStatus < 0 && errno != EINTR) {
					printf("Error connecting %d\n", errno);
					return -1;
				}
				else if (retStatus > 0) {
					// Socket selected for write 
					len = sizeof(int);
					if (getsockopt(this->socket_, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &len) < 0) {
						printf("Error in getsockopt() %d\n", errno);
						return -1;
					}
					// Check the value returned... 
					if (valopt) {
						printf("Error in delayed connection() %d\n", valopt);
						return -1;
					}
					// We have connection now
				}
				else {
					printf("Client select timeout, failed to connect in 5s\n");
					return -1;
				}
			}
			else {
				fprintf(stderr, "Error connecting %d\n", errno);
				return -1;
			}
		}
		this->running_ = 1;
		// Start thread to listen for messages
		this->receiverThread = std::thread(&LinuxClient::ListenForMessages, this);
		return 1;
	}

	int LinuxClient::isConnected()
	{
		return this->connected_;
	}
	void LinuxClient::Disconnect() {
		this->running_ = 0;
		// TODO: Add 0 message to disconnect faster
	}
	void LinuxClient::SendMessage(int id, std::string message) {
		this->sendHandler_.AddMessage(0,message);
	}

	void LinuxClient::AddReceivedMessage(int id, const std::string& message) {
		std::unique_lock<std::mutex> lock(this->receiveMutex);
		this->receiveHandler_.AddMessage(id, message);
		this->receiveCv.notify_all();
	}

	std::pair<int, std::string> LinuxClient::ListenForReceivedMessage(int timeout) {
		std::pair<int, std::string> message;
		std::unique_lock<std::mutex> lock(this->receiveMutex);
		message = this->receiveHandler_.GetMessage();
		if (message.first == -1 && this->running_ == 1) {
			if (timeout == -1) {
				this->receiveCv.wait(lock);
			}
			else {
				this->receiveCv.wait_for(lock, std::chrono::milliseconds(timeout));
			}
			message = this->receiveHandler_.GetMessage();
		}

		return message;
	}

	void LinuxClient::SendToSocket(int id, std::string& message) {
		if (message.size() > JM_CLIENT_SEND_BUFFER_SIZE) {
			std::memcpy(this->send_buffer_, &message[0], JM_CLIENT_SEND_BUFFER_SIZE);
		}
		else {
			std::memcpy(this->send_buffer_, &message[0], message.size());
		}
		this->send_buffer_[message.size()] = '\0';
		send(this->socket_, this->send_buffer_, strlen(this->send_buffer_), 0);
	}
}