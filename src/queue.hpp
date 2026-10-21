#ifndef __DEF_QUEUE_H___
#define __DEF_QUEUE_H___

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <utility>

namespace chatpp {
	class message_queue {
	private:
		std::queue<std::pair<std::string, std::string>> messages_queue_;
		std::mutex mut_;
		std::condition_variable data_cond_;
		bool stopped_ = false;
		std::size_t max_size_;
	public:
		explicit message_queue(std::size_t max_size = 1024) : max_size_(max_size) {}

		template <class T>
		void enqueue(T&& e) {
			{
				std::lock_guard<std::mutex> lk(mut_);
				if (stopped_) {
					return;
				}
				messages_queue_.push(std::forward<T>(e));
				if (messages_queue_.size() > max_size_) {
					messages_queue_.pop();
				}
			}
			data_cond_.notify_one();
		}

		std::optional<std::pair<std::string, std::string>> wait_pop_message() {
			std::unique_lock<std::mutex> lk(mut_);
			data_cond_.wait(lk, [this] { return stopped_ || !messages_queue_.empty(); });
			if (messages_queue_.empty()) {
				return std::nullopt;
			}
			auto message = std::move(messages_queue_.front());
			messages_queue_.pop();
			return message;
		}

		void stop() {
			{
				std::lock_guard<std::mutex> lk(mut_);
				stopped_ = true;
			}
			data_cond_.notify_all();
		}
	};
}

#endif
