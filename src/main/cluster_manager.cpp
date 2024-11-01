// Copyright(C) 2024 InfiniFlow, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

module;

#include <vector>

module cluster_manager;

import stl;
import infinity_context;
import logger;
import infinity_exception;
import peer_server_thrift_types;
import wal_manager;
import wal_entry;
import admin_statement;

namespace infinity {

ClusterManager::~ClusterManager() {
    other_node_map_.clear();
    this_node_.reset();
    if (client_to_leader_.get() != nullptr) {
        client_to_leader_->UnInit(true);
    }
    client_to_leader_.reset();
}

Status ClusterManager::InitAsLeader(const String &node_name) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (this_node_.get() != nullptr) {
        return Status::ErrorInit("Init node as leader error: already initialized.");
    }
    this_node_ = MakeShared<NodeInfo>();
    this_node_->node_role_ = NodeRole::kLeader;
    this_node_->node_status_ = NodeStatus::kAlive;
    this_node_->node_name_ = node_name;
    this_node_->ip_address_ = InfinityContext::instance().config()->PeerServerIP();
    this_node_->port_ = InfinityContext::instance().config()->PeerServerPort();

    auto now = std::chrono::system_clock::now();
    auto time_since_epoch = now.time_since_epoch();
    this_node_->last_update_ts_ = std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch).count();

    return Status::OK();
}

Status ClusterManager::InitAsFollower(const String &node_name, const String &leader_ip, i64 leader_port) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (this_node_.get() != nullptr) {
        return Status::ErrorInit("Init node as follower error: already initialized.");
    }
    this_node_ = MakeShared<NodeInfo>();
    this_node_->node_role_ = NodeRole::kFollower;
    this_node_->node_status_ = NodeStatus::kAlive;
    this_node_->node_name_ = node_name;
    this_node_->ip_address_ = InfinityContext::instance().config()->PeerServerIP();
    this_node_->port_ = InfinityContext::instance().config()->PeerServerPort();

    auto now = std::chrono::system_clock::now();
    auto time_since_epoch = now.time_since_epoch();
    this_node_->last_update_ts_ = std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch).count();

    leader_node_ = MakeShared<NodeInfo>();
    leader_node_->node_role_ = NodeRole::kLeader;
    leader_node_->node_status_ = NodeStatus::kInvalid;
    leader_node_->ip_address_ = leader_ip;
    leader_node_->port_ = leader_port;

    Status client_status = Status::OK();
    std::tie(client_to_leader_, client_status) = ClusterManager::ConnectToServerNoLock(node_name, leader_ip, leader_port);
    if (!client_status.ok()) {
        return client_status;
    }

    return client_status;
}

Status ClusterManager::InitAsLearner(const String &node_name, const String &leader_ip, i64 leader_port) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (this_node_.get() != nullptr) {
        return Status::ErrorInit("Init node as learner error: already initialized.");
    }
    this_node_ = MakeShared<NodeInfo>();
    this_node_->node_role_ = NodeRole::kLearner;
    this_node_->node_status_ = NodeStatus::kAlive;
    this_node_->node_name_ = node_name;
    this_node_->ip_address_ = InfinityContext::instance().config()->PeerServerIP();
    this_node_->port_ = InfinityContext::instance().config()->PeerServerPort();

    auto now = std::chrono::system_clock::now();
    auto time_since_epoch = now.time_since_epoch();
    this_node_->last_update_ts_ = std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch).count();

    leader_node_ = MakeShared<NodeInfo>();
    leader_node_->node_role_ = NodeRole::kLeader;
    leader_node_->node_status_ = NodeStatus::kInvalid;
    leader_node_->ip_address_ = leader_ip;
    leader_node_->port_ = leader_port;

    Status client_status = Status::OK();
    std::tie(client_to_leader_, client_status) = ClusterManager::ConnectToServerNoLock(node_name, leader_ip, leader_port);
    if (!client_status.ok()) {
        return client_status;
    }

    return client_status;
}

