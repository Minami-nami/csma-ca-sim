#pragma once
#include "NodeInfo.h"
#include <list>
#include <shared_mutex>

class NodeStatus {
public:
    using mac_t          = NodeInfo::mac_t;
    using mutex_t        = std::mutex;
    using socket_ptr_t   = std::shared_ptr<boost::asio::ip::tcp::socket>;
    using shared_mutex_t = std::shared_mutex;
    using shared_lock_t  = std::shared_lock<shared_mutex_t>;
    using unique_lock_t  = std::unique_lock<shared_mutex_t>;

public:
    NodeStatus() = default;
    NodeInfo            &addNode(NodeInfo &&node);
    void                 removeNode(mac_t mac_address);
    NodeInfo            &getNode(mac_t mac_address);
    bool                 isConflicting(mac_t mac_address_src, mac_t &mac_conflict);
    std::list<NodeInfo> &getNodes() noexcept {
        shared_lock_t lock(mutex_);
        return nodes_;
    }

private:
    std::list<NodeInfo> nodes_;
    shared_mutex_t      mutex_;
};