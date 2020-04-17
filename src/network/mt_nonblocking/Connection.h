
#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <cstring>
#include <sys/epoll.h>
#include <spdlog/logger.h>
#include <protocol/Parser.h>
#include <afina/Storage.h>
#include <afina/execute/Command.h>


namespace Afina {
namespace Network {
namespace MTnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> ps, std::shared_ptr<spdlog::logger> lg) : _socket(s), pStorage(ps), _logger(lg) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
        is_Alive.store(true);
    }
    inline bool isAlive() const { return is_Alive.load(); }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class Worker;
    friend class ServerImpl;

    std::atomic<bool> is_Alive;

    int _socket;
    struct epoll_event _event;
    std::mutex mutex;

    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> pStorage;
    std::unique_ptr<Execute::Command> command_to_execute;
    Protocol::Parser parser;
    std::size_t arg_remains;
    std::string argument_for_command;
    std::vector<std::string> replies;
    ssize_t current_bytes = 0;

    ssize_t rest = 0;

};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
