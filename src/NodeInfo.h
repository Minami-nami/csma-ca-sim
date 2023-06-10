#pragma once
#include <QTime>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>

class NodeInfo {
public:
    using mac_t               = std::byte;
    using ip_t                = boost::asio::ip::address;
    using port_t              = uint16_t;
    using mutex_t             = std::mutex;
    using shared_mutex_t      = std::shared_mutex;
    using shared_lock_t       = std::shared_lock<shared_mutex_t>;
    using unique_lock_t       = std::unique_lock<shared_mutex_t>;
    using socket_ptr_t        = std::weak_ptr<boost::asio::ip::tcp::socket>;
    using shared_socket_ptr_t = std::shared_ptr<boost::asio::ip::tcp::socket>;

public:
    NodeInfo(mac_t mac_address, ip_t ip_address, port_t port, shared_socket_ptr_t socket);
    NodeInfo(NodeInfo &&node);
    NodeInfo(const NodeInfo &node);
    NodeInfo &operator=(NodeInfo &&node);
    NodeInfo &operator=(const NodeInfo &node);

private:
    shared_mutex_t mutex_;

public:
    ip_t         ip_address_;
    QTime        time_;
    port_t       port_;
    bool         is_send_;
    mac_t        mac_address_;
    socket_ptr_t socket_;

    void setSend(bool is_send);

    void setTime(QTime time);

    void setIP(ip_t ip_address);

    void setPort(port_t port);

    void setMac(mac_t mac_address);

    bool isSend();

    QTime getTime();

    ip_t getIP();

    port_t getPort();

    mac_t getMac();
};