Status ClusterManager::UnInit(bool not_unregister) {

    if (hb_periodic_thread_.get() != nullptr) {
        {
            std::lock_guard lock(hb_mutex_);
            hb_running_ = false;
            hb_cv_.notify_all();
        }
        hb_periodic_thread_->join();
        hb_periodic_thread_.reset();
    }

    std::unique_lock<std::mutex> lock(mutex_);
    if (!not_unregister) {
        Status status = UnregisterToLeaderNoLock();
    }

    other_node_map_.clear();
    leader_node_.reset();
    this_node_.reset();
    if (client_to_leader_.get() != nullptr) {
        client_to_leader_->UnInit(true);
    }
    client_to_leader_.reset();

    reader_client_map_.clear();
    logs_to_sync_.clear();

    return Status::OK();
}

Status ClusterManager::RegisterToLeader() {
    // Register to leader;
    std::unique_lock<std::mutex> lock(mutex_);
    Status status = RegisterToLeaderNoLock();
    if (!status.ok()) {
        return status;
    }

    this->hb_running_ = true;
    hb_periodic_thread_ = MakeShared<Thread>([this] { this->HeartBeatToLeader(); });
    return status;
}

void ClusterManager::HeartBeatToLeader() {
    // Heartbeat interval
    auto hb_interval = std::chrono::milliseconds(leader_node_->heartbeat_interval_);
    while (true) {
        std::unique_lock hb_lock(this->hb_mutex_);
        this->hb_cv_.wait_for(hb_lock, hb_interval, [&] { return !this->hb_running_; });
        if (!hb_running_) {
            // UnInit cluster manager will set hb_running_ false;
            break;
        }

        if (!client_to_leader_->ServerConnected()) {
            // If peer_client was disconnected, try to reconnect to server.
            Status connect_status = client_to_leader_->Reconnect();
            if (!connect_status.ok()) {
                LOG_ERROR(connect_status.message());
                continue;
            }
        }

        // Update latest update time
        auto hb_now = std::chrono::system_clock::now();
        auto hb_time_since_epoch = hb_now.time_since_epoch();
        this_node_->last_update_ts_ = std::chrono::duration_cast<std::chrono::seconds>(hb_time_since_epoch).count();

        // Send heartbeat
        SharedPtr<HeartBeatPeerTask> hb_task = MakeShared<HeartBeatPeerTask>(this_node_->node_name_,
                                                                             this_node_->node_role_,
                                                                             this_node_->ip_address_,
                                                                             this_node_->port_,
                                                                             storage_->txn_manager()->CurrentTS());
        client_to_leader_->Send(hb_task);
        hb_task->Wait();

        if (hb_task->error_code_ != 0) {
            LOG_ERROR(fmt::format("Can't connect to leader: {}, {}:{}, error: {}",
                                  leader_node_->node_name_,
                                  leader_node_->ip_address_,
                                  leader_node_->port_,
                                  hb_task->error_message_));
            leader_node_->node_status_ = NodeStatus::kTimeout;

            continue;
        }

        // Prepare the update time of leader
        hb_now = std::chrono::system_clock::now();
        hb_time_since_epoch = hb_now.time_since_epoch();

        std::unique_lock<std::mutex> cluster_manager_lock(mutex_);
        // Update leader info
        leader_node_->node_status_ = NodeStatus::kAlive;
        leader_node_->last_update_ts_ = std::chrono::duration_cast<std::chrono::seconds>(hb_time_since_epoch).count();
        leader_node_->leader_term_ = hb_task->leader_term_;

        // Update heartbeat sending count
        ++this_node_->heartbeat_count_;

        // Update the non-leader node info from leader
        UpdateNodeInfoNoLock(hb_task->other_nodes_);

        // Check if leader can't send message to this reader node
        this_node_->node_status_ = hb_task->sender_status_;
    }
    return;
}

void ClusterManager::CheckHeartBeat() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (this_node_->node_role_ != NodeRole::kLeader) {
        String error_message = "Invalid node role.";
        UnrecoverableError(error_message);
    }
    hb_periodic_thread_ = MakeShared<Thread>([this] { this->CheckHeartBeatInner(); });
}

