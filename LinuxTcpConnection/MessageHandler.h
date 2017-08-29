#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include <string>
#include <queue>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <pthread.h>

namespace jm_networking
{
	class LinuxServer;

	enum MessageMode {
		CALLBACK,
		MANUAL_FETCHING
	};
	
	// Message handler class to send and receive messages with thread safety.
	// Message format: <int,string>
	template <class T>
	class MessageHandler {
		
		std::vector<std::pair<T*, void(T::*)(int, std::string&)> > callbacks;

		std::queue<std::pair<int, std::string> > messages;
		int running;

		std::condition_variable queueCv;
		std::mutex queueMutex;
		std::mutex callbacksMutex;

		std::thread sendThread;
		MessageMode currentMode_;

		void ListenForMessages() {

			std::unique_lock<std::mutex> lck(this->queueMutex);
			std::pair<int, std::string> message;
			while (this->running)
			{
				// Wait for messages.
				while (this->messages.empty()) {
					this->queueCv.wait(lck);
				}
				message = this->messages.front();
				this->messages.pop();

				if (message.second[0] == '\0') {
					continue;
				}
				// Message received, lets send it.
				for (unsigned int i = 0; i < this->callbacks.size(); i++) {
					printf("MessageHandler sending callback\n");
					T* object = this->callbacks[i].first;
					(object->*(this->callbacks[i].second))(message.first, message.second);
				}
			}
		}

	public:

		void AddMessageCallback(void(T::*func)(int, std::string&), T* object) {
			std::unique_lock<std::mutex> lck(this->callbacksMutex);
			this->callbacks.push_back(std::pair<T*, void(T::*)(int, std::string&)>(object, func));
		}

		MessageHandler() {
			this->currentMode_ = MANUAL_FETCHING;
		}

		~MessageHandler() {
			this->running = 0;
			if (this->currentMode_ == CALLBACK && this->sendThread.joinable()) {
				std::string quit_message;
				quit_message.push_back('\0');
				this->AddMessage(0, quit_message);
				this->sendThread.join();
			}
		}

		MessageHandler(MessageMode mode) {
			this->currentMode_ = mode;
			if (mode == CALLBACK) {
				// Start a thread
				this->sendThread = std::thread(&MessageHandler::ListenForMessages, this);
			}
		}
		
		std::pair<int, std::string> GetMessage() {
			std::unique_lock<std::mutex> lck(this->queueMutex);
			if (this->messages.size() == 0) {
				return std::pair<int,std::string>(-1, "");
			}
			std::pair<int, std::string> message = this->messages.front();
			this->messages.pop();
			return message;
		}

		std::vector<std::pair<int, std::string>> GetAllMessages() {
			std::unique_lock<std::mutex> lck(this->queueMutex);
			if (this->messages.size() == 0) {
				return std::pair<int, std::string>(-1, "");
			}
			std::vector<std::pair<int, std::string>> ret;
			for (unsigned int i = 0; i < this->messages.size(); i++) {
				ret.push_back(this->messages.front());
			}
			this->messages = std::queue<std::pair<int, std::string> >();
			return ret;
		}

		void AddMessage(int id, const std::string& message) {
			if (message.size() == 0) {
				return;
			}
			std::unique_lock<std::mutex> lck(this->queueMutex);
			std::pair<int, std::string> mes(id, message);
			this->messages.push(mes);
			this->queueCv.notify_all();
		}
	};
}
#endif // !MESSAGE_HANDLER_H