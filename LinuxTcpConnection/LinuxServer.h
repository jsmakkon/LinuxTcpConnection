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
		// Messages for received messages
		MessageHandler<LinuxServer> mHandler;

		int serverSocket;
		int port;
		int running;
		char send_buffer_[JM_SEND_BUFFER_SIZE];
		// Received messages mutex and CV
		std::condition_variable queueCv;
		std::mutex queueMutex;

		// Connections
		std::vector<Connection*> clientConnections;
		std::mutex connectionsMutex;

		// Connection functions
		void InsertConnection(Connection* con);
		int GetConnectionCount();
		Connection* GetConnectionByIndex(unsigned int index);
		Connection* GetConnectionById(int id);
		void RemoveConnectionById(int id);
		void RemoveConnectionByIndex(int index);

		// Message handler for sending messages
		MessageHandler<LinuxServer> sendHandler;
		// Data object sent to threads
		std::thread listenThread; // Listens for new connections
		//std::thread sendThread;

		void ListenForConnections();
		// Sending
		void SendToSocket(int id, std::string& message);

		void AddReceivedMessage(int id, const std::string& message);

		

	public:

		LinuxServer();
		~LinuxServer();
		int StartServer(int port);
		void SendGlobalMessage(std::string message);
		void SendMessage(int id, std::string message);
		// Returns one message, blocks until one is found or the server is shut down
		std::pair<int, std::string> ListenForReceivedMessage(int timeout = -1);


	};
}


#endif