void ClusterManager::CheckHeartBeatInner() {
    if (this_node_->node_role_ != NodeRole::kLeader) {
        String error_message = "Invalid node role.";
        UnrecoverableError(error_message);
    }
    // this_node_ is the leader;
    auto hb_interval = std::chrono::milliseconds(this_node_->heartbeat_interval_);
    while (true) {
        std::unique_lock hb_lock(this->hb_mutex_);
        this->hb_cv_.wait_for(hb_lock, hb_interval, [&] { return !this->hb_running_; });
        if (!hb_running_) {
            // UnInit cluster manager will set hb_running_ false;
            break;
        }

        auto now = std::chrono::system_clock::now();
        auto time_since_epoch = now.time_since_epoch();
        this_node_->last_update_ts_ = std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch).count();

        for (auto &[node_name, node_info] : other_node_map_) {
            if (node_info->node_status_ == NodeStatus::kAlive) {
                if (node_info->last_update_ts_ + 2 * this_node_->heartbeat_interval_ < this_node_->last_update_ts_) {
                    node_info->node_status_ = NodeStatus::kTimeout;
                    LOG_INFO(fmt::format("Node {} is timeout", node_name));
                }
            }
        }
    }
}

Status ClusterManager::RegisterToLeaderNoLock() {
    // Register to leader, used by follower and learner
    SharedPtr<RegisterPeerTask> register_peer_task = nullptr;
    if (storage_->reader_init_phase() == ReaderInitPhase::kPhase2) {
        register_peer_task = MakeShared<RegisterPeerTask>(this_node_->node_name_,
                                                          this_node_->node_role_,
                                                          this_node_->ip_address_,
                                                          this_node_->port_,
                                                          storage_->txn_manager()->CurrentTS());
    } else {
        register_peer_task =
            MakeShared<RegisterPeerTask>(this_node_->node_name_, this_node_->node_role_, this_node_->ip_address_, this_node_->port_, 0);
    }

    client_to_leader_->Send(register_peer_task);
    register_peer_task->Wait();

    Status status = Status::OK();
    if (register_peer_task->error_code_ != 0) {
        status.code_ = static_cast<ErrorCode>(register_peer_task->error_code_);
        status.msg_ = MakeUnique<String>(register_peer_task->error_message_);
        return status;
    }
    auto now = std::chrono::system_clock::now();
    auto time_since_epoch = now.time_since_epoch();
    leader_node_->last_update_ts_ = std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch).count();
    leader_node_->node_name_ = register_peer_task->leader_name_;
    leader_node_->node_status_ = NodeStatus::kAlive;
    //    client_to_leader_->SetPeerNode(NodeRole::kLeader, register_peer_task->leader_name_, register_peer_task->update_time_);
    // Start HB thread.
    if (register_peer_task->heartbeat_interval_ == 0) {
        leader_node_->heartbeat_interval_ = 1000; // 1000 ms by default
    } else {
        leader_node_->heartbeat_interval_ = register_peer_task->heartbeat_interval_;
    }

    return status;
}

Status ClusterManager::UnregisterToLeaderNoLock() {
    Status status = Status::OK();
    if (this_node_->node_role_ == NodeRole::kFollower or this_node_->node_role_ == NodeRole::kLearner) {
        if (leader_node_->node_status_ == NodeStatus::kAlive) {
            // Leader is alive, need to unregister
            SharedPtr<UnregisterPeerTask> unregister_task = MakeShared<UnregisterPeerTask>(this_node_->node_name_);
            client_to_leader_->Send(unregister_task);
            unregister_task->Wait();
            if (unregister_task->error_code_ != 0) {
                LOG_ERROR(fmt::format("Fail to unregister from leader: {}", unregister_task->error_message_));
                status.code_ = static_cast<ErrorCode>(unregister_task->error_code_);
                status.msg_ = MakeUnique<String>(unregister_task->error_message_);
            }
        }
    }
    return status;
}

