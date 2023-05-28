#pragma once

#include "qdatetime.h"
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
    NodeInfo(mac_t mac_address, ip_t ip_address, port_t port, shared_socket_ptr_t socket)
        : mac_address_(mac_address), ip_address_(ip_address), port_(port), socket_(socket), is_send_(false), time_(){};
    NodeInfo() = default;
    NodeInfo(NodeInfo &&node) : mac_address_(node.mac_address_), ip_address_(node.ip_address_), port_(node.port_), socket_(node.socket_), is_send_(node.is_send_), time_(node.time_){};
    NodeInfo(const NodeInfo &node) : mac_address_(node.mac_address_), ip_address_(node.ip_address_), port_(node.port_), socket_(node.socket_), is_send_(node.is_send_), time_(node.time_){};
    NodeInfo &operator=(NodeInfo &&node) {
        mac_address_ = node.mac_address_;
        ip_address_  = node.ip_address_;
        port_        = node.port_;
        socket_      = node.socket_;
        is_send_     = node.is_send_;
        time_        = node.time_;
        return *this;
    };
    NodeInfo &operator=(const NodeInfo &node) {
        mac_address_ = node.mac_address_;
        ip_address_  = node.ip_address_;
        port_        = node.port_;
        socket_      = node.socket_;
        is_send_     = node.is_send_;
        time_        = node.time_;
        return *this;
    };

private:
    shared_mutex_t mutex_;

public:
    ip_t         ip_address_;
    QTime        time_;
    port_t       port_;
    bool         is_send_;
    mac_t        mac_address_;
    socket_ptr_t socket_;

    void setSend(bool is_send) {
        unique_lock_t lock(mutex_);
        is_send_ = is_send;
    };

    void setTime(QTime time) {
        unique_lock_t lock(mutex_);
        time_ = time;
    };

    void setIP(ip_t ip_address) {
        unique_lock_t lock(mutex_);
        ip_address_ = ip_address;
    };

    void setPort(port_t port) {
        unique_lock_t lock(mutex_);
        port_ = port;
    };

    void setMac(mac_t mac_address) {
        unique_lock_t lock(mutex_);
        mac_address_ = mac_address;
    };

    bool isSend() {
        shared_lock_t lock(mutex_);
        return is_send_;
    };

    QTime getTime() {
        shared_lock_t lock(mutex_);
        return time_;
    };

    ip_t getIP() {
        shared_lock_t lock(mutex_);
        return ip_address_;
    };

    port_t getPort() {
        shared_lock_t lock(mutex_);
        return port_;
    };

    mac_t getMac() {
        shared_lock_t lock(mutex_);
        return mac_address_;
    };
};