#ifndef LINUX_SERVER_H
#define LINUX_SERVER_H

#include <pthread.h>
#include <condition_variable>
#include <mutex>
#include <map>
#include <list>

#define JM_SEND_BUFFER_SIZE 1024

#include "MessageHandler.h"

namespace jm_networking {

	// ******* Nested classes/structs *******
	struct Connection {
		int socket;
		int id;
		std::string ip;
	};

	class LinuxServer {

	private:
		// Messages for received and sendimessages
		MessageHandler<LinuxServer> receivedHandler_;
		MessageHandler<LinuxServer> sendHandler_;

		int serverSocket_;
		int port_;
		int running_;
		char send_buffer_[JM_SEND_BUFFER_SIZE];
		// Received messages mutex and CV
		std::condition_variable queueCv_;
		std::mutex queueMutex_;

		// Connections
		std::vector<Connection*> clientConnections_;
		std::mutex connectionsMutex_;

		// Data object sent to threads
		std::thread listenThread_; // Listens for new connections

		// Private functions
		void InsertConnection(Connection* con);
		int GetConnectionCount();
		Connection* GetConnectionByIndex(unsigned int index);
		Connection* GetConnectionById(int id);
		void RemoveConnectionById(int id);
		void RemoveConnectionByIndex(int index);
		void ListenForConnections();
		void SendToSocket(int id, std::string& message);
		void AddReceivedMessage(int id, const std::string& message);

	public:
		// Creates the object and setups the sending messagehandler.
		LinuxServer();
		~LinuxServer();
		// Start server by listening the given port. Returns 0 for success and -1
		// for error.
		int StartServer(int port);
		// Send message to all clients
		void SendGlobalMessage(std::string message);
		// Send message to client with given id
		void SendMessage(int id, std::string message);
		// Returns one message, blocks until one is found or the server is shut down
		std::pair<int, std::string> ListenForReceivedMessage(int timeout = -1);
	};
}


#endif