Tuple<SharedPtr<PeerClient>, Status> ClusterManager::ConnectToServerNoLock(const String &from_node_name, const String &server_ip, i64 server_port) {
    SharedPtr<PeerClient> client = MakeShared<PeerClient>(from_node_name, server_ip, server_port);
    Status client_status = client->Init();
    if (!client_status.ok()) {
        return {nullptr, client_status};
    }
    return {client, client_status};
}

Status ClusterManager::SendLogs(const String &node_name,
                                const SharedPtr<PeerClient> &peer_client,
                                const Vector<SharedPtr<String>> &logs,
                                bool synchronize,
                                bool on_register) {
    SharedPtr<SyncLogTask> sync_log_task = MakeShared<SyncLogTask>(node_name, logs, on_register);
    peer_client->Send(sync_log_task);

    Status status = Status::OK();
    if (synchronize) {
        sync_log_task->Wait();
    } else {
        return status;
    }

    if (sync_log_task->error_code_ != 0) {
        LOG_ERROR(fmt::format("Fail to send log follower: {}, error message: {}", node_name, sync_log_task->error_message_));
        status.code_ = static_cast<ErrorCode>(sync_log_task->error_code_);
        status.msg_ = MakeUnique<String>(sync_log_task->error_message_);
    }
    return status;
}

Status ClusterManager::GetReadersInfo(Vector<SharedPtr<NodeInfo>> &followers,
                                      Vector<SharedPtr<PeerClient>> &follower_clients,
                                      Vector<SharedPtr<NodeInfo>> &learners,
                                      Vector<SharedPtr<PeerClient>> &learner_clients) {
    if (this_node_->node_role_ != NodeRole::kLeader) {
        return Status::InvalidNodeRole("Expect leader node");
    }
    std::unique_lock<std::mutex> lock(mutex_);
    for (const auto &node_info_pair : other_node_map_) {
        if (node_info_pair.second->node_role_ == NodeRole::kFollower && node_info_pair.second->node_status_ == NodeStatus::kAlive) {
            followers.emplace_back(node_info_pair.second);
            follower_clients.emplace_back(reader_client_map_[node_info_pair.first]);
        }
        if (node_info_pair.second->node_role_ == NodeRole::kLearner && node_info_pair.second->node_status_ == NodeStatus::kAlive) {
            learners.emplace_back(node_info_pair.second);
            learner_clients.emplace_back(reader_client_map_[node_info_pair.first]);
        }
    }

    return Status::OK();
}

Status ClusterManager::AddNodeInfo(const SharedPtr<NodeInfo> &node_info) {
    // Only used by Leader on follower/learner registration.
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (this_node_->node_role_ != NodeRole::kLeader) {
            String error_message = "Non-leader role can't add other node.";
            UnrecoverableError(error_message);
        }

        // Add by register
        auto iter = other_node_map_.find(node_info->node_name_);
        if (iter != other_node_map_.end()) {
            // Duplicated node
            return Status::DuplicateNode(node_info->node_name_);
        }
    }

    // Connect to follower/learner server.
    auto [client_to_follower, client_status] =
        ClusterManager::ConnectToServerNoLock(this_node_->node_name_, node_info->ip_address_, node_info->port_);
    if (!client_status.ok()) {
        return client_status;
    }

    Status log_sending_status = SyncLogsOnRegistration(node_info, client_to_follower);
    if (log_sending_status.ok()) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (this_node_->node_role_ != NodeRole::kLeader) {
            String error_message = "Invalid node role.";
            UnrecoverableError(error_message);
        }

        // Add by register
        auto iter = other_node_map_.find(node_info->node_name_);
        if (iter == other_node_map_.end()) {
            // Not find, so add the node
            other_node_map_.emplace(node_info->node_name_, node_info);
        } else {
            // Duplicated node
            // TODO: If the exist node lost connection, or other malfunction, use new node_info to replace it.
            return Status::DuplicateNode(node_info->node_name_);
        }

        reader_client_map_.emplace(node_info->node_name_, client_to_follower);
    }

    return log_sending_status;
}

