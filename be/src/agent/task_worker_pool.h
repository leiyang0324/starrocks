// This file is made available under Elastic License 2.0.
// This file is based on code available under the Apache license here:
//   https://github.com/apache/incubator-doris/blob/master/be/src/agent/task_worker_pool.h

// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef STARROCKS_BE_SRC_TASK_WORKER_POOL_H
#define STARROCKS_BE_SRC_TASK_WORKER_POOL_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "agent/status.h"
#include "agent/utils.h"
#include "gen_cpp/AgentService_types.h"
#include "gen_cpp/HeartbeatService_types.h"
#include "storage/olap_define.h"
#include "storage/storage_engine.h"

namespace starrocks {

class ExecEnv;

class TaskWorkerPool {
public:
    enum TaskWorkerType {
        CREATE_TABLE,
        DROP_TABLE,
        PUSH,
        REALTIME_PUSH,
        PUBLISH_VERSION,
        // Deprecated
        CLEAR_ALTER_TASK,
        CLEAR_TRANSACTION_TASK,
        DELETE,
        ALTER_TABLE,
        // Deprecated
        QUERY_SPLIT_KEY,
        CLONE,
        STORAGE_MEDIUM_MIGRATE,
        CHECK_CONSISTENCY,
        REPORT_TASK,
        REPORT_DISK_STATE,
        REPORT_OLAP_TABLE,
        UPLOAD,
        DOWNLOAD,
        MAKE_SNAPSHOT,
        RELEASE_SNAPSHOT,
        MOVE,
        RECOVER_TABLET,
        UPDATE_TABLET_META_INFO
    };

    typedef void* (*CALLBACK_FUNCTION)(void*);

    TaskWorkerPool(const TaskWorkerType task_worker_type, ExecEnv* env, const TMasterInfo& master_info, int worker_num);
    virtual ~TaskWorkerPool();

    // Start the task worker callback thread
    virtual void start();

    // Submit task to task pool
    //
    // Input parameters:
    // * task: the task need callback thread to do
    virtual void submit_task(const TAgentTaskRequest& task);

private:
    bool _register_task_info(const TTaskType::type task_type, int64_t signature);
    void _remove_task_info(const TTaskType::type task_type, int64_t signature);
    void _spawn_callback_worker_thread(CALLBACK_FUNCTION callback_func);
    void _finish_task(const TFinishTaskRequest& finish_task_request);
    uint32_t _get_next_task_index(int32_t thread_count, std::deque<TAgentTaskRequest>& tasks, TPriority::type priority);

    static void* _create_tablet_worker_thread_callback(void* arg_this);
    static void* _drop_tablet_worker_thread_callback(void* arg_this);
    static void* _push_worker_thread_callback(void* arg_this);
    static void* _publish_version_worker_thread_callback(void* arg_this);
    static void* _clear_transaction_task_worker_thread_callback(void* arg_this);
    static void* _alter_tablet_worker_thread_callback(void* arg_this);
    static void* _clone_worker_thread_callback(void* arg_this);
    static void* _storage_medium_migrate_worker_thread_callback(void* arg_this);
    static void* _check_consistency_worker_thread_callback(void* arg_this);
    static void* _report_task_worker_thread_callback(void* arg_this);
    static void* _report_disk_state_worker_thread_callback(void* arg_this);
    static void* _report_tablet_worker_thread_callback(void* arg_this);
    static void* _upload_worker_thread_callback(void* arg_this);
    static void* _download_worker_thread_callback(void* arg_this);
    static void* _make_snapshot_thread_callback(void* arg_this);
    static void* _release_snapshot_thread_callback(void* arg_this);
    static void* _move_dir_thread_callback(void* arg_this);
    static void* _update_tablet_meta_worker_thread_callback(void* arg_this);

    void _alter_tablet(TaskWorkerPool* worker_pool_this, const TAgentTaskRequest& alter_tablet_request,
                       int64_t signature, const TTaskType::type task_type, TFinishTaskRequest* finish_task_request);

    AgentStatus _get_tablet_info(const TTabletId tablet_id, const TSchemaHash schema_hash, int64_t signature,
                                 TTabletInfo* tablet_info);

    AgentStatus _move_dir(const TTabletId tablet_id, const TSchemaHash schema_hash, const std::string& src,
                          int64_t job_id, bool overwrite, std::vector<std::string>* error_msgs);

    // Reference to the ExecEnv::_master_info
    const TMasterInfo& _master_info;
    TBackend _backend;
    std::unique_ptr<AgentUtils> _agent_utils;
    std::unique_ptr<MasterServerClient> _master_client;
    ExecEnv* _env;

    // Protect task queue
    std::mutex _worker_thread_lock;
    std::condition_variable* _worker_thread_condition_variable;
    std::deque<TAgentTaskRequest> _tasks;

    uint32_t _worker_count = 0;
    TaskWorkerType _task_worker_type;
    CALLBACK_FUNCTION _callback_function;

    static FrontendServiceClientCache _master_service_client_cache;
    static std::atomic_ulong _s_report_version;

    static std::mutex _s_task_signatures_lock;
    static std::map<TTaskType::type, std::set<int64_t>> _s_task_signatures;

    TaskWorkerPool(const TaskWorkerPool&) = delete;
    const TaskWorkerPool& operator=(const TaskWorkerPool&) = delete;
}; // class TaskWorkerPool
} // namespace starrocks
#endif // STARROCKS_BE_SRC_TASK_WORKER_POOL_H
