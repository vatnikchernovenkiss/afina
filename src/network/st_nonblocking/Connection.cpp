#include "Connection.h"

#include <iostream>
#include <sys/uio.h>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() {
    _logger->debug("Start {} socket", _socket);
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    _event.data.fd = _socket;
    _event.data.ptr = this;
}

// See Connection.h
void Connection::OnError() {
    _logger->debug("Error on {} socket", _socket);
    is_Alive = false;
}

// See Connection.h
void Connection::OnClose() {
    _logger->debug("Close {} socket", _socket);
    is_Alive = false;
}

// See Connection.h
void Connection::DoRead() {
    _logger->debug("Read from {} socket", _socket);

    try {
        int readed_bytes = 0;
        char client_buffer[4096] = {0};
        while ((readed_bytes = read(_socket, client_buffer + current_bytes, sizeof(client_buffer) - current_bytes)) >
               0) {
            _logger->debug("Got {} bytes from socket", readed_bytes);
            current_bytes += readed_bytes;
            // Single block of data readed from the socket could trigger inside actions a multiple times,
            // for example:
            // - read#0: [<command1 start>]
            // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
            while (current_bytes > 0) {
                _logger->debug("Process {} bytes", current_bytes);
                // There is no command yet
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    if (parser.Parse(client_buffer, current_bytes, parsed)) {
                        // There is no command to be launched, continue to parse input stream
                        // Here we are, current chunk finished some command, process it
                        _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }

                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        break;
                    } else {
                        std::memmove(client_buffer, client_buffer + parsed, readed_bytes - parsed);
                        current_bytes -= parsed;
                    }
                }

                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", readed_bytes, arg_remains);
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(arg_remains, std::size_t(readed_bytes));
                    argument_for_command.append(client_buffer, to_read);

                    std::memmove(client_buffer, client_buffer + to_read, readed_bytes - to_read);
                    arg_remains -= to_read;
                    current_bytes -= to_read;
                }

                // Thre is command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    command_to_execute->Execute(*pStorage, argument_for_command, result);

                    // Send response
                    result += "\r\n";
                    replies.push_back(result);
                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            }
        }
        if (readed_bytes == 0 || errno == EAGAIN || errno == EWOULDBLOCK) {
            _logger->debug("Connection closed");
        } else {
            throw std::runtime_error(std::string(strerror(errno)));
        }
    } catch (std::runtime_error &ex) {
        OnError();
    }
}

// See Connection.h
void Connection::DoWrite() {
    if (replies.empty()) {
        return;
    }
    _logger->debug("Write on {} socket", _socket);

    struct iovec iovecs[replies.size()];

    iovecs[0].iov_len = replies[0].size() - cur_position;
    iovecs[0].iov_base = &(replies[0][0]) + cur_position;
    for (int i = 1; i < replies.size(); i++) {
        iovecs[i].iov_len = replies[i].size();
        iovecs[i].iov_base = &(replies[i][0]);
    }

    int written;
    if ((written = writev(_socket, iovecs, replies.size())) <= 0) {
        OnError();
    }
    cur_position += written;

    int i = 0;
    for (; (i >= replies.size() || (cur_position - iovecs[i].iov_len) < 0); i++) {
        cur_position -= iovecs[i].iov_len;
    }
    replies.erase(replies.begin(), replies.begin() + i);
    if (replies.empty()) {
        _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR; // без записи
    }
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