Status ClusterManager::RemoveNodeInfo(const String &node_name) {
    // Used by leader to remove node
    if (node_name == this_node_->node_name_) {
        return Status::InvalidNodeRole(fmt::format("Can't remove current node: {}", node_name));
    }

    NodeRole server_role = InfinityContext::instance().GetServerRole();
    if (server_role != NodeRole::kLeader) {
        return Status::InvalidNodeRole(fmt::format("Can't remove node in {} mode", ToString(server_role)));
    }

    SharedPtr<PeerClient> client_{nullptr};
    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto iter = other_node_map_.find(node_name);
        if (iter != other_node_map_.end()) {
            iter->second->node_status_ = NodeStatus::kRemoved;
        } else {
            return Status::NotExistNode(node_name);
        }

        client_ = reader_client_map_[node_name];
        reader_client_map_.erase(node_name);
    }
    SharedPtr<ChangeRoleTask> change_role_task = MakeShared<ChangeRoleTask>(node_name, "admin");
    client_->Send(change_role_task);
    change_role_task->Wait();

    {
        std::unique_lock<std::mutex> lock(mutex_);
        other_node_map_.erase(node_name);
    }

    Status status = Status::OK();
    if (change_role_task->error_code_ != 0) {
        LOG_ERROR(fmt::format("Fail to change {} to the role of ADMIN, error: ", node_name, change_role_task->error_message_));
        status.code_ = static_cast<ErrorCode>(change_role_task->error_code_);
        status.msg_ = MakeUnique<String>(change_role_task->error_message_);
    }
    return status;
}

Status ClusterManager::UpdateNodeByLeader(const String &node_name, UpdateNodeOp update_node_op) {
    // Only used in leader mode.
    std::unique_lock<std::mutex> lock(mutex_);

    auto iter = other_node_map_.find(node_name);
    if (iter == other_node_map_.end()) {
        return Status::NotExistNode(fmt::format("Attempt to remove non-exist node: {}", node_name));
    } else {
        switch (update_node_op) {
            case UpdateNodeOp::kRemove: {
                other_node_map_.erase(node_name);
                break;
            }
            case UpdateNodeOp::kLostConnection: {
                // Can't connect to the node
                iter->second->node_status_ = NodeStatus::kLostConnection;
                break;
            }
        }
    }

    auto client_iter = reader_client_map_.find(node_name);
    if (client_iter == reader_client_map_.end()) {
        return Status::NotExistNode(fmt::format("Attempt to disconnect from non-exist node: {}", node_name));
    } else {
        switch (update_node_op) {
            case UpdateNodeOp::kRemove: {
                client_iter->second->UnInit(true);
                reader_client_map_.erase(node_name);
                break;
            }
            case UpdateNodeOp::kLostConnection: {
                client_iter->second->UnInit(false);
                reader_client_map_.erase(node_name);
                break;
            }
        }
    }

    return Status::OK();
}

