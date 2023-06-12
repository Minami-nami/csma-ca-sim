#include "NodeStatus.h"
#include "NodeInfo.h"
#include "qdatetime.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <mutex>
#include <tuple>
#include <utility>

NodeInfo &NodeStatus::addNode(NodeInfo &&node) {
    unique_lock_t lock(mutex_);
    nodes_.push_back(node);
    return nodes_.back();
}

NodeInfo &NodeStatus::getNode(mac_t mac_address) {
    shared_lock_t lock(mutex_);
    auto          it = std::find_if(nodes_.begin(), nodes_.end(), [mac_address](const NodeInfo &node) { return node.mac_address_ == mac_address; });
    if (it != nodes_.end()) return *it;
    throw std::runtime_error("Node not found");
}

void NodeStatus::removeNode(mac_t mac_address) {
    unique_lock_t lock(mutex_);
    auto          it = std::remove_if(nodes_.begin(), nodes_.end(), [mac_address](const NodeInfo &node) { return node.mac_address_ == mac_address; });
}

bool NodeStatus::isConflicting(mac_t mac_address_src, mac_t &mac_conflict, bool &send) {
    unique_lock_t lock(list_mutex_);
    auto          it_prev = lastsend_list_.begin(), it_next = ++lastsend_list_.begin();
    while (it_next != lastsend_list_.end()) {
        if (std::get<0>(*it_prev).msecsTo(std::get<0>(*it_next)) > 8000) {  // time out
            it_next = it_prev = lastsend_list_.erase(lastsend_list_.begin(), it_next);
            ++it_next;
        }
        else if (std::get<1>(*it_prev) == mac_address_src && std::get<1>(*it_next) != mac_address_src) {
            mac_conflict = std::get<1>(*it_next);
            send         = true;

            if (std::get<2>(*it_next) == true) lastsend_list_.erase(it_prev, ++it_next);
            std::get<2>(*it_prev) = true;
            return true;
        }
        if (std::get<1>(*it_next) == mac_address_src && std::get<1>(*it_prev) != mac_address_src) {
            mac_conflict = std::get<1>(*it_prev);
            send         = false;

            if (std::get<2>(*it_prev) == true) lastsend_list_.erase(it_prev, ++it_next);
            std::get<2>(*it_next) = true;
            return true;
        }
    }
    send = false;
    return false;
}

void NodeStatus::addLastSend(QTime lastsend, mac_t lastmac) {
    unique_lock_t lock_time(list_mutex_);
    lastsend_list_.push_back(std::tuple<QTime, mac_t, bool>(lastsend, lastmac, false));
}

std::list<NodeInfo> &NodeStatus::getNodes() noexcept {
    shared_lock_t lock(mutex_);
    return nodes_;
}