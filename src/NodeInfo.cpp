#include "NodeInfo.h"

NodeInfo::NodeInfo(mac_t mac_address, ip_t ip_address, port_t port, shared_socket_ptr_t socket)
    : mac_address_(mac_address), ip_address_(ip_address), port_(port), socket_(socket), is_send_(false), time_(){};

NodeInfo::NodeInfo(NodeInfo &&node) : mac_address_(node.mac_address_), ip_address_(node.ip_address_), port_(node.port_), socket_(node.socket_), is_send_(node.is_send_), time_(node.time_){};

NodeInfo::NodeInfo(const NodeInfo &node) : mac_address_(node.mac_address_), ip_address_(node.ip_address_), port_(node.port_), socket_(node.socket_), is_send_(node.is_send_), time_(node.time_){};

NodeInfo &NodeInfo::operator=(NodeInfo &&node) {
    mac_address_ = node.mac_address_;
    ip_address_  = node.ip_address_;
    port_        = node.port_;
    socket_      = node.socket_;
    is_send_     = node.is_send_;
    time_        = node.time_;
    return *this;
};

NodeInfo &NodeInfo::operator=(const NodeInfo &node) {
    mac_address_ = node.mac_address_;
    ip_address_  = node.ip_address_;
    port_        = node.port_;
    socket_      = node.socket_;
    is_send_     = node.is_send_;
    time_        = node.time_;
    return *this;
};

void NodeInfo::setSend(bool is_send) {
    unique_lock_t lock(mutex_);
    is_send_ = is_send;
};

void NodeInfo::setTime(QTime time) {
    unique_lock_t lock(mutex_);
    time_ = time;
};

void NodeInfo::setIP(ip_t ip_address) {
    unique_lock_t lock(mutex_);
    ip_address_ = ip_address;
};

void NodeInfo::setPort(port_t port) {
    unique_lock_t lock(mutex_);
    port_ = port;
};

void NodeInfo::setMac(mac_t mac_address) {
    unique_lock_t lock(mutex_);
    mac_address_ = mac_address;
};

bool NodeInfo::isSend() {
    shared_lock_t lock(mutex_);
    return is_send_;
};

QTime NodeInfo::getTime() {
    shared_lock_t lock(mutex_);
    return time_;
};

NodeInfo::ip_t NodeInfo::getIP() {
    shared_lock_t lock(mutex_);
    return ip_address_;
};

NodeInfo::port_t NodeInfo::getPort() {
    shared_lock_t lock(mutex_);
    return port_;
};

NodeInfo::mac_t NodeInfo::getMac() {
    shared_lock_t lock(mutex_);
    return mac_address_;
};