Status ClusterManager::UpdateNodeInfoByHeartBeat(const SharedPtr<NodeInfo> &non_leader_node,
                                                 Vector<infinity_peer_server::NodeInfo> &other_nodes,
                                                 i64 &leader_term,
                                                 infinity_peer_server::NodeStatus::type &sender_status) {
    // Used by leader
    std::unique_lock<std::mutex> lock(mutex_);
    if (this_node_->node_role_ != NodeRole::kLeader) {
        String error_message = "Invalid node role.";
        UnrecoverableError(error_message);
    }

    other_nodes.reserve(other_node_map_.size());
    leader_term = this_node_->leader_term_;
    bool non_leader_node_found{false};
    for (const auto &node_info_pair : other_node_map_) {
        NodeInfo *other_node = node_info_pair.second.get();
        if (other_node->node_name_ == non_leader_node->node_name_) {
            if (other_node->ip_address_ != non_leader_node->ip_address_ or other_node->port_ != non_leader_node->port_) {
                String error_message = fmt::format("Node {}, IP address: {} or port: {} is changed.",
                                                   other_node->node_name_,
                                                   other_node->ip_address_,
                                                   other_node->port_);
                return Status::NodeInfoUpdated(ToString(other_node->node_role_));
            }
            // Found the node
            switch (other_node->node_status_) {
                case NodeStatus::kAlive:
                case NodeStatus::kTimeout: {
                    // just update the timestamp
                    other_node->txn_timestamp_ = non_leader_node->txn_timestamp_;
                    auto now = std::chrono::system_clock::now();
                    auto time_since_epoch = now.time_since_epoch();
                    other_node->last_update_ts_ = std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch).count();
                    ++other_node->heartbeat_count_;
                    sender_status = infinity_peer_server::NodeStatus::type::kAlive;
                    break;
                }
                case NodeStatus::kRemoved: {
                    sender_status = infinity_peer_server::NodeStatus::type::kRemoved;
                }
                case NodeStatus::kLostConnection: {
                    LOG_ERROR(fmt::format("Node {} from {}:{} can't connected, but still can receive heartbeat.",
                                          other_node->node_name_,
                                          other_node->ip_address_,
                                          other_node->port_));
                    sender_status = infinity_peer_server::NodeStatus::type::kLostConnection;
                    break;
                }
                case NodeStatus::kInvalid: {
                    UnrecoverableError("Invalid node status");
                }
            }

            non_leader_node_found = true;
        } else {
            infinity_peer_server::NodeInfo thrift_node_info;
            thrift_node_info.node_name = other_node->node_name_;
            thrift_node_info.node_ip = other_node->ip_address_;
            thrift_node_info.node_port = other_node->port_;
            thrift_node_info.txn_timestamp = other_node->txn_timestamp_;
            thrift_node_info.hb_count = other_node->heartbeat_count_;
            switch (other_node->node_role_) {
                case NodeRole::kLeader: {
                    thrift_node_info.node_type = infinity_peer_server::NodeType::kLeader;
                    break;
                }
                case NodeRole::kFollower: {
                    thrift_node_info.node_type = infinity_peer_server::NodeType::kFollower;
                    break;
                }
                case NodeRole::kLearner: {
                    thrift_node_info.node_type = infinity_peer_server::NodeType::kLearner;
                    break;
                }
                default: {
                    String error_message = fmt::format("Invalid node role of node: {}", other_node->node_name_);
                    return Status::InvalidNodeRole(ToString(other_node->node_role_));
                }
            }
            switch (other_node->node_status_) {
                case NodeStatus::kAlive: {
                    thrift_node_info.node_status = infinity_peer_server::NodeStatus::kAlive;
                    break;
                }
                case NodeStatus::kTimeout: {
                    thrift_node_info.node_status = infinity_peer_server::NodeStatus::kTimeout;
                    break;
                }
                default: {
                    String error_message = fmt::format("Invalid node status of node: {}", other_node->node_name_);
                    return Status::InvalidNodeStatus(error_message);
                }
            }

            other_nodes.emplace_back(thrift_node_info);
        }
    }

    if (!non_leader_node_found) {
        other_node_map_.emplace(non_leader_node->node_name_, non_leader_node);
    }
    return Status::OK();
}

Status ClusterManager::SyncLogsOnRegistration(const SharedPtr<NodeInfo> &non_leader_node, const SharedPtr<PeerClient> &peer_client) {
    // Used by leader on registration phase

    // Check leader WAL and get the diff log
    LOG_TRACE("Leader will get the log diff");
    WalManager *wal_manager = storage_->wal_manager();
    Vector<SharedPtr<String>> wal_strings = wal_manager->GetDiffWalEntryString(non_leader_node->txn_timestamp_);

    // Leader will send the WALs
    LOG_TRACE(fmt::format("Leader will send the diff logs count: {} to {} synchronously", wal_strings.size(), non_leader_node->node_name_));
    return SendLogs(non_leader_node->node_name_, peer_client, wal_strings, true, true);
}

