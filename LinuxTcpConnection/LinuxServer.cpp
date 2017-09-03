#include "LinuxServer.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

namespace jm_networking {


	LinuxServer::LinuxServer():sendHandler_(CALLBACK) {
		this->running_ = 0;
	}

	LinuxServer::~LinuxServer() {
		this->running_ = 0;
		this->listenThread_.join();
	}

	// TODO: Throw on fails
	void LinuxServer::ListenForConnections() {

		int opt = 1, id = 0;

		this->serverSocket_= socket(AF_INET, SOCK_STREAM, 0);

		if (this->serverSocket_ < 0) {
			printf("Creating server socket failed: %d\n", errno);
			return ;
		}

		int status = fcntl(this->serverSocket_, F_SETFL, fcntl(this->serverSocket_, F_GETFL, 0) | O_NONBLOCK);

		if (status == -1) {
			printf("Fcntl failed: %d\n", errno);
			return;
		}

		if (setsockopt(this->serverSocket_, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
		{
			printf("Setsockopt failed\n"); 
			return;
		}

		struct sockaddr_in servAddr;
		
		servAddr.sin_family = AF_INET;
		servAddr.sin_addr.s_addr = INADDR_ANY;
		servAddr.sin_port = htons((uint16_t)this->port_);

		if (bind(this->serverSocket_, (struct sockaddr *) &servAddr,
			sizeof(servAddr)) < 0) {
			return ;
		}

		int servAddrLen = sizeof(servAddr);

		listen(this->serverSocket_, 3);
		int select_ret = 0;
		fd_set readSet;
		int client_sock;
		int greatest_fd = this->serverSocket_;
		long read_amount;
		char buffer[1025]; // TODO: Check for proper buffer size
		struct timeval timeout;
		while (this->running_) {
			// Set timeout
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
			// Clear the set
			FD_ZERO(&readSet);
			// Add serversocket to set
			FD_SET(this->serverSocket_, &readSet);
			// Add all the connections to set
			int temp_socket;
			for (int i = 0; i < this->GetConnectionCount(); i++) {
				temp_socket = this->GetConnectionByIndex(i)->socket;
				FD_SET(temp_socket, & readSet);
				if (temp_socket > greatest_fd) {
					greatest_fd = temp_socket;
				}
			}
			// Start selecting
			select_ret = select(greatest_fd + 1, &readSet, NULL, NULL, &timeout);
			if (select_ret == 0) // Timeout
			{
				continue;
			}
			else if (select_ret == -1) // Error
			{
				// TODO: Handle error
				return;
			}
			else if (FD_ISSET(this->serverSocket_, &readSet)){ // New connection arriving
				client_sock = accept(this->serverSocket_, (struct sockaddr *)&servAddr, (socklen_t*)&servAddrLen);
				if (client_sock < 0) {
					// Client is gone already, or some error occured
					// TODO: Proper error handling
					printf("Client lost at accept\n");
					continue;
				}
				Connection* con = new Connection();
				con->id = ++id;
				con->ip = std::string(inet_ntoa(servAddr.sin_addr));
				con->socket = client_sock;
				this->InsertConnection(con);
				printf("Added new connection, ID: %d, IP: %s\n", con->id, con->ip.c_str());
			}
			else { // Some input from sockets
				for (int i = 0; i < this->GetConnectionCount(); i++) {

					Connection* con = this->GetConnectionByIndex(i);
					if (FD_ISSET(con->socket, &readSet)) {
						//Check if it was for closing , and also read the incoming message
						if ((read_amount = read(con->socket, buffer, 1024)) == 0)
						{
							//Somebody disconnected , get his details and print
							getpeername(con->socket, (struct sockaddr*)&servAddr, (socklen_t*)&servAddrLen);
							printf("Connection %d disconnected\n", con->id);
							//printf("Host disconnected , ip %s , port %d \n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

							//Close the socket and mark as 0 in list for reuse
							close(con->socket);
							this->RemoveConnectionByIndex(i);
						}

						//Echo back the message that came in
						else
						{
							printf("Received message\n");
							//set the string terminating NULL byte on the end of the data read
							buffer[read_amount] = '\0';
							std::string message(buffer);
							this->AddReceivedMessage(con->id,message);
						}
					}
				}
			}
		}

		close(this->serverSocket_);
	}

	int LinuxServer::StartServer(int p) {
		// TODO: Success of start of listening should be returned
		this->port_ = p;
		this->running_ = 1;
		// Start listening connections in new thread
		this->listenThread_ = std::thread(&LinuxServer::ListenForConnections, this);
		// Add callback to sendHandler to send the messages
		this->sendHandler_.AddMessageCallback(&LinuxServer::SendToSocket,this);

		return 0;
	}

	// ServerData - Connections
	void LinuxServer::InsertConnection(Connection* con) {
		std::unique_lock<std::mutex> lock(connectionsMutex_);
		clientConnections_.push_back(con);
	}

	int LinuxServer::GetConnectionCount() {
		int count = -1;
		std::unique_lock<std::mutex> lock(connectionsMutex_);
		count = static_cast<int>(clientConnections_.size());
		return count;
	}

	Connection* LinuxServer::GetConnectionByIndex(unsigned int index) {
		Connection* con = NULL;
		std::unique_lock<std::mutex> lock(connectionsMutex_);
		if (index < clientConnections_.size()) {
			con = clientConnections_[index];
		}
		return con;
	}

	Connection* LinuxServer::GetConnectionById(int id) {
		Connection* con = NULL;
		std::unique_lock<std::mutex> lock(connectionsMutex_);
		std::vector<Connection*>::iterator it = clientConnections_.begin();
		for (; it != clientConnections_.end(); it++) {
			if ((*it)->id == id) {
				con = *it;
			}
		}

		return con;
	}
	void LinuxServer::RemoveConnectionById(int id) {
		std::unique_lock<std::mutex> lock(connectionsMutex_);
		std::vector<Connection*>::iterator it = clientConnections_.begin();
		for (; it != clientConnections_.end(); it++) {
			if ((*it)->id == id) {
				clientConnections_.erase(it);
			}
		}
	}

	void LinuxServer::RemoveConnectionByIndex(int index) {
		std::unique_lock<std::mutex> lock(connectionsMutex_);
		Connection* con = this->clientConnections_[index];
		delete con;
		this->clientConnections_.erase(this->clientConnections_.begin() +index);

	}

	void LinuxServer::SendToSocket(int id, std::string& message) {
		Connection* con = this->GetConnectionById(id);
		if (message.size() > JM_SEND_BUFFER_SIZE) {
			std::memcpy(this->send_buffer_, &message[0], JM_SEND_BUFFER_SIZE);
		}
		else {
			std::memcpy(this->send_buffer_, &message[0], message.size());
		}
		this->send_buffer_[message.size()] = '\0';
		send(con->socket, this->send_buffer_, strlen(this->send_buffer_), 0);
	}

	void LinuxServer::SendMessage(int id, std::string message) {
		this->sendHandler_.AddMessage(id, message);
	}

	void LinuxServer::SendGlobalMessage(std::string message) {
		for (unsigned int i = 0; i < this->clientConnections_.size(); i++) {
			this->sendHandler_.AddMessage(this->clientConnections_[i]->id, message);
		}
	}

	void LinuxServer::AddReceivedMessage(int id, const std::string& message) {
		// TODO: Move this to messagehandler if possible, with ListenForReceivedMessage
		std::unique_lock<std::mutex> lock(this->queueMutex_);
		this->receivedHandler_.AddMessage(id, message);
		this->queueCv_.notify_all();
	}

	std::pair<int, std::string> LinuxServer::ListenForReceivedMessage(int timeout) {
		std::pair<int, std::string> message;
		std::unique_lock<std::mutex> lock(this->queueMutex_);
		message = this->receivedHandler_.GetMessage();
		if (message.first == -1 && this->running_ == 1) {
			if (timeout == -1) {
				this->queueCv_.wait(lock);
			}
			else {
				this->queueCv_.wait_for(lock, std::chrono::milliseconds(timeout));
			}
			message = this->receivedHandler_.GetMessage();
		}

		return message;
	}
}