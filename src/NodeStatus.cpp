#include "NodeStatus.h"
#include "NodeInfo.h"
#include <algorithm>
#include <mutex>
#include <pstl/glue_algorithm_defs.h>

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

bool NodeStatus::isConflicting(mac_t mac_address_src, mac_t &mac_conflict) {
    shared_lock_t lock(mutex_);
    for (auto &node : nodes_) {
        if (node.mac_address_ != mac_address_src && node.is_send_) {
            mac_conflict = node.mac_address_;
            return true;
        }
    }
    return false;
}