void ClusterManager::PrepareLogs(const SharedPtr<String> &log_string) { logs_to_sync_.emplace_back(log_string); }

Status ClusterManager::SyncLogs() {
    LOG_TRACE("Sync logs to follower and async logs to learner");
    Set<String> sent_nodes;
    while (true) {
        // Get follower and learner node
        Vector<SharedPtr<NodeInfo>> followers;
        Vector<SharedPtr<PeerClient>> follower_clients;
        Vector<SharedPtr<NodeInfo>> learners;
        Vector<SharedPtr<PeerClient>> learner_clients;

        Status status = GetReadersInfo(followers, follower_clients, learners, learner_clients);
        if (!status.ok()) {
            return status;
        }

        SizeT follower_count = followers.size();
        SizeT learner_count = learners.size();

        if (follower_count != follower_clients.size() && learner_count != learner_clients.size()) {
            return Status::UnexpectedError("Node info and node client count isn't match");
        }

        // Replicate logs to follower
        for (SizeT idx = 0; idx < follower_count; ++idx) {
            const String &follower_name = followers[idx]->node_name_;
            if (!sent_nodes.contains(follower_name)) {
                status = SendLogs(follower_name, follower_clients[idx], logs_to_sync_, true, false);
                if (status.ok()) {
                    sent_nodes.insert(follower_name);
                }
            }
        }

        // Replicate logs to learner
        for (SizeT idx = 0; idx < learner_count; ++idx) {
            const String &learner_name = learners[idx]->node_name_;
            if (!sent_nodes.contains(learner_name)) {
                status = SendLogs(learner_name, learner_clients[idx], logs_to_sync_, false, false);
                if (status.ok()) {
                    sent_nodes.insert(learner_name);
                }
            }
        }

        if (sent_nodes.size() == follower_count + learner_count) {
            logs_to_sync_.clear();
            break;
        }
    }
    return Status::OK();
}

Status ClusterManager::SetFollowerNumber(SizeT new_follower_number) {
    if (new_follower_number > 5) {
        return Status::NotSupport("Attempt to set follower count larger than 5.");
    }

    // Check current follower count, if new count is less, leader will downgrade some followers to learner
    follower_count_ = new_follower_number;
    return Status::OK();
}

SizeT ClusterManager::GetFollowerNumber() const { return follower_count_; }

Status ClusterManager::UpdateNodeInfoNoLock(const Vector<SharedPtr<NodeInfo>> &info_of_nodes) {
    // Only follower and learner will use this function.
    HashMap<String, bool> exist_node_name_map;
    exist_node_name_map.reserve(other_node_map_.size());
    for (const auto &node_pair : other_node_map_) {
        exist_node_name_map.emplace(node_pair.first, false);
    }

    auto now = std::chrono::system_clock::now();
    auto time_since_epoch = now.time_since_epoch();

    for (const SharedPtr<NodeInfo> &node_info : info_of_nodes) {
        auto iter = other_node_map_.find(node_info->node_name_);
        if (iter == other_node_map_.end()) {
            node_info->last_update_ts_ = std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch).count();
            other_node_map_.emplace(node_info->node_name_, node_info);
        } else {
            iter->second->txn_timestamp_ = node_info->txn_timestamp_;
            iter->second->ip_address_ = node_info->ip_address_;
            iter->second->port_ = node_info->port_;
            iter->second->node_role_ = node_info->node_role_;
            iter->second->node_status_ = node_info->node_status_;
            iter->second->last_update_ts_ = std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch).count();

            exist_node_name_map[node_info->node_name_] = true;
        }
    }

    for (const auto &exist_node_pair : exist_node_name_map) {
        if (!exist_node_pair.second) {
            other_node_map_.erase(exist_node_pair.first);
        }
    }

    return Status::OK();
}

