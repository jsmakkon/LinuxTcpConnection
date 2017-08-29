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

		std::condition_variable receiveCv;
		std::mutex receiveMutex;

		MessageHandler<LinuxClient> receiveHandler_;
		MessageHandler<LinuxClient> sendHandler_;

		std::thread receiverThread;

		void ListenForMessages();

		void SendToSocket(int id, std::string& message);
		void CloseSocket();
		void AddReceivedMessage(int id, const std::string & message);
	public:

		LinuxClient();
		~LinuxClient();
		// Returns 1 if successful, -1 if timeout or error
		int ConnectTo(std::string ip, int port, int timeout_millis);
		int isConnected();
		void Disconnect();
		void SendMessage(int id, std::string message);
		
		std::pair<int, std::string> ListenForReceivedMessage(int timeout = -1);

	};

}

#endif // LINUX_CLIENT_H
