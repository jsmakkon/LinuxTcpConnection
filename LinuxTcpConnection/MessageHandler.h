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
	// TODO: Rework to work between two classes as the current implementation 
	// works only with one class. For now, this is enough, since the messagehandler
	// is only used with socket classes.
	// Message format: <int,string>
	template <class T>
	class MessageHandler {
		
		std::vector<std::pair<T*, void(T::*)(int, std::string&)> > callbacks_;

		std::queue<std::pair<int, std::string> > messages_;
		int running_;

		std::condition_variable queueCv_;
		std::mutex queueMutex_;
		std::mutex callbacksMutex_;

		std::thread sendThread_;
		MessageMode currentMode_;

		void ListenForMessages() {

			std::unique_lock<std::mutex> lck(this->queueMutex_);
			std::pair<int, std::string> message;
			while (this->running_)
			{
				// Wait for messages.
				while (this->messages_.empty()) {
					this->queueCv_.wait(lck);
				}
				message = this->messages_.front();
				this->messages_.pop();

				if (message.second[0] == '\0') {
					continue;
				}
				// Message received, lets send it.
				for (unsigned int i = 0; i < this->callbacks_.size(); i++) {
					printf("MessageHandler sending callback\n");
					T* object = this->callbacks_[i].first;
					(object->*(this->callbacks_[i].second))(message.first, message.second);
				}
			}
		}

	public:

		void AddMessageCallback(void(T::*func)(int, std::string&), T* object) {
			std::unique_lock<std::mutex> lck(this->callbacksMutex_);
			this->callbacks_.push_back(std::pair<T*, void(T::*)(int, std::string&)>(object, func));
		}

		MessageHandler() {
			this->currentMode_ = MANUAL_FETCHING;
		}

		~MessageHandler() {
			this->running_ = 0;
			if (this->currentMode_ == CALLBACK && this->sendThread_.joinable()) {
				std::string quit_message;
				quit_message.push_back('\0');
				this->AddMessage(0, quit_message);
				this->sendThread_.join();
			}
		}

		MessageHandler(MessageMode mode) {
			this->currentMode_ = mode;
			if (mode == CALLBACK) {
				// Start a thread
				this->sendThread_ = std::thread(&MessageHandler::ListenForMessages, this);
			}
		}
		
		std::pair<int, std::string> GetMessage() {
			std::unique_lock<std::mutex> lck(this->queueMutex_);
			if (this->messages_.size() == 0) {
				return std::pair<int,std::string>(-1, "");
			}
			std::pair<int, std::string> message = this->messages_.front();
			this->messages_.pop();
			return message;
		}

		std::vector<std::pair<int, std::string>> GetAllMessages() {
			std::unique_lock<std::mutex> lck(this->queueMutex_);
			if (this->messages_.size() == 0) {
				return std::pair<int, std::string>(-1, "");
			}
			std::vector<std::pair<int, std::string>> ret;
			for (unsigned int i = 0; i < this->messages_.size(); i++) {
				ret.push_back(this->messages_.front());
			}
			this->messages_ = std::queue<std::pair<int, std::string> >();
			return ret;
		}

		void AddMessage(int id, const std::string& message) {
			if (message.size() == 0) {
				return;
			}
			std::unique_lock<std::mutex> lck(this->queueMutex_);
			std::pair<int, std::string> mes(id, message);
			this->messages_.push(mes);
			this->queueCv_.notify_all();
		}
	};
}
#endif // !MESSAGE_HANDLER_H