Status ClusterManager::ApplySyncedLogNolock(const Vector<String> &synced_logs) {
    WalManager *wal_manager = storage_->wal_manager();
    TransactionID last_txn_id = 0;
    TxnTimeStamp last_commit_ts = 0;
    for (auto &log_str : synced_logs) {
        const i32 entry_size = log_str.size();
        const char *ptr = log_str.data();
        SharedPtr<WalEntry> entry = WalEntry::ReadAdv(ptr, entry_size);
        LOG_DEBUG(fmt::format("WAL Entry: {}", entry->ToString()));
        last_txn_id = entry->txn_id_;
        last_commit_ts = entry->commit_ts_;
        wal_manager->ReplayWalEntry(*entry, false);
    }

    LOG_INFO(fmt::format("Replicated from leader: latest txn commit_ts: {}, latest txn id: {}", last_commit_ts, last_txn_id));
    storage_->catalog()->next_txn_id_ = last_txn_id;
    storage_->wal_manager()->UpdateCommitState(last_commit_ts, 0);
    storage_->txn_manager()->SetStartTS(last_commit_ts);
    return Status::OK();
}

Status ClusterManager::ContinueStartup(const Vector<String> &synced_logs) {
    WalManager *wal_manager = storage_->wal_manager();
    bool is_checkpoint = true;
    TxnTimeStamp last_commit_ts;
    for (auto &log_str : synced_logs) {
        const i32 entry_size = log_str.size();
        const char *ptr = log_str.data();
        SharedPtr<WalEntry> entry = WalEntry::ReadAdv(ptr, entry_size);
        for (const auto &cmd : entry->cmds_) {
            if (is_checkpoint) {
                if (cmd->GetType() != WalCommandType::CHECKPOINT) {
                    is_checkpoint = false;
                }
            } else {
                if (cmd->GetType() == WalCommandType::CHECKPOINT) {
                    UnrecoverableError("Expect non-checkpoint log");
                }
            }
        }
        LOG_DEBUG(fmt::format("WAL Entry: {}", entry->ToString()));
        wal_manager->ReplayWalEntry(*entry, true);
        last_commit_ts = entry->commit_ts_;
    }

    storage_->SetReaderStorageContinue(last_commit_ts + 1);
    return Status::OK();
}

Vector<SharedPtr<NodeInfo>> ClusterManager::ListNodes() const {
    // ADMIN SHOW NODES;
    std::unique_lock<std::mutex> lock(mutex_);
    Vector<SharedPtr<NodeInfo>> result;
    result.reserve(other_node_map_.size() + 2);
    result.emplace_back(this_node_);
    if (this_node_->node_role_ == NodeRole::kFollower or this_node_->node_role_ == NodeRole::kLearner) {
        result.emplace_back(leader_node_);
    }
    for (const auto &node_info_pair : other_node_map_) {
        result.emplace_back(node_info_pair.second);
    }

    return result;
}

Tuple<Status, SharedPtr<NodeInfo>> ClusterManager::GetNodeInfoPtrByName(const String &node_name) const {
    std::unique_lock<std::mutex> lock(mutex_);

    if (this_node_->node_role_ == NodeRole::kAdmin or this_node_->node_role_ == NodeRole::kStandalone or
        this_node_->node_role_ == NodeRole::kUnInitialized) {
        String error_message = "Error node role type";
        UnrecoverableError(error_message);
    }

    if (this_node_->node_role_ == NodeRole::kFollower or this_node_->node_role_ == NodeRole::kLearner) {
        if (node_name == leader_node_->node_name_) {
            // Show leader
            return {Status::OK(), leader_node_};
        }
    }

    if (node_name == this_node_->node_name_) {
        // Show this node
        return {Status::OK(), this_node_};
    }

    auto iter = other_node_map_.find(node_name);
    if (iter == other_node_map_.end()) {
        return {Status::NotExistNode(node_name), nullptr};
    }
    return {Status::OK(), iter->second};
}

SharedPtr<NodeInfo> ClusterManager::ThisNode() const {
    // ADMIN SHOW NODE;
    std::unique_lock<std::mutex> lock(mutex_);
    return this->this_node_;
}

} // namespace infinity
