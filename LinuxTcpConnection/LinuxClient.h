#ifndef LINUX_CLIENT_H
#define LINUX_CLIENT_H

#include <thread>
#include "MessageHandler.h"

#define JM_CLIENT_SEND_BUFFER_SIZE 1024

namespace jm_networking {

	class LinuxClient {

	private:
		int socket_;
		int connected_;
		int running_;

		char send_buffer_[JM_CLIENT_SEND_BUFFER_SIZE];

		std::condition_variable receiveCv_;
		std::mutex receiveMutex_;

		MessageHandler<LinuxClient> receiveHandler_;
		MessageHandler<LinuxClient> sendHandler_;

		std::thread receiverThread_;

		void ListenForMessages();
		void SendToSocket(int id, std::string& message);
		void CloseSocket();
		void AddReceivedMessage(int id, const std::string & message);
	public:
		// Creates the object and setups the sending  messagehandler.
		LinuxClient();
		~LinuxClient();
		// Try to create connection. Returns 0 on success and <0 on error or
		// timeout. If connection is already established to some address,
		// nothing is done.
		int ConnectTo(std::string ip, int port, int timeout_millis);
		
		int isConnected();

		void Disconnect();

		// Send message to server. If connection is not made, nothing is done.
		void SendMessage(std::string message);
		
		// Listens for message received through the socket. Returns pair, where
		// the first is always 0 and second includes the message.
		// In case of timeout, first is set to -1.
		std::pair<int, std::string> ListenForReceivedMessage(int timeout = -1);

	};

}

#endif // LINUX_CLIENT_H
