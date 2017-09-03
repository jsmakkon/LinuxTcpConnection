#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <netdb.h>

#include "LinuxClient.h"

namespace jm_networking {

	LinuxClient::LinuxClient() : sendHandler_(CALLBACK) {
		this->connected_ = 0;
		this->running_ = 0;

		this->sendHandler_.AddMessageCallback(&LinuxClient::SendToSocket,this);
	}

	LinuxClient::~LinuxClient() {
		this->running_ = 0;
		this->receiverThread_.join();
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
			read(this->socket_, discard_buffer, 1024);
		}

		if (shutdown(this->socket_, SHUT_RDWR) < 0) {
			printf("Shutdown error: %d", errno);
		}

		if (close(this->socket_) < 0) {
			printf("Close error: %d", errno);
		}
		this->connected_ = 0;
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
				// TODO: keepalive and ping check here
				continue;
			}
			else if (retValue == -1) // Error
			{
				// TODO: Handle error
				printf("Socket error, closing socket");
				break;
			}
			else { // Some input from socket
				read_amount = read(this->socket_, buffer, 1024);
				//Check if it was for closing , and also read the incoming message
				if (read_amount == 0)
				{
					// Server disconnected us, close the socket
					printf("Socket disconnected");
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
	
	int LinuxClient::ConnectTo(std::string address, int port, int timeout_millis)
	{
		if (this->connected_ == 1) {
			printf("Trying to connect when connected, returning");
			return -1;
		}
		int retStatus = 0;
		fd_set fdset;
		socklen_t len;
		int valopt;
		struct timeval timeout;
		struct addrinfo hints;
		struct addrinfo *addresses, *adr;
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_INET; // We go with ipv4 for now
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = 0;
		std::string port_str = std::to_string(port);

		int ret = getaddrinfo(address.c_str(), port_str.c_str(), &hints, &addresses);
		if (ret != 0)
		{
			printf("getaddrinfo error\n");
			return- 1;
		}

		for (adr = addresses; adr != NULL; adr = adr->ai_next) {
			this->socket_ = socket(adr->ai_family, adr->ai_socktype,
				adr->ai_protocol);

			if (this->socket_ < 0) {
				printf("Failed to create socket\n");
				continue;
			}

			int status = fcntl(this->socket_, F_SETFL, fcntl(this->socket_, F_GETFL, 0) | O_NONBLOCK);

			if (status == -1) {
				printf("Fcntl failed: %d\n", errno);
				close(this->socket_);
				continue;
			}
			retStatus = connect(this->socket_, adr->ai_addr, adr->ai_addrlen);

			if (retStatus < 0) {
				if (errno == EINPROGRESS) {

					timeout.tv_sec = timeout_millis / 1000;
					timeout.tv_usec = (timeout_millis % 1000) * 1000;
					FD_ZERO(&fdset);
					FD_SET(this->socket_, &fdset);
					retStatus = select(this->socket_ + 1, NULL, &fdset, NULL, &timeout);
					if (retStatus < 0 && errno != EINTR) {
						printf("Error connecting %d\n", errno);
						close(this->socket_);
						continue;
					}
					else if (retStatus > 0) {
						// Socket selected for write 
						len = sizeof(int);
						if (getsockopt(this->socket_, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &len) < 0) {
							printf("Error in getsockopt() %d\n", errno);
							close(this->socket_);
							continue;
						}
						// Check the error value it is fine
						if (valopt) {
							printf("Error in delayed connection() %d\n", valopt);
							close(this->socket_);
							continue;
						}
						// We have connection now
						this->connected_ = 1;
						break;
					}
					else {
						printf("Client select timeout, failed to connect in 5s\n");
						close(this->socket_);
						continue;
					}
				}
				else {
					printf("Connect failed to address: %d\n", errno);
					close(this->socket_);
					continue;
				}
			}
		}

		// Resolve the result
		if (this->connected_ == 0) {
			printf("Failed to connect\n");
			return -1;
		}
		else {
			// Start thread to listen for incoming messages
			this->running_ = 1;
			printf("Connection succesful");
			this->receiverThread_ = std::thread(&LinuxClient::ListenForMessages, this);
			return 0;
		}
	}

	int LinuxClient::isConnected()
	{
		return this->connected_;
	}
	void LinuxClient::Disconnect() {
		this->running_ = 0;
		// TODO: Add 0 message to disconnect faster
	}
	void LinuxClient::SendMessage(std::string message) {
		this->sendHandler_.AddMessage(0, message);
	}

	void LinuxClient::AddReceivedMessage(int id, const std::string& message) {
		std::unique_lock<std::mutex> lock(this->receiveMutex_);
		this->receiveHandler_.AddMessage(id, message);
		this->receiveCv_.notify_all();
	}

	std::pair<int, std::string> LinuxClient::ListenForReceivedMessage(int timeout) {
		std::pair<int, std::string> message;
		std::unique_lock<std::mutex> lock(this->receiveMutex_);
		message = this->receiveHandler_.GetMessage();
		if (message.first == -1 && this->running_ == 1) {
			if (timeout == -1) {
				this->receiveCv_.wait(lock);
			}
			else {
				this->receiveCv_.wait_for(lock, std::chrono::milliseconds(timeout));
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