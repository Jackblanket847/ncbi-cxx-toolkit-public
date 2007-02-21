/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: Network scheduler job status database.
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/version.hpp>
#include <corelib/ncbireg.hpp>
#include <connect/services/netschedule_api.hpp>
#include <connect/ncbi_socket.hpp>

#include <db.h>
#include <bdb/bdb_trans.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_util.hpp>

#include <util/qparse/query_parse.hpp>
#include <util/qparse/query_exec.hpp>
#include <util/qparse/query_exec_bv.hpp>
#include <util/time_line.hpp>

#include "bdb_queue.hpp"

#include "nslb.hpp"

BEGIN_NCBI_SCOPE

const unsigned k_max_dead_locks = 100;  // max. dead lock repeats


/// Mutex to guard vector of busy IDs 
DEFINE_STATIC_FAST_MUTEX(x_NetSchedulerMutex_BusyID);


/// Class guards the id, guarantees exclusive access to the object
///
/// @internal
///
class CIdBusyGuard
{
public:
    CIdBusyGuard(TNSBitVector* id_set, 
                 unsigned int  id,
                 unsigned      timeout)
        : m_IdSet(id_set), m_Id(id)
    {
        _ASSERT(id);
        unsigned cnt = 0; unsigned sleep_ms = 10;
        while (true) {
            {{
            CFastMutexGuard guard(x_NetSchedulerMutex_BusyID);
            if (!(*id_set)[id]) {
                id_set->set(id);
                break;
            }
            }}
            cnt += sleep_ms;
            if (cnt > timeout * 1000) {
                NCBI_THROW(CNetServiceException, 
                           eTimeout, "Failed to lock object");
            }
            SleepMilliSec(sleep_ms);
        } // while
    }

    ~CIdBusyGuard()
    {
        Release();
    }

    void Release()
    {
        if (m_Id) {
            CFastMutexGuard guard(x_NetSchedulerMutex_BusyID);
            m_IdSet->set(m_Id, false);
            m_Id = 0;
        }
    }
private:
    CIdBusyGuard(const CIdBusyGuard&);
    CIdBusyGuard& operator=(const CIdBusyGuard&);
private:
    TNSBitVector* m_IdSet;
    unsigned int  m_Id;
};



/////////////////////////////////////////////////////////////////////////////
// CQueueCollection implementation
CQueueCollection::CQueueCollection(const CQueueDataBase& db) :
    m_QueueDataBase(db)
{}


CQueueCollection::~CQueueCollection()
{
}


void CQueueCollection::Close()
{
    CWriteLockGuard guard(m_Lock);
    NON_CONST_ITERATE(TQueueMap, it, m_QMap) {
        SLockedQueue* q = it->second;
        if (q->lb_coordinator) {
            q->lb_coordinator->StopCollectorThread();
        }
    }
    m_QMap.clear();
}


CRef<SLockedQueue> CQueueCollection::GetLockedQueue(const string& name) const
{
    CReadLockGuard guard(m_Lock);
    TQueueMap::const_iterator it = m_QMap.find(name);
    if (it == m_QMap.end()) {
        string msg = "Job queue not found: ";
        msg += name;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }
    return it->second;
}


bool CQueueCollection::QueueExists(const string& name) const
{
    CReadLockGuard guard(m_Lock);
    TQueueMap::const_iterator it = m_QMap.find(name);
    return (it != m_QMap.end());
}


SLockedQueue& CQueueCollection::AddQueue(const string& name,
                                         SLockedQueue* queue)
{
    CWriteLockGuard guard(m_Lock);
    m_QMap[name] = queue;
    return *queue;
}


bool CQueueCollection::RemoveQueue(const string& name)
{
    CWriteLockGuard guard(m_Lock);
    TQueueMap::iterator it = m_QMap.find(name);
    if (it == m_QMap.end()) return false;
    m_QMap.erase(it);
    return true;
}


CQueueIterator CQueueCollection::begin() const
{
    return CQueueIterator(m_QueueDataBase, m_QMap.begin(), &m_Lock);
}


CQueueIterator CQueueCollection::end() const
{
    return CQueueIterator(m_QueueDataBase, m_QMap.end(), NULL);
}


CQueueIterator::CQueueIterator(const CQueueDataBase& db,
                               CQueueCollection::TQueueMap::const_iterator iter,
                               CRWLock* lock) :
    m_QueueDataBase(db), m_Iter(iter), m_Queue(db), m_Lock(lock)
{
    if (m_Lock) m_Lock->ReadLock();
}


CQueueIterator::CQueueIterator(const CQueueIterator& rhs) :
    m_QueueDataBase(rhs.m_QueueDataBase),
    m_Iter(rhs.m_Iter),
    m_Queue(rhs.m_QueueDataBase),
    m_Lock(rhs.m_Lock)
{
    // Linear on lock
    if (m_Lock) rhs.m_Lock = 0;
}


CQueueIterator::~CQueueIterator()
{
    if (m_Lock) m_Lock->Unlock();
}


CQueue& CQueueIterator::operator*()
{
    m_Queue.x_Assume(m_Iter->second);
    return m_Queue;
}


const string CQueueIterator::GetName()
{
    return m_Iter->first;
}


void CQueueIterator::operator++()
{
    ++m_Iter;
}


/////////////////////////////////////////////////////////////////////////////
// CQueueDataBase implementation
CQueueDataBase::CQueueDataBase()
: m_Env(0),
  m_QueueCollection(*this),
  m_StopPurge(false),
  m_DeleteChkPointCnt(0),
  m_FreeStatusMemCnt(0),
  m_LastFreeMem(time(0)),
  m_LastR2P(time(0)),
  m_UdpPort(0)
{
}


CQueueDataBase::~CQueueDataBase()
{
    try {
        Close();
    } catch (exception& )
    {}
}


void CQueueDataBase::Open(const string& path, 
                          unsigned      cache_ram_size,
                          unsigned      max_locks,
                          unsigned      log_mem_size,
                          unsigned      max_trans)
{
    m_Path = CDirEntry::AddTrailingPathSeparator(path);

    {{
        CDir dir(m_Path);
        if ( !dir.Exists() ) {
            dir.Create();
        }
    }}

    delete m_Env;
    m_Env = new CBDB_Env();


    // memory log option. we probably need to reset LSN
    // numbers
/*
    if (log_mem_size) {
    }

    // Private environment for LSN recovery
    {{
        m_Name = "jsqueue";
        string err_file = m_Path + "err" + string(m_Name) + ".log";
        m_Env->OpenErrFile(err_file.c_str());

        m_Env->SetCacheSize(1024*1024);
        m_Env->OpenPrivate(path.c_str());
        
        m_Env->LsnReset("jsq_test.db");

        delete m_Env;
        m_Env = new CBDB_Env();
    }}
*/



    m_Name = "jsqueue";
    string err_file = m_Path + "err" + string(m_Name) + ".log";
    m_Env->OpenErrFile(err_file.c_str());

    if (log_mem_size) {
        m_Env->SetLogInMemory(true);
        m_Env->SetLogBSize(log_mem_size);
    } else {
        m_Env->SetLogFileMax(200 * 1024 * 1024);
        m_Env->SetLogAutoRemove(true);
    }

    

    // Check if bdb env. files are in place and try to join
    CDir dir(m_Path);
    CDir::TEntries fl = dir.GetEntries("__db.*", CDir::eIgnoreRecursive);

    if (fl.empty()) {
        if (cache_ram_size) {
            m_Env->SetCacheSize(cache_ram_size);
        }
        //unsigned max_locks = m_Env->GetMaxLocks();
        if (max_locks) {
            m_Env->SetMaxLocks(max_locks);
            m_Env->SetMaxLockObjects(max_locks);
        }
        if (max_trans) {
            m_Env->SetTransactionMax(max_trans);
        }

        m_Env->OpenWithTrans(path.c_str(), CBDB_Env::eThreaded);
    } else {
        if (cache_ram_size) {
            m_Env->SetCacheSize(cache_ram_size);
        }
        try {
            m_Env->JoinEnv(path.c_str(), CBDB_Env::eThreaded);
            if (!m_Env->IsTransactional()) {
                LOG_POST(Info << 
                         "JS: '" << 
                         "' Warning: Joined non-transactional environment ");
            }
        } 
        catch (CBDB_ErrnoException& err_ex) 
        {
            if (err_ex.BDB_GetErrno() == DB_RUNRECOVERY) {
                LOG_POST(Warning << 
                         "JS: '" << 
                         "'Warning: DB_ENV returned DB_RUNRECOVERY code."
                         " Running the recovery procedure.");
            }
            m_Env->OpenWithTrans(path.c_str(), 
                                CBDB_Env::eThreaded | CBDB_Env::eRunRecovery);

        }
        catch (CBDB_Exception&)
        {
            m_Env->OpenWithTrans(path.c_str(), 
                                 CBDB_Env::eThreaded | CBDB_Env::eRunRecovery);
        }

    } // if else
    m_Env->SetDirectDB(true);
    m_Env->SetDirectLog(true);

    m_Env->SetLockTimeout(10 * 1000000); // 10 sec

    m_Env->SetTasSpins(5);

    if (m_Env->IsTransactional()) {
        m_Env->SetTransactionTimeout(10 * 1000000); // 10 sec
        m_Env->TransactionCheckpoint();
    }

    m_QueueDescriptionDB.SetEnv(*m_Env);
    m_QueueDescriptionDB.Open("sys_qdescr.db", CBDB_RawFile::eReadWriteCreate);
}


static
CNSLB_ThreasholdCurve* s_ConfigureCurve(const SQueueParameters& params,
                                        unsigned                exec_delay)
{
    auto_ptr<CNSLB_ThreasholdCurve> curve;
    do {

    if (params.lb_curve == SQueueParameters::eLBLinear) {
        curve.reset(new CNSLB_ThreasholdCurve_Linear(params.lb_curve_high,
                                                     params.lb_curve_linear_low));
        break;
    }

    if (params.lb_curve == SQueueParameters::eLBRegression) {
        curve.reset(new CNSLB_ThreasholdCurve_Regression(params.lb_curve_high,
                                                params.lb_curve_regression_a));
        break;
    }

    } while(0);

    if (curve.get()) {
        curve->ReGenerateCurve(exec_delay);
    }
    return curve.release();
}

/*
static
CNSLB_DecisionModule* s_ConfigureDecision(const IRegistry& reg, 
                                          const string&    sname)
{
    auto_ptr<CNSLB_DecisionModule> decision;
    string lb_policy = 
        reg.GetString(sname, "lb_policy", kEmptyStr);

    do {
    if (lb_policy.empty() ||
        NStr::CompareNocase(lb_policy, "rate")==0) {
        decision.reset(new CNSLB_DecisionModule_DistributeRate());
        break;
    }
    if (NStr::CompareNocase(lb_policy, "cpu_avail")==0) {
        decision.reset(new CNSLB_DecisionModule_CPU_Avail());
        break;
    }

    } while(0);

    return decision.release();
}
*/

void CQueueDataBase::Configure(const IRegistry& reg, unsigned* min_run_timeout)
{
    bool no_default_queues = 
        reg.GetBool("server", "no_default_queues", false, 0, IRegistry::eReturn);

    CFastMutexGuard guard(m_ConfigureLock);

    x_CleanParamMap();

    CBDB_Transaction trans(*m_Env, 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);
    m_QueueDescriptionDB.SetTransaction(&trans);

    list<string> sections;
    reg.EnumerateSections(&sections);

    ITERATE(list<string>, it, sections) {
        string qclass, tmp;
        const string& sname = *it;
        NStr::SplitInTwo(sname, "_", tmp, qclass);
        if (NStr::CompareNocase(tmp, "queue") != 0 &&
            NStr::CompareNocase(tmp, "qclass") != 0) {
            continue;
        }
        if (m_QueueParamMap.find(qclass) != m_QueueParamMap.end()) {
            LOG_POST(Warning << tmp << " section " << sname
                             << " conflicts with previous " << 
                             (NStr::CompareNocase(tmp, "queue") == 0 ?
                                 "qclass" : "queue") <<
                             " section with same queue/qclass name");
            continue;
        }

        SQueueParameters* params = new SQueueParameters;
        params->Read(reg, sname);
        m_QueueParamMap[qclass] = params;
        *min_run_timeout = 
            std::min(*min_run_timeout, (unsigned)params->run_timeout_precision);
        if (!no_default_queues && NStr::CompareNocase(tmp, "queue") == 0) {
            m_QueueDescriptionDB.queue = qclass;
            m_QueueDescriptionDB.kind = SLockedQueue::eKindStatic;
            m_QueueDescriptionDB.qclass = qclass;
            m_QueueDescriptionDB.UpdateInsert();
        }
    }

    list<string> queues;
    reg.EnumerateEntries("queues", &queues);
    ITERATE(list<string>, it, queues) {
        const string& qname = *it;
        string qclass = reg.GetString("queues", qname, "");
        if (!qclass.empty()) {
            m_QueueDescriptionDB.queue = qname;
            m_QueueDescriptionDB.kind = SLockedQueue::eKindStatic;
            m_QueueDescriptionDB.qclass = qclass;
            m_QueueDescriptionDB.UpdateInsert();
        }
    }
    trans.Commit();
    m_QueueDescriptionDB.Sync();

    CBDB_FileCursor cur(m_QueueDescriptionDB);
    cur.SetCondition(CBDB_FileCursor::eFirst);

    while (cur.Fetch() == eBDB_Ok) {
        string qname(m_QueueDescriptionDB.queue);
        string qclass(m_QueueDescriptionDB.qclass);
        int kind = m_QueueDescriptionDB.kind;
        TQueueParamMap::iterator it = m_QueueParamMap.find(qclass);
        if (it == m_QueueParamMap.end()) {
            LOG_POST(Error << "Can not find class " << qclass << " for queue " << qname);
            continue;
        }
        const SQueueParameters& params = *(it->second);
        bool qexists = m_QueueCollection.QueueExists(qname);
        if (!qexists) {
            LOG_POST(Info 
                << "Mounting queue:           " << qname                        << "\n"
                << "   Timeout:               " << params.timeout               << "\n"
                << "   Notification timeout:  " << params.notif_timeout         << "\n"
                << "   Run timeout:           " << params.run_timeout           << "\n"
                << "   Run timeout precision: " << params.run_timeout_precision << "\n"
                << "   Programs:              " << params.program_name          << "\n"
                << "   Delete when done:      " << params.delete_when_done      << "\n"
            );
            MountQueue(qname, qclass, kind, params);
        } else {
            // update non-critical queue parameters
            UpdateQueueParameters(qname, params);
        }

        // re-load load balancing settings
        UpdateQueueLBParameters(qname, params);
    }
}


void CQueueDataBase::MountQueue(const string& qname,
                                const string& qclass,
                                TQueueKind    kind,
                                const SQueueParameters& params)
{
    _ASSERT(m_Env);

    auto_ptr<SLockedQueue> q(new SLockedQueue(qname, qclass, kind));
    q->Open(*m_Env, m_Path);

    q->timeout = params.timeout;
    q->notif_timeout = params.notif_timeout;
    q->delete_done = params.delete_when_done;
    q->last_notif = time(0);

    SLockedQueue& queue = m_QueueCollection.AddQueue(qname, q.release());

    queue.run_timeout = params.run_timeout;
    if (params.run_timeout) {
        queue.run_time_line =
            new CJobTimeLine(params.run_timeout_precision, 0);
    }

    // scan the queue, load the state machine from DB

    CBDB_FileCursor cur(queue.db);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    unsigned recs = 0;

    for (;cur.Fetch() == eBDB_Ok; ++recs) {
        unsigned job_id = queue.db.id;
        int status = queue.db.status;


        if (job_id > (unsigned)queue.m_LastId.Get()) {
            queue.m_LastId.Set(job_id);
        }
        queue.status_tracker.SetExactStatusNoLock(job_id, 
                      (CNetScheduleAPI::EJobStatus) status, 
                      true);

        if (status == (int) CNetScheduleAPI::eRunning && 
            queue.run_time_line) {
            // Add object to the first available slot
            // it is going to be rescheduled or dropped
            // in the background control thread
            queue.run_time_line->AddObjectToSlot(0, job_id);
        }
    }

    queue.udp_socket.SetReuseAddress(eOn);
    unsigned short udp_port = GetUdpPort();
    if (udp_port) {
        queue.udp_socket.Bind(udp_port);
    }

    // program version control
    if (!params.program_name.empty()) {
        queue.program_version_list.AddClientInfo(params.program_name);
    }

    queue.subm_hosts.SetHosts(params.subm_hosts);
    queue.failed_retries = params.failed_retries;
    queue.empty_lifetime = params.empty_lifetime;
    queue.wnode_hosts.SetHosts(params.wnode_hosts);
    queue.rec_dump_flag = params.dump_db;


    LOG_POST(Info << "Queue records = " << recs);
}


void CQueueDataBase::CreateQueue(const string& qname, const string& qclass,
                                 const string& comment)
{
    CFastMutexGuard guard(m_ConfigureLock);
    if (m_QueueCollection.QueueExists(qname)) {
        string err = string("Queue \"") + qname + "\" already exists";
        NCBI_THROW(CNetScheduleException, eDuplicateName, err);
    }
    TQueueParamMap::iterator it = m_QueueParamMap.find(qclass);
    if (it == m_QueueParamMap.end()) {
        string err = string("Can not find class \"") + qclass +
                            "\" for queue \"" + qname + "\"";
        NCBI_THROW(CNetScheduleException, eUnknownQueueClass, err);
    }
    CBDB_Transaction trans(*m_Env, 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);
    m_QueueDescriptionDB.SetTransaction(&trans);
    m_QueueDescriptionDB.queue = qname;
    m_QueueDescriptionDB.kind = SLockedQueue::eKindDynamic;
    m_QueueDescriptionDB.qclass = qclass;
    m_QueueDescriptionDB.comment = comment;
    m_QueueDescriptionDB.UpdateInsert();
    trans.Commit();
    m_QueueDescriptionDB.Sync();
    const SQueueParameters& params = *(it->second);
    MountQueue(qname, qclass, SLockedQueue::eKindDynamic, params);
    UpdateQueueLBParameters(qname, params);
}


void CQueueDataBase::DeleteQueue(const string& qname)
{
    CFastMutexGuard guard(m_ConfigureLock);
    CBDB_Transaction trans(*m_Env, 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);
    m_QueueDescriptionDB.SetTransaction(&trans);
    m_QueueDescriptionDB.queue = qname;
    if (m_QueueDescriptionDB.Fetch() != eBDB_Ok) {
        string msg = "Job queue not found: ";
        msg += qname;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }
    int kind = m_QueueDescriptionDB.kind;
    if (!kind) {
        string msg = "Static queue \"";
        msg += qname;
        msg += "\" can not be deleted"; 
        NCBI_THROW(CNetScheduleException, eAccessDenied, msg);
    }
    // Signal queue to wipe out database files.
    CRef<SLockedQueue> queue(m_QueueCollection.GetLockedQueue(qname));
    queue->delete_database = true;
    // Remove it from collection
    if (!m_QueueCollection.RemoveQueue(qname)) {
        string msg = "Job queue not found: ";
        msg += qname;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }
    // Remove it from DB
    m_QueueDescriptionDB.Delete(CBDB_File::eIgnoreError);
    trans.Commit();
}


void CQueueDataBase::QueueInfo(const string& qname, int& kind,
                               string* qclass, string* comment)
{
    CFastMutexGuard guard(m_ConfigureLock);
    m_QueueDescriptionDB.SetTransaction(0);
    m_QueueDescriptionDB.queue = qname;
    if (m_QueueDescriptionDB.Fetch() != eBDB_Ok) {
        string msg = "Job queue not found: ";
        msg += qname;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }
    kind = m_QueueDescriptionDB.kind;
    string qclass_str(m_QueueDescriptionDB.qclass);
    *qclass = qclass_str;
    string comment_str(m_QueueDescriptionDB.comment);
    *comment = comment_str;
}


void CQueueDataBase::GetQueueNames(string *list, const string& sep) const
{
    CFastMutexGuard guard(m_ConfigureLock);
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        *list += it.GetName(); *list += sep;
    }
}

void CQueueDataBase::UpdateQueueParameters(const string& qname,
                                           const SQueueParameters& params)
{
    CRef<SLockedQueue> queue(m_QueueCollection.GetLockedQueue(qname));

    queue->program_version_list.Clear();
    if (!params.program_name.empty()) {
        queue->program_version_list.AddClientInfo(params.program_name);
    }
    queue->subm_hosts.SetHosts(params.subm_hosts);
    queue->failed_retries = params.failed_retries;
    queue->wnode_hosts.SetHosts(params.wnode_hosts);
    {{
        CFastMutexGuard guard(queue->rec_dump_lock);
        queue->rec_dump_flag = params.dump_db;
    }}
}

void CQueueDataBase::UpdateQueueLBParameters(const string& qname,
                                             const SQueueParameters& params)
{
    CRef<SLockedQueue> queue(m_QueueCollection.GetLockedQueue(qname));

    if (params.lb_flag == queue->lb_flag) return;

    if (params.lb_service.empty()) {
        if (params.lb_flag) {
            LOG_POST(Error << "Queue:" << qname 
                << "cannot be load balanced. Missing lb_service ini setting");
        }
        queue->lb_flag = false;
    } else {
        ENSLB_RunDelayType lb_delay_type = eNSLB_Constant;
        unsigned lb_stall_time = 6;
        if (NStr::CompareNocase(params.lb_exec_delay_str, "run_time")) {
            lb_delay_type = eNSLB_RunTimeAvg;
        } else {
            try {
                int stall_time =
                    NStr::StringToInt(params.lb_exec_delay_str);
                if (stall_time > 0) {
                    lb_stall_time = stall_time;
                }
            } 
            catch(exception& ex)
            {
                ERR_POST("Invalid value of lb_exec_delay " 
                        << ex.what()
                        << " Offending value:"
                        << params.lb_exec_delay_str
                        );
            }
        }

        CNSLB_Coordinator::ENonLbHostsPolicy non_lb_hosts = 
            CNSLB_Coordinator::eNonLB_Allow;
        if (NStr::CompareNocase(params.lb_unknown_host, "deny") == 0) {
            non_lb_hosts = CNSLB_Coordinator::eNonLB_Deny;
        } else
        if (NStr::CompareNocase(params.lb_unknown_host, "allow") == 0) {
            non_lb_hosts = CNSLB_Coordinator::eNonLB_Allow;
        } else
        if (NStr::CompareNocase(params.lb_unknown_host, "reserve") == 0) {
            non_lb_hosts = CNSLB_Coordinator::eNonLB_Reserve;
        }

        if (queue->lb_flag == false) { // LB is OFF
            LOG_POST(Error << "Queue:" << qname 
                        << " is load balanced. " << params.lb_service);

            if (queue->lb_coordinator == 0) {
                auto_ptr<CNSLB_ThreasholdCurve> 
                deny_curve(s_ConfigureCurve(params, lb_stall_time));

                //CNSLB_DecisionModule*   decision_maker = 0;

                auto_ptr<CNSLB_DecisionModule_DistributeRate> 
                    decision_distr_rate(
                        new CNSLB_DecisionModule_DistributeRate);

                auto_ptr<INSLB_Collector>   
                    collect(new CNSLB_LBSMD_Collector());

                auto_ptr<CNSLB_Coordinator> 
                coord(new CNSLB_Coordinator(
                                params.lb_service,
                                collect.release(),
                                deny_curve.release(),
                                decision_distr_rate.release(),
                                params.lb_collect_time,
                                non_lb_hosts));

                queue->lb_coordinator = coord.release();
                queue->lb_stall_delay_type = lb_delay_type;
                queue->lb_stall_time = lb_stall_time;
                queue->lb_stall_time_mult = params.lb_stall_time_mult;
                queue->lb_flag = true;

            } else {  // LB is ON
                // reconfigure the LB delay
                CFastMutexGuard guard(queue->lb_stall_time_lock);
                queue->lb_stall_delay_type = lb_delay_type;
                if (lb_delay_type == eNSLB_Constant) {
                    queue->lb_stall_time = lb_stall_time;
                }
                queue->lb_stall_time_mult = params.lb_stall_time_mult;
            }
        }
    }
}


void CQueueDataBase::Close()
{
    StopNotifThread();
    StopPurgeThread();
    StopExecutionWatcherThread();

    if (m_Env) {
        m_Env->TransactionCheckpoint();
    }

    x_CleanParamMap();

    m_QueueCollection.Close();
    m_QueueDescriptionDB.Close();
    try {
        if (m_Env) {
            if (m_Env->CheckRemove()) {
                LOG_POST(Info    <<
                         "JS: '" <<
                         m_Name  << "' Unmounted. BDB ENV deleted.");
            } else {
                LOG_POST(Warning << "JS: '" << m_Name 
                                 << "' environment still in use.");
            }
        }
    }
    catch (exception& ex) {
        LOG_POST(Warning << "JS: '" << m_Name 
                         << "' Exception in Close() " << ex.what()
                         << " (ignored.)");
    }

    delete m_Env; m_Env = 0;
}


void CQueueDataBase::TransactionCheckPoint()
{
    m_Env->TransactionCheckpoint();
    m_Env->CleanLog();
}


void CQueueDataBase::NotifyListeners(void)
{
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        (*it).NotifyListeners();
    }
}


void CQueueDataBase::CheckExecutionTimeout(void)
{
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        (*it).CheckExecutionTimeout();
    }    
}


void CQueueDataBase::Purge(void)
{
    x_Returned2Pending();

    unsigned global_del_rec = 0;

    // check not to rescan the database too often 
    // when we have low client activity of inserting new jobs.
    /*
    Need to be replaced with load-based logic.
    The logic is flawed anyway - the case when last purged (that is,
    long expired) job is closer than 3000 to new id is VERY rare if
    server is being used at all. 3000 new jobs with lifetime of 10
    days switch off this check for all these 10 days.
    if (m_PurgeLastId + 3000 > (unsigned) m_IdCounter.Get()) {
        ++m_PurgeSkipCnt;

        // probably nothing to do yet, skip purge execution
        if (m_PurgeSkipCnt < 10) { // only 10 skips in a row
            if (unc_del_rec < n_jobs_to_delete) {
                // If there is no unconditional delete jobs - sleep
                SleepMilliSec(1000);
            }
            return;
        }

        m_PurgeSkipCnt = 0;        
    }
    */

    // Delete obsolete job records, based on time stamps 
    // and expiration timeouts
    unsigned n_queue_limit = 2000; // TODO: determine this based on load

    CNetScheduleAPI::EJobStatus statuses_to_delete_from[] = {
        CNetScheduleAPI::eFailed, CNetScheduleAPI::eCanceled,
        CNetScheduleAPI::eDone, CNetScheduleAPI::ePending
    };
    const int kStatusesSize = sizeof(statuses_to_delete_from) /
                              sizeof(CNetScheduleAPI::EJobStatus);

    vector<string> queues_to_delete;
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        unsigned queue_del_rec = 0;
        const unsigned batch_size = 100;
        if ((*it).IsExpired()) {
            queues_to_delete.push_back(it.GetName());
            continue;
        }
        for (bool stop_flag = false; !stop_flag; ) {
            // stop if all statuses have less than batch_size jobs to delete
            stop_flag = true;
            for (int n = 0; n < kStatusesSize; ++n) {
                CNetScheduleAPI::EJobStatus s = statuses_to_delete_from[n];
                unsigned del_rec = (*it).CheckDeleteBatch(batch_size, s);
                stop_flag = stop_flag && (del_rec < batch_size);
                queue_del_rec += del_rec;
            }

            // do not delete more than certain number of
            // records from the queue in one Purge
            if (queue_del_rec >= n_queue_limit)
                break;

            if (x_CheckStopPurge()) return;
        }
        global_del_rec += queue_del_rec;
    }

    // Delete jobs unconditionally, from internal vector.
    // This is done to spread massive job deletion in time
    // and thus smooth out peak loads
    // TODO: determine batch size based on load
    unsigned n_jobs_to_delete = 10000; 
    unsigned unc_del_rec = x_PurgeUnconditional(n_jobs_to_delete);
    global_del_rec += unc_del_rec;

    m_DeleteChkPointCnt += global_del_rec;
    if (m_DeleteChkPointCnt > 1000) {
        m_DeleteChkPointCnt = 0;
        x_OptimizeAffinity();
        m_Env->TransactionCheckpoint();
    }

    m_FreeStatusMemCnt += global_del_rec;
    x_OptimizeStatusMatrix();

    ITERATE(vector<string>, it, queues_to_delete) {
        try {
            DeleteQueue((*it));
        } catch (...) { // TODO: use more specific exception
            LOG_POST(Warning << "Queue " << (*it) << " already gone.");
        }
    }

    if (global_del_rec > n_jobs_to_delete + n_queue_limit * m_QueueCollection.GetSize()) {
        SleepMilliSec(1000);
    }
}

unsigned CQueueDataBase::x_PurgeUnconditional(unsigned batch_size) {
    // Purge unconditional jobs
    unsigned del_rec = 0;
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        del_rec += (*it).DoDeleteBatch(batch_size);
    }
    return del_rec;
}


void CQueueDataBase::x_Returned2Pending(void)
{
    const int kR2P_delay = 7; // re-submission delay
    time_t curr = time(0);

    if (m_LastR2P + kR2P_delay <= curr) {
        m_LastR2P = curr;
        ITERATE(CQueueCollection, it, m_QueueCollection) {
            (*it).Returned2Pending();
        }
    }
}


void CQueueDataBase::x_OptimizeAffinity(void)
{
    // remove unused affinity elements
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        (*it).ClearAffinityIdx();
    }
}


void CQueueDataBase::x_OptimizeStatusMatrix(void)
{
    time_t curr = time(0);
    // optimize memory every 15 min. or after 1 million of deleted records
    const int kMemFree_Delay = 15 * 60; 
    const unsigned kRecordThreshold = 1000000;
    if ((m_FreeStatusMemCnt > kRecordThreshold) ||
        (m_LastFreeMem + kMemFree_Delay <= curr)) {
        m_FreeStatusMemCnt = 0;
        m_LastFreeMem = curr;

        ITERATE(CQueueCollection, it, m_QueueCollection) {
            (*it).FreeUnusedMem();
            if (x_CheckStopPurge()) return;
        } // ITERATE
    }
}


void CQueueDataBase::x_CleanParamMap(void)
{
    NON_CONST_ITERATE(TQueueParamMap, it, m_QueueParamMap) {
        SQueueParameters* params = it->second;
        delete params;
    }
    m_QueueParamMap.clear();
}


void CQueueDataBase::StopPurge(void)
{
    CFastMutexGuard guard(m_PurgeLock);
    m_StopPurge = true;
}


bool CQueueDataBase::x_CheckStopPurge(void)
{
    CFastMutexGuard guard(m_PurgeLock);
    bool stop_purge = m_StopPurge;
    m_StopPurge = false;
    return stop_purge;
}


void CQueueDataBase::RunPurgeThread(void)
{
    LOG_POST(Info << "Starting guard and cleaning thread...");
    m_PurgeThread.Reset(
        new CJobQueueCleanerThread(*this, 1));
    m_PurgeThread->Run();
    LOG_POST(Info << "Started.");
}


void CQueueDataBase::StopPurgeThread(void)
{
    if (!m_PurgeThread.Empty()) {
        LOG_POST(Info << "Stopping guard and cleaning thread...");
        StopPurge();
        m_PurgeThread->RequestStop();
        m_PurgeThread->Join();
        LOG_POST(Info << "Stopped.");
    }
}

void CQueueDataBase::RunNotifThread(void)
{
    if (GetUdpPort() == 0) {
        return;
    }

    LOG_POST(Info << "Starting client notification thread...");
    m_NotifThread.Reset(
        new CJobNotificationThread(*this, 1));
    m_NotifThread->Run();
    LOG_POST(Info << "Started.");
}

void CQueueDataBase::StopNotifThread(void)
{
    if (!m_NotifThread.Empty()) {
        LOG_POST(Info << "Stopping notification thread...");
        m_NotifThread->RequestStop();
        m_NotifThread->Join();
        LOG_POST(Info << "Stopped.");
    }
}

void CQueueDataBase::RunExecutionWatcherThread(unsigned run_delay)
{
    LOG_POST(Info << "Starting execution watcher thread...");
    m_ExeWatchThread.Reset(
        new CJobQueueExecutionWatcherThread(*this, run_delay));
    m_ExeWatchThread->Run();
    LOG_POST(Info << "Started.");
}

void CQueueDataBase::StopExecutionWatcherThread(void)
{
    if (!m_ExeWatchThread.Empty()) {
        LOG_POST(Info << "Stopping execution watch thread...");
        m_ExeWatchThread->RequestStop();
        m_ExeWatchThread->Join();
        LOG_POST(Info << "Stopped.");
    }
}


/////////////////////////////////////////////////////////////////////////////
// CQueue implementation

#define DB_TRY(where) for (unsigned n_tries = 0; true; ) { try {
#define DB_END \
    } catch (CBDB_ErrnoException& ex) { \
        if (ex.IsDeadLock()) { \
            if (++n_tries < k_max_dead_locks) { \
                if (m_LQueue->monitor.IsMonitorActive()) { \
                    m_LQueue->monitor.SendString( \
                        "DeadLock repeat in "##where); \
                } \
                SleepMilliSec(250); \
                continue; \
            } \
        } else if (ex.IsNoMem()) { \
            if (++n_tries < k_max_dead_locks) { \
                if (m_LQueue->monitor.IsMonitorActive()) { \
                    m_LQueue->monitor.SendString( \
                        "No resource repeat in "##where); \
                } \
                SleepMilliSec(250); \
                continue; \
            } \
        } \
        ERR_POST("Too many transaction repeats in "##where); \
        throw; \
    } \
    break; \
}


CQueue::CQueue(CQueueDataBase& db, 
               const string&   queue_name,
               unsigned        client_host_addr)
: m_Db(db),
  m_LQueue(db.m_QueueCollection.GetLockedQueue(queue_name)),
  m_ClientHostAddr(client_host_addr),
  m_QueueDbAccessCounter(0)
{
}


unsigned CQueue::CountRecs()
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CFastMutexGuard guard(q->lock);
    q->db.SetTransaction(0);
    return q->db.CountRecs();
}

bool CQueue::IsExpired(void)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CFastMutexGuard guard(q->lock);
    if (q->kind && q->empty_lifetime != -1) {
        unsigned cnt = q->status_tracker.Count();
        if (cnt) {
            q->became_empty = -1;
        } else {
            if (q->became_empty != -1 &&
                q->became_empty + q->empty_lifetime < time(0))
            {
                return true;
            }
            if (q->became_empty == -1)
                q->became_empty = time(0);
        }
    }
    return false;
}


#define NS_PRINT_TIME(msg, t) \
    do \
    { unsigned tt = t; \
      CTime _t(tt); _t.ToLocalTime(); \
      out << msg << (tt ? _t.AsString() : kEmptyStr) << fsp; \
    } while(0)

#define NS_PFNAME(x_fname) \
    (fflag ? (const char*) x_fname : (const char*) "")

void CQueue::x_PrintJobDbStat(SQueueDB&      db, 
                              CNcbiOstream&  out,
                              const char*    fsp,
                              bool           fflag)
{
    out << fsp << NS_PFNAME("id: ") << (unsigned) db.id << fsp;
    CNetScheduleAPI::EJobStatus status = 
        (CNetScheduleAPI::EJobStatus)(int)db.status;
    out << NS_PFNAME("status: ") << CNetScheduleAPI::StatusToString(status) 
        << fsp;

    NS_PRINT_TIME(NS_PFNAME("time_submit: "), db.time_submit);
    NS_PRINT_TIME(NS_PFNAME("time_run: "), db.time_run);
    NS_PRINT_TIME(NS_PFNAME("time_done: "), db.time_done);

    out << NS_PFNAME("timeout: ") << (unsigned)db.timeout << fsp;
    out << NS_PFNAME("run_timeout: ") << (unsigned)db.run_timeout << fsp;

    time_t exp_time = 
        x_ComputeExpirationTime((unsigned)db.time_run, 
                                (unsigned)db.run_timeout);
    NS_PRINT_TIME(NS_PFNAME("time_run_expire: "), exp_time);


    unsigned subm_addr = db.subm_addr;
    out << NS_PFNAME("subm_addr: ") 
        << (subm_addr ? CSocketAPI::gethostbyaddr(subm_addr) : kEmptyStr) << fsp;
    out << NS_PFNAME("subm_port: ") << (unsigned) db.subm_port << fsp;
    out << NS_PFNAME("subm_timeout: ") << (unsigned) db.subm_timeout << fsp;

    unsigned addr = db.worker_node1;
    out << NS_PFNAME("worker_node1: ")
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    addr = db.worker_node2;
    out << NS_PFNAME("worker_node2: ") 
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    addr = db.worker_node3;
    out << NS_PFNAME("worker_node3: ") 
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    addr = db.worker_node4;
    out << NS_PFNAME("worker_node4: ") 
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    addr = db.worker_node5;
    out << NS_PFNAME("worker_node5: ") 
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    out << NS_PFNAME("run_counter: ") << (unsigned) db.run_counter << fsp;
    out << NS_PFNAME("ret_code: ") << (unsigned) db.ret_code << fsp;

    unsigned aff_id = (unsigned) db.aff_id;
    if (aff_id) {
        CRef<SLockedQueue> q(x_GetLQueue());
        string token = q->affinity_dict.GetAffToken(aff_id);
        out << NS_PFNAME("aff_token: ") << "'" << token << "'" << fsp;
    }
    out << NS_PFNAME("aff_id: ") << aff_id << fsp;
    out << NS_PFNAME("mask: ") << (unsigned) db.mask << fsp;

    out << NS_PFNAME("input: ")        << "'" <<(string) db.input       << "'" << fsp;
    out << NS_PFNAME("output: ")       << "'" <<(string) db.output       << "'" << fsp;
    out << NS_PFNAME("err_msg: ")      << "'" <<(string) db.err_msg      << "'" << fsp;
    out << NS_PFNAME("progress_msg: ") << "'" <<(string) db.progress_msg << "'" << fsp;
    out << "\n";
}

void 
CQueue::x_PrintShortJobDbStat(SQueueDB&     db, 
                              const string& host,
                              unsigned      port,
                              CNcbiOstream& out,
                              const char*   fsp)
{
    char buf[1024];
    sprintf(buf, NETSCHEDULE_JOBMASK, 
                 (unsigned)db.id, host.c_str(), port);
    out << buf << fsp;
    CNetScheduleAPI::EJobStatus status = 
        (CNetScheduleAPI::EJobStatus)(int)db.status;
    out << CNetScheduleAPI::StatusToString(status) << fsp;

    out << "'" << (string) db.input    << "'" << fsp;
    out << "'" << (string) db.output   << "'" << fsp;
    out << "'" << (string) db.err_msg  << "'" << fsp;

    out << "\n";
}


void 
CQueue::PrintJobDbStat(unsigned                    job_id, 
                       CNcbiOstream&               out,
                       CNetScheduleAPI::EJobStatus status)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;
    if (status == CNetScheduleAPI::eJobNotFound) {
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(0);
        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {
            x_PrintJobDbStat(db, out);
        } else {
            out << "Job not found id=" << job_id;
        }
        out << "\n";
    } else {
        TNSBitVector bv;
        q->status_tracker.StatusSnapshot(status, &bv);

        TNSBitVector::enumerator en(bv.first());
        for (;en.valid(); ++en) {
            unsigned id = *en;
            {{
                CFastMutexGuard guard(q->lock);
                db.SetTransaction(0);
                
                db.id = id;

                if (db.Fetch() == eBDB_Ok) {
                    x_PrintJobDbStat(db, out);
                }
            }}
        } // for
        out << "\n";
    }
}


void CQueue::PrintJobStatusMatrix(CNcbiOstream& out)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->status_tracker.PrintStatusMatrix(out);
}


bool CQueue::IsVersionControl() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->program_version_list.IsConfigured();
}


bool CQueue::IsMatchingClient(const CQueueClientInfo& cinfo) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->program_version_list.IsMatchingClient(cinfo);
}


bool CQueue::IsSubmitAllowed() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return (m_ClientHostAddr == 0) || 
            q->subm_hosts.IsAllowed(m_ClientHostAddr);
}


bool CQueue::IsWorkerAllowed() const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return (m_ClientHostAddr == 0) || 
            q->wnode_hosts.IsAllowed(m_ClientHostAddr);
}


double CQueue::GetAverage(TStatEvent n_event)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->GetAverage(n_event);
}


void CQueue::PrintAllJobDbStat(CNcbiOstream& out)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;
    CFastMutexGuard guard(q->lock);

    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *q->GetCursor(trans);
    CBDB_CursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    while (cur.Fetch() == eBDB_Ok) {
        x_PrintJobDbStat(db, out);
        if (!out.good()) break;
    }
}


void CQueue::PrintStat(CNcbiOstream& out)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;
    CFastMutexGuard guard(q->lock);
    db.SetTransaction(0);
    db.PrintStat(out);
}


void CQueue::PrintQueue(CNcbiOstream&               out, 
                        CNetScheduleAPI::EJobStatus job_status,
                        const string&               host,
                        unsigned                    port)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    TNSBitVector bv;
    q->status_tracker.StatusSnapshot(job_status, &bv);
    SQueueDB& db = q->db;

    TNSBitVector::enumerator en(bv.first());
    for (;en.valid(); ++en) {
        unsigned id = *en;
        {{
            CFastMutexGuard guard(q->lock);
            db.SetTransaction(0);
            
            db.id = id;

            if (db.Fetch() == eBDB_Ok) {
                x_PrintShortJobDbStat(db, host, port, out, "\t");
            }
        }}
    }
}


// Functor for EQ
class CQueryFunctionEQ : public CQueryFunction_BV_Base<TNSBitVector>
{
public:
    CQueryFunctionEQ(CRef<SLockedQueue> queue) :
      m_Queue(queue)
    {}
    typedef CQueryFunction_BV_Base<TNSBitVector> TParent;
    typedef TParent::TBVContainer::TBuffer TBuffer;
    typedef TParent::TBVContainer::TBitVector TBitVector;
    virtual void Evaluate(CQueryParseTree::TNode& qnode);
private:
    void x_CheckArgs(const CQueryFunctionBase::TArgVector& args);
    CRef<SLockedQueue> m_Queue;
};

void CQueryFunctionEQ::Evaluate(CQueryParseTree::TNode& qnode)
{
    //NcbiCout << "Key: " << key << " Value: " << val << NcbiEndl;
    CQueryFunctionBase::TArgVector args;
    this->MakeArgVector(qnode, args);
    x_CheckArgs(args);
    const string& key = args[0]->GetValue().GetStrValue();
    const string& val = args[1]->GetValue().GetStrValue();
    auto_ptr<TNSBitVector> bv;
    auto_ptr<TBuffer> buf;
    if (key == "status") {
        // special case for status
        CNetScheduleAPI::EJobStatus status =
            CNetScheduleAPI::StringToStatus(val);
        if (status == CNetScheduleAPI::eJobNotFound)
            NCBI_THROW(CNetScheduleException,
                eQuerySyntaxError, string("Unknown status: ") + val);
        bv.reset(new TNSBitVector);
        m_Queue->status_tracker.StatusSnapshot(status, bv.get());
    } else {
        if (val == "*") {
            // wildcard
            bv.reset(new TNSBitVector);
            m_Queue->ReadTags(key, bv.get());
        } else {
            buf.reset(new TBuffer);
            m_Queue->ReadTag(key, val, buf.get());
        }
    }
    if (qnode.GetValue().IsNot()) {
        // Apply NOT here
        if (bv.get()) {
            bv.get()->invert();
        } else if (buf.get()) {
            bv.reset(new TNSBitVector());
            bm::operation_deserializer<TNSBitVector>::deserialize(*bv,
                                                &((*buf.get())[0]),
                                                0,
                                                bm::set_ASSIGN);
            bv.get()->invert();
        }
    }
    if (bv.get())
        this->MakeContainer(qnode)->SetBV(bv.release());
    else if (buf.get())
        this->MakeContainer(qnode)->SetBuffer(buf.release());
}

void CQueryFunctionEQ::x_CheckArgs(const CQueryFunctionBase::TArgVector& args)
{
    if (args.size() != 2 ||
        (args[0]->GetValue().GetType() != CQueryParseNode::eIdentifier  &&
         args[0]->GetValue().GetType() != CQueryParseNode::eString)) {
        NCBI_THROW(CNetScheduleException,
             eQuerySyntaxError, "Wrong arguments for '='");
    }
}


string CQueue::ExecQuery(const string& query, const string& action,
                         const string& fields)
{
    //NcbiCout << "Query: \"" << query <<
    //            "\" Action: \"" << action <<
    //            "\" Fields: \"" << fields << "\"" << NcbiEndl;

    CRef<SLockedQueue> q(x_GetLQueue());
    TNSBitVector alive_jobs;
    q->status_tracker.GetAliveJobs(alive_jobs);
    CQueryParseTree qtree;
    try {
        qtree.Parse(query.c_str());
    } catch (CQueryParseException& ex) {
        NCBI_THROW(CNetScheduleException, eQuerySyntaxError, ex.GetMsg());
    }
    CQueryParseTree::TNode* top = qtree.GetQueryTree();
    if (!top)
        NCBI_THROW(CNetScheduleException,
            eQuerySyntaxError, "Query syntax error");

    CQueryExec qexec;
    qexec.AddFunc(CQueryParseNode::eAnd,
        new CQueryFunction_BV_Logic<TNSBitVector>(bm::set_AND));
    qexec.AddFunc(CQueryParseNode::eOr,
        new CQueryFunction_BV_Logic<TNSBitVector>(bm::set_OR));
    qexec.AddFunc(CQueryParseNode::eSub,
        new CQueryFunction_BV_Logic<TNSBitVector>(bm::set_SUB));
    qexec.AddFunc(CQueryParseNode::eXor,
        new CQueryFunction_BV_Logic<TNSBitVector>(bm::set_XOR));
    qexec.AddFunc(CQueryParseNode::eIn,
        new CQueryFunction_BV_In_Or<TNSBitVector>());
    qexec.AddFunc(CQueryParseNode::eNot,
        new CQueryFunction_BV_Not<TNSBitVector>());

    qexec.AddFunc(CQueryParseNode::eEQ,
        new CQueryFunctionEQ(q));

    {{
        CReadLockGuard(q->GetTagLock());
        qexec.Evaluate(qtree);
    }}

    IQueryParseUserObject* uo = top->GetValue().GetUserObject();
    if (!uo)
        NCBI_THROW(CNetScheduleException,
            eQuerySyntaxError, "Query syntax error");
    typedef CQueryEval_BV_Value<TNSBitVector> BV_UserObject;
    BV_UserObject* result =
        dynamic_cast<BV_UserObject*>(uo);
    _ASSERT(result);
    auto_ptr<TNSBitVector> bv(result->ReleaseBV());
    if (!bv.get()) {
        bv.reset(new TNSBitVector());
        BV_UserObject::TBuffer *buf = result->ReleaseBuffer();
        if (buf && buf->size()) {
            bm::operation_deserializer<TNSBitVector>::deserialize(*bv,
                                                &((*buf)[0]),
                                                0,
                                                bm::set_ASSIGN);
        }
    }
    // Filter against status matrix
    *(bv.get()) &= alive_jobs;
    if (action == "COUNT") {
        return NStr::IntToString(bv->count());
    } else if (action == "DROP") {
    } else if (action == "FRES") {
    } else if (action == "SLCT") {
    } else if (action == "CNCL") {
    }

    return "";
}


void CQueue::PrintNodeStat(CNcbiOstream& out) const
{
    CRef<SLockedQueue> q(x_GetLQueue());

    const SLockedQueue::TListenerList& wnodes = q->wnodes;
    SLockedQueue::TListenerList::size_type lst_size;
    CReadLockGuard guard(q->wn_lock);
    lst_size = wnodes.size();
  
    time_t curr = time(0);

    for (SLockedQueue::TListenerList::size_type i = 0; i < lst_size; ++i) {
        const SQueueListener* ql = wnodes[i];
        time_t last_connect = ql->last_connect;

        // cut off one day old obsolete connections
        if ( (last_connect + (24 * 3600)) < curr) {
            continue;
        }

        CTime lc_time(last_connect);
        lc_time.ToLocalTime();
        out << ql->auth << " @ " << CSocketAPI::gethostbyaddr(ql->host) 
            << "  UDP:" << ql->udp_port << "  " << lc_time.AsString() << "\n";
    }
}


void CQueue::PrintSubmHosts(CNcbiOstream& out) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->subm_hosts.PrintHosts(out);
}


void CQueue::PrintWNodeHosts(CNcbiOstream& out) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->wnode_hosts.PrintHosts(out);
}


unsigned int 
CQueue::Submit(SNS_SubmitRecord* rec,
               unsigned      host_addr,
               unsigned      port,
               unsigned      wait_timeout,
               const char*   progress_msg)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    bool was_empty = !q->status_tracker.AnyPending();

    unsigned int job_id = q->GetNextId();

    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   job_id,
                                   CNetScheduleAPI::ePending);
    rec->job_id = job_id;
    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    rec->affinity_id = 0;
    if (*rec->affinity_token) { // not empty
        rec->affinity_id =
            q->affinity_dict.CheckToken(rec->affinity_token, trans);
    }

    {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);

        x_AssignSubmitRec(db,
            rec, time(0), host_addr, port, wait_timeout, progress_msg);

        db.Insert();

        // update affinity index
        if (rec->affinity_id) {
            q->aff_idx.SetTransaction(&trans);
            x_AddToAffIdx_NoLock(rec->affinity_id, job_id);
        }

        // update tags
        q->SetTagDbTransaction(&trans);
        TNSTagMap tag_map;
        q->AppendTags(tag_map, rec->tags, job_id);
        q->FlushTags(tag_map);
    }}

    trans.Commit();
    js_guard.Commit();

    if (was_empty) NotifyListeners(true);

    if ((job_id % 1000) == 0) {
        db.GetEnv()->TransactionCheckpoint();
    }
    if ((job_id % 1000000) == 0) {
        db.GetEnv()->CleanLog();
    }

    return job_id;
}


unsigned int 
CQueue::SubmitBatch(vector<SNS_SubmitRecord>& batch,
                    unsigned                  host_addr,
                    unsigned                  port,
                    unsigned                  wait_timeout,
                    const char*               progress_msg)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    bool was_empty = !q->status_tracker.AnyPending();

    unsigned job_id = q->GetNextIdBatch(batch.size());

    SQueueDB& db = q->db;

    unsigned batch_aff_id = 0; // if batch comes with the same affinity
    bool     batch_has_aff = false;

    // process affinity ids
    {{
        CBDB_Transaction trans(*db.GetEnv(), 
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);
        for (unsigned i = 0; i < batch.size(); ++i) {
            SNS_SubmitRecord& subm = batch[i];
            if (subm.affinity_id == (unsigned)kMax_I4) { // take prev. token
                _ASSERT(i > 0);
                subm.affinity_id = batch[i-1].affinity_id;
            } else {
                if (subm.affinity_token[0]) {
                    subm.affinity_id = 
                        q->affinity_dict.CheckToken(subm.affinity_token, trans);
                    batch_has_aff = true;
                    batch_aff_id = (i == 0 )? subm.affinity_id : 0;
                } else {
                    subm.affinity_id = 0;
                    batch_aff_id = 0;
                }

            }
        }
        trans.Commit();
    }}

    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);
    TNSTagMap tag_map;

    {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);

        q->SetTagDbTransaction(&trans);
        unsigned job_id_cnt = job_id;
        time_t now = time(0);
        NON_CONST_ITERATE(vector<SNS_SubmitRecord>, it, batch) {
            it->job_id = job_id_cnt;
            x_AssignSubmitRec(db,
                &(*it), now, host_addr, port, wait_timeout, progress_msg);
            ++job_id_cnt;
            //for (unsigned n_tries = 0; true; ) {
            //    try {
                    db.Insert();
            //    } catch (CBDB_ErrnoException& ex) {
            //        if ((ex.IsDeadLock() || ex.IsNoMem()) &&
            //            ++n_tries < k_max_dead_locks) {
            //            SleepMilliSec(250);
            //            continue;
            //        }
            //        ERR_POST("Too many transaction repeats in CQueue::SubmitBatch");
            //        throw;
            //    }
            //    break;
            //}
            // update tags
            q->AppendTags(tag_map, it->tags, it->job_id);
        }

        // Store the affinity index
        q->aff_idx.SetTransaction(&trans);
        if (batch_has_aff) {
            if (batch_aff_id) {  // whole batch comes with the same affinity
                x_AddToAffIdx_NoLock(batch_aff_id,
                                    job_id, 
                                    job_id + batch.size() - 1);
            } else {
                x_AddToAffIdx_NoLock(batch);
            }
        }
        q->FlushTags(tag_map);
    }}
    trans.Commit();

    q->status_tracker.AddPendingBatch(job_id, job_id + batch.size() - 1);

    if (was_empty) NotifyListeners(true);

    return job_id;
}


void 
CQueue::x_AssignSubmitRec(SQueueDB&     db,
                          const SNS_SubmitRecord* rec,
                          time_t        time_submit,
                          unsigned      host_addr,
                          unsigned      port,
                          unsigned      wait_timeout,
                          const char*   progress_msg)
{
    db.id = rec->job_id;
    db.status = (int) CNetScheduleAPI::ePending;

    db.time_submit = time_submit;
    db.time_run = 0;
    db.time_done = 0;
    db.timeout = 0;
    db.run_timeout = 0;

    db.subm_addr = host_addr;
    db.subm_port = port;
    db.subm_timeout = wait_timeout;

    db.worker_node1 = 0;
    db.worker_node2 = 0;
    db.worker_node3 = 0;
    db.worker_node4 = 0;
    db.worker_node5 = 0;

    db.run_counter = 0;
    db.ret_code = 0;
    db.time_lb_first_eval = 0;
    db.aff_id = rec->affinity_id;
    db.mask = rec->mask;

    if (*rec->input)
        db.input = rec->input;
    db.output = "";

    db.err_msg = "";

    if (progress_msg) {
        db.progress_msg = progress_msg;
    }
}


unsigned 
CQueue::CountStatus(CNetScheduleAPI::EJobStatus st) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return q->status_tracker.CountStatus(st);
}


void 
CQueue::StatusStatistics(
    CNetScheduleAPI::EJobStatus status,
    TNSBitVector::statistics* st) const
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->status_tracker.StatusStatistics(status, st);
}


void CQueue::ForceReschedule(unsigned int job_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    {{
        SQueueDB& db = q->db;
        CBDB_Transaction trans(*db.GetEnv(), 
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);

        CFastMutexGuard guard(q->lock);
        db.SetTransaction(0);

        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {
            //int status = db.status;
            db.status = (int) CNetScheduleAPI::ePending;
            db.worker_node5 = 0; 

            unsigned run_counter = db.run_counter;
            if (run_counter) {
                db.run_counter = --run_counter;
            }

            db.SetTransaction(&trans);
            db.UpdateInsert();
            trans.Commit();

        } else {
            // TODO: Integrity error or job just expired?
            return;
        }    
    }}

    q->status_tracker.ForceReschedule(job_id);
}


void CQueue::Cancel(unsigned int job_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());

//    CIdBusyGuard id_guard(&m_Db.m_UsedIds, job_id, 3);

    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   job_id,
                                   CNetScheduleAPI::eCanceled);
    CNetScheduleAPI::EJobStatus st = js_guard.GetOldStatus();
    if (q->status_tracker.IsCancelCode(st)) {
        js_guard.Commit();
        return;
    }

    {{
        SQueueDB& db = q->db;
        CBDB_Transaction trans(*db.GetEnv(), 
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);

        CFastMutexGuard guard(q->lock);
        db.SetTransaction(0);

        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {
            int status = db.status;
            if (status < (int) CNetScheduleAPI::eCanceled) {
                db.status = (int) CNetScheduleAPI::eCanceled;
                db.time_done = time(0);
                
                db.SetTransaction(&trans);
                db.UpdateInsert();
                trans.Commit();
            }
        } else {
            // TODO: Integrity error or job just expired?
        }    
    }}
    js_guard.Commit();

    x_RemoveFromTimeLine(job_id);
}


void CQueue::DropJob(unsigned job_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    q->Erase(job_id);
    x_RemoveFromTimeLine(job_id);

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::DropJob() job id=";
        msg += NStr::IntToString(job_id);
        q->monitor.SendString(msg);
    }
}


void CQueue::PutResult(unsigned int  job_id,
                       int           ret_code,
                       const char*   output)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   job_id,
                                   CNetScheduleAPI::eDone);
    if (q->delete_done) {
        q->Erase(job_id);
    }
    CNetScheduleAPI::EJobStatus st = js_guard.GetOldStatus();
    if (q->status_tracker.IsCancelCode(st)) {
        js_guard.Commit();
        return;
    }
    bool rec_updated;
    SSubmitNotifInfo si;
    time_t curr = time(0);

    SQueueDB& db = q->db;

    for (unsigned repeat = 0; true; ) {
        try {
            CBDB_Transaction trans(*db.GetEnv(), 
                                   CBDB_Transaction::eTransASync,
                                   CBDB_Transaction::eNoAssociation);

            {{
                CFastMutexGuard guard(q->lock);
                db.SetTransaction(&trans);
                CBDB_FileCursor& cur = *q->GetCursor(trans);
                CBDB_CursorGuard cg(cur);
                rec_updated = 
                    x_UpdateDB_PutResultNoLock(db, curr, cur, 
                                               job_id, ret_code, output, &si);
            }}

            trans.Commit();
            js_guard.Commit();
            if (job_id) x_Count(SLockedQueue::eStatPutEvent);
        } 
        catch (CBDB_ErrnoException& ex) {
            if (ex.IsDeadLock()) {
                if (++repeat < k_max_dead_locks) {
                    if (q->monitor.IsMonitorActive()) {
                        q->monitor.SendString(
                            "DeadLock repeat in CQueue::PutResult");
                    }
                    SleepMilliSec(250);
                    continue;
                }
            } else 
            if (ex.IsNoMem()) {
                if (++repeat < k_max_dead_locks) {
                    if (q->monitor.IsMonitorActive()) {
                        q->monitor.SendString(
                            "No resource repeat in CQueue::PutResult");
                    }
                    SleepMilliSec(250);
                    continue;
                }
            }
            ERR_POST("Too many transaction repeats in CQueue::PutResult.");
            throw;
        }
        break;
    } // for

    x_RemoveFromTimeLine(job_id);

    if (rec_updated) {
        // check if we need to send a UDP notification
        if ( si.subm_timeout && si.subm_addr && si.subm_port &&
            (si.time_submit + si.subm_timeout >= (unsigned)curr)) {

            char msg[1024];
            sprintf(msg, "JNTF %u", job_id);

            CFastMutexGuard guard(q->us_lock);

            //EIO_Status status = 
            q->udp_socket.Send(msg, strlen(msg)+1, 
                              CSocketAPI::ntoa(si.subm_addr), si.subm_port);
        }

        // Update runtime statistics

        unsigned run_elapsed = curr - si.time_run;
        unsigned turn_around = curr - si.time_submit;
        {{
            CFastMutexGuard guard(q->qstat_lock);
            q->qstat.run_count++;
            q->qstat.total_run_time += run_elapsed;
            q->qstat.total_turn_around_time += turn_around;
        }}

    }

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::PutResult() job id=";
        msg += NStr::IntToString(job_id);
        msg += " ret_code=";
        msg += NStr::IntToString(ret_code);
        msg += " output=";
        msg += output;

        q->monitor.SendString(msg);
    }
}


bool 
CQueue::x_UpdateDB_PutResultNoLock(SQueueDB&            db,
                                   time_t               curr,
                                   CBDB_FileCursor&     cur,
                                   unsigned             job_id,
                                   int                  ret_code,
                                   const char*          output,
                                   SSubmitNotifInfo*    subm_info)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        // TODO: Integrity error or job just expired?
        return false;
    }

    if (subm_info) {
        subm_info->time_submit  = db.time_submit;
        subm_info->time_run     = db.time_run;
        subm_info->subm_addr    = db.subm_addr;
        subm_info->subm_port    = db.subm_port;
        subm_info->subm_timeout = db.subm_timeout;
    }

    if (q->delete_done) {
        cur.Delete();
    } else {
        db.status = (int) CNetScheduleAPI::eDone;
        db.ret_code = ret_code;
        db.output = output;
        db.time_done = curr;

        cur.Update();
    }
    return true;
}


SQueueDB* CQueue::x_GetLocalDb()
{
    return 0;
    CRef<SLockedQueue> q(x_GetLQueue());
    SQueueDB* pqdb = m_QueueDB.get();
    if (pqdb == 0  &&  ++m_QueueDbAccessCounter > 1) {
        printf("Opening private db\n"); // DEBUG
        const string& file_name = q->db.FileName();
        CBDB_Env* env = q->db.GetEnv();
        m_QueueDB.reset(pqdb = new SQueueDB());
        if (env) {
            pqdb->SetEnv(*env);
        }
        pqdb->Open(file_name.c_str(), CBDB_RawFile::eReadWrite);
    }
    return pqdb;
}


CBDB_FileCursor* 
CQueue::x_GetLocalCursor(CBDB_Transaction& trans)
{
    CBDB_FileCursor* pcur = m_QueueDB_Cursor.get();
    if (pcur) {
        pcur->ReOpen(&trans);
        return pcur;
    }

    SQueueDB* pqdb = m_QueueDB.get();
    if (pqdb == 0) {
        return 0;
    }
    pcur = new CBDB_FileCursor(*pqdb, 
                               trans,
                               CBDB_FileCursor::eReadModifyUpdate);
    m_QueueDB_Cursor.reset(pcur);
    return pcur;
}


void 
CQueue::PutResultGetJob(unsigned int   done_job_id,
                        int            ret_code,
                        const char*    output,
                        unsigned int   worker_node,
                        unsigned int*  job_id,
                        char*          input,
//                        char*          jout,
//                        char*          jerr,
                        const string&  client_name,
                        unsigned*      job_mask)
{
    _ASSERT(job_id);
    _ASSERT(input);
    _ASSERT(job_mask);

    CRef<SLockedQueue> q(x_GetLQueue());

    unsigned dead_locks = 0;       // dead lock counter

    time_t curr = time(0);
    
    // 
    bool need_update;
    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   done_job_id,
                                   CNetScheduleAPI::eDone,
                                   &need_update);
    // This is a HACK - if js_guard is not commited, it will rollback
    // to previous state, so it is safe to change status after the guard.
    if (q->delete_done) {
        q->Erase(done_job_id);
    }
    // TODO: implement transaction wrapper (a la js_guard above)
    // for x_FindPendingJob
    // TODO: move affinity assignment there as well
    unsigned pending_job_id = x_FindPendingJob(client_name, worker_node);
    bool done_rec_updated = false;
    bool use_db_mutex;

    // When working with the same database file concurrently there is
    // chance of internal Berkeley DB deadlock. (They say it's legal!)
    // In this case Berkeley DB returns an error code(DB_LOCK_DEADLOCK)
    // and recovery is up to the application.
    // If it happens I repeat the transaction several times.
    //
repeat_transaction:
    SQueueDB* pqdb = x_GetLocalDb();
    if (pqdb) {
        // we use private (this thread only data file)
        use_db_mutex = false;
    } else {
        use_db_mutex = true;
        pqdb = &q->db;
    }

    unsigned job_aff_id = 0;

    try {
        CBDB_Transaction trans(*(pqdb->GetEnv()), 
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);

        if (use_db_mutex) {
            q->lock.Lock();
        }
        pqdb->SetTransaction(&trans);

        CBDB_FileCursor* pcur;
        if (use_db_mutex) {
            pcur = q->GetCursor(trans);
        } else {
            pcur = x_GetLocalCursor(trans);
        }
        CBDB_FileCursor& cur = *pcur;
        {{
            CBDB_CursorGuard cg(cur);

            // put result
            if (need_update) {
                done_rec_updated =
                    x_UpdateDB_PutResultNoLock(
                      *pqdb, curr, cur, done_job_id, ret_code, output, NULL);
            }

            if (pending_job_id) {
                *job_id = pending_job_id;
                EGetJobUpdateStatus upd_status;
                upd_status =
                    x_UpdateDB_GetJobNoLock(*pqdb, curr, cur, trans,
                                         worker_node, pending_job_id, input,
                                         &job_aff_id,
                                         job_mask);
                switch (upd_status) {
                case eGetJobUpdate_JobFailed:
                    q->status_tracker.ChangeStatus(*job_id, 
                                                   CNetScheduleAPI::eFailed);
                    *job_id = 0;
                    break;
                case eGetJobUpdate_JobStopped:
                    *job_id = 0;
                    break;
                case eGetJobUpdate_NotFound:
                    *job_id = 0;
                    break;
                case eGetJobUpdate_Ok:
                    break;
                default:
                    _ASSERT(0);
                } // switch

            } else {
                *job_id = 0;
            }
        }}

        if (use_db_mutex) {
            q->lock.Unlock();
        }

        trans.Commit();
        js_guard.Commit();
        // TODO: commit x_FindPendingJob guard here
        if (done_job_id) x_Count(SLockedQueue::eStatPutEvent);
        if (*job_id) x_Count(SLockedQueue::eStatGetEvent);
    }
    catch (CBDB_ErrnoException& ex) {
        if (use_db_mutex) {
            q->lock.Unlock();
        }
        if (ex.IsDeadLock()) {
            if (++dead_locks < k_max_dead_locks) {
                if (q->monitor.IsMonitorActive()) {
                    q->monitor.SendString(
                        "DeadLock repeat in CQueue::JobExchange");
                }
                SleepMilliSec(250);
                goto repeat_transaction;
            } 
        }
        else
        if (ex.IsNoMem()) {
            if (++dead_locks < k_max_dead_locks) {
                if (q->monitor.IsMonitorActive()) {
                    q->monitor.SendString(
                        "No resource repeat in CQueue::JobExchange");
                }
                SleepMilliSec(250);
                goto repeat_transaction;
            }
        }
        ERR_POST("Too many transaction repeats in CQueue::JobExchange.");
        throw;
    }
    catch (...) {
        if (use_db_mutex) {
            q->lock.Unlock();
        }
        throw;
    }

    if (job_aff_id) {
        CFastMutexGuard aff_guard(q->aff_map_lock);
        q->worker_aff_map.AddAffinity(worker_node, client_name,
                                      job_aff_id);
    }

    x_TimeLineExchange(done_job_id, *job_id, curr);

    if (done_rec_updated) {
        // TODO: send a UDP notification and update runtime stat
    }

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();

        msg += " CQueue::PutResultGetJob() (PUT) job id=";
        msg += NStr::IntToString(done_job_id);
        msg += " ret_code=";
        msg += NStr::IntToString(ret_code);
        msg += " output=";
        msg += output;

        q->monitor.SendString(msg);
    }
}


void CQueue::PutProgressMessage(unsigned int  job_id,
                                const char*   msg)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    CFastMutexGuard guard(q->lock);
    db.SetTransaction(&trans);

    {{
    CBDB_FileCursor& cur = *q->GetCursor(trans);
    CBDB_CursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        // TODO: Integrity error or job just expired?
        return;
    }
    db.progress_msg = msg;
    cur.Update();
    }}

    trans.Commit();

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string mmsg = tmp_t.AsString();
        mmsg += " CQueue::PutProgressMessage() job id=";
        mmsg += NStr::IntToString(job_id);
        mmsg += " msg=";
        mmsg += msg;

        q->monitor.SendString(mmsg);
    }

}


void CQueue::JobFailed(unsigned int  job_id,
                       const string& err_msg,
                       const string& output,
                       int           ret_code,
                       unsigned int  worker_node,
                       const string& client_name)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    // We first change memory state to "Failed", it is safer because
    // there is only danger to find job in inconsistent state, and because
    // Failed is terminal, usually you can not allocate job or do anything
    // disturbing from this state.
    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   job_id,
                                   CNetScheduleAPI::eFailed);

    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    unsigned subm_addr, subm_port, subm_timeout, time_submit;
    time_t curr = time(0);

    {{
        CFastMutexGuard aff_guard(q->aff_map_lock);
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *q->GetCursor(trans);
        CBDB_CursorGuard cg(cur);    

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << job_id;

        if (cur.FetchFirst() != eBDB_Ok) {
            // TODO: Integrity error or job just expired?
            return;
        }

        db.time_done = curr;
        db.err_msg = err_msg;
        db.output = output;
        db.ret_code = ret_code;

        unsigned run_counter = db.run_counter;
        if (run_counter <= q->failed_retries) {
            // NB: due to conflict with locking pattern of x_FindPendingJob we can not
            // acquire aff_map_lock here! So we aquire it earlier (see above).
            q->worker_aff_map.BlacklistJob(worker_node, client_name, job_id);
            // Pending status is not a bug here, returned and pending
            // has the same meaning, but returned jobs are getting delayed
            // for a little while (eReturned status)
            db.status = (int) CNetScheduleAPI::ePending;
            // We can do this because js_guard record only old state and
            // on Commit just releases job.
            q->status_tracker.SetStatus(job_id,
                CNetScheduleAPI::eReturned);
        } else {
            db.status = (int) CNetScheduleAPI::eFailed;
        }

        // We don't need to lock affinity map anymore, so to reduce locking
        // region we can manually release it here.
        aff_guard.Release();

        time_submit = db.time_submit;
        subm_addr = db.subm_addr;
        subm_port = db.subm_port;
        subm_timeout = db.subm_timeout;

        cur.Update();
    }}

    trans.Commit();
    js_guard.Commit();

    x_RemoveFromTimeLine(job_id);

    // check if we need to send a UDP notification
    if (subm_addr && subm_timeout &&
        (time_submit + subm_timeout >= (unsigned)curr)) {

        char msg[1024];
        sprintf(msg, "JNTF %u", job_id);

        CFastMutexGuard guard(q->us_lock);

        q->udp_socket.Send(msg, strlen(msg)+1, 
                                  CSocketAPI::ntoa(subm_addr), subm_port);
    }

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::JobFailed() job id=";
        msg += NStr::IntToString(job_id);
        msg += " err_msg=";
        msg += err_msg;
        msg += " output=";
        msg += output;
        if (db.status == (int) CNetScheduleAPI::ePending)
            msg += " rescheduled";
        q->monitor.SendString(msg);
    }
}


void CQueue::SetJobRunTimeout(unsigned job_id, unsigned tm)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    CNetScheduleAPI::EJobStatus st = GetStatus(job_id);
    if (st != CNetScheduleAPI::eRunning) {
        return;
    }
    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    unsigned exp_time = 0;
    time_t curr = time(0);

    {{
    CFastMutexGuard guard(q->lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *q->GetCursor(trans);
    CBDB_CursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        return;
    }

    unsigned time_run = db.time_run;
    unsigned run_timeout = db.run_timeout;

    exp_time = x_ComputeExpirationTime(time_run, run_timeout);
    if (exp_time == 0) {
        return;
    }

    db.run_timeout = tm;
    db.time_run = curr;

    cur.Update();

    }}

    trans.Commit();

    {{
        CJobTimeLine& tl = *q->run_time_line;

        CWriteLockGuard guard(q->rtl_lock);
        tl.MoveObject(exp_time, curr + tm, job_id);
    }}

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::SetJobRunTimeout: Job id=";
        msg += NStr::IntToString(job_id);
        tmp_t.SetTimeT(curr + tm);
        tmp_t.ToLocalTime();
        msg += " new_expiration_time=";
        msg += tmp_t.AsString();
        msg += " job_timeout(sec)=";
        msg += NStr::IntToString(tm);
        msg += " job_timeout(minutes)=";
        msg += NStr::IntToString(tm/60);

        q->monitor.SendString(msg);
    }
}

void CQueue::JobDelayExpiration(unsigned job_id, unsigned tm)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    unsigned q_time_descr = 20;
    unsigned run_timeout;
    unsigned time_run;
    unsigned old_run_timeout;

    CNetScheduleAPI::EJobStatus st = GetStatus(job_id);
    if (st != CNetScheduleAPI::eRunning) {
        return;
    }
    SQueueDB& db = q->db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    unsigned exp_time = 0;
    time_t curr = time(0);

    {{
    CFastMutexGuard guard(q->lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *q->GetCursor(trans);
    CBDB_CursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        return;
    }

    //    int status = db.status;

    time_run = db.time_run;
    run_timeout = db.run_timeout;
    if (run_timeout == 0) {
        run_timeout = q->run_timeout;
    }
    old_run_timeout = run_timeout;

    // check if current timeout is enought and job requires no prolongation
    time_t safe_exp_time = 
        curr + std::max((unsigned)q->run_timeout, 2*tm) + q_time_descr;
    if (time_run + run_timeout > (unsigned) safe_exp_time) {
        return;
    }

    if (time_run == 0) {
        time_run = curr;
        db.time_run = curr;
    }

    while (time_run + run_timeout <= (unsigned) safe_exp_time) {
        run_timeout += std::max((unsigned)q->run_timeout, tm);
    }
    db.run_timeout = run_timeout;

    cur.Update();

    }}

    trans.Commit();
    exp_time = x_ComputeExpirationTime(time_run, run_timeout);


    {{
        CJobTimeLine& tl = *q->run_time_line;

        CWriteLockGuard guard(q->rtl_lock);
        tl.MoveObject(exp_time, curr + tm, job_id);
    }}

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::JobDelayExpiration: Job id=";
        msg += NStr::IntToString(job_id);
        tmp_t.SetTimeT(curr + tm);
        tmp_t.ToLocalTime();
        msg += " new_expiration_time=";
        msg += tmp_t.AsString();
        msg += " job_timeout(sec)=";
        msg += NStr::IntToString(run_timeout);
        msg += " job_timeout(minutes)=";
        msg += NStr::IntToString(run_timeout/60);

        q->monitor.SendString(msg);
    }
}


void CQueue::ReturnJob(unsigned int job_id)
{
    _ASSERT(job_id);

    CRef<SLockedQueue> q(x_GetLQueue());

    CNetSchedule_JS_Guard js_guard(q->status_tracker, 
                                   job_id,
                                   CNetScheduleAPI::eReturned);
    CNetScheduleAPI::EJobStatus st = js_guard.GetOldStatus();
    // if canceled or already returned or done
    if (q->status_tracker.IsCancelCode(st) || 
        (st == CNetScheduleAPI::eReturned) || 
        (st == CNetScheduleAPI::eDone)) {
        js_guard.Commit();
        return;
    }
    {{
        SQueueDB& db = q->db;

        CFastMutexGuard guard(q->lock);
        db.SetTransaction(0);

        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {            
            CBDB_Transaction trans(*db.GetEnv(), 
                                CBDB_Transaction::eTransASync,
                                CBDB_Transaction::eNoAssociation);
            db.SetTransaction(&trans);

            // Pending status is not a bug here, returned and pending
            // has the same meaning, but returned jobs are getting delayed
            // for a little while (eReturned status)
            db.status = (int) CNetScheduleAPI::ePending;
            unsigned run_counter = db.run_counter;
            if (run_counter) {
                db.run_counter = --run_counter;
            }

            db.UpdateInsert();

            trans.Commit();
        } else {
            // TODO: Integrity error or job just expired?
        }    
    }}
    js_guard.Commit();
    x_RemoveFromTimeLine(job_id);

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::ReturnJob: job id=";
        msg += NStr::IntToString(job_id);
        q->monitor.SendString(msg);
    }
}

void CQueue::x_GetJobLB(unsigned int   worker_node,
                        unsigned int*  job_id, 
                        char*          input,
                        unsigned*      job_mask)
{
    _ASSERT(worker_node && input);
    _ASSERT(job_mask);

    CRef<SLockedQueue> q(x_GetLQueue());

    unsigned get_attempts = 0;
    unsigned fetch_attempts = 0;
    const unsigned kMaxGetAttempts = 100;


    ++get_attempts;
    if (get_attempts > kMaxGetAttempts) {
        *job_id = 0;
        return;
    }

    *job_id = q->status_tracker.BorrowPendingJob();

    if (!*job_id) {
        return;
    }
    CNetSchedule_JS_BorrowGuard bguard(q->status_tracker, 
                                       *job_id,
                                       CNetScheduleAPI::ePending);

    time_t curr = time(0);
    unsigned lb_stall_time = 0;
    ENSLB_RunDelayType stall_delay_type;


    //
    // Define current execution delay
    //

    {{
        CFastMutexGuard guard(q->lb_stall_time_lock);
        stall_delay_type = q->lb_stall_delay_type;
        lb_stall_time    = q->lb_stall_time;
    }}

    switch (stall_delay_type) {
    case eNSLB_Constant:
        break;
    case eNSLB_RunTimeAvg:
        {
            double total_run_time, run_count, lb_stall_time_mult;

            {{
            CFastMutexGuard guard(q->qstat_lock);
            if (q->qstat.run_count) {
                total_run_time = q->qstat.total_run_time;
                run_count = q->qstat.run_count;
                lb_stall_time_mult = q->lb_stall_time_mult;
            } else {
                // no statistics yet, nothing to do, take default queue value
                break;
            }
            }}

            double avg_time = total_run_time / run_count;
            lb_stall_time = (unsigned)(avg_time * lb_stall_time_mult);
        }
        break;
    default:
        _ASSERT(0);
    } // switch

    if (lb_stall_time == 0) {
        lb_stall_time = 6;
    }


fetch_db:
    ++fetch_attempts;
    if (fetch_attempts > kMaxGetAttempts) {
        LOG_POST(Error << "Failed to fetch the job record job_id=" << *job_id);
        *job_id = 0;
        return;
    }
 

    try {
        SQueueDB& db = q->db;
        CBDB_Transaction trans(*db.GetEnv(),
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);
        {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *q->GetCursor(trans);
        CBDB_CursorGuard cg(cur);    

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << *job_id;
        if (cur.Fetch() != eBDB_Ok) {
            if (fetch_attempts < kMaxGetAttempts) {
                goto fetch_db;
            }
            *job_id = 0;
            return;
        }
        CNetScheduleAPI::EJobStatus status = 
            (CNetScheduleAPI::EJobStatus)(int)db.status;

        // internal integrity check
        if (!(status == CNetScheduleAPI::ePending ||
              status == CNetScheduleAPI::eReturned)
            ) {
            if (q->status_tracker.IsCancelCode(
                (CNetScheduleAPI::EJobStatus) status)) {
                // this job has been canceled while i'm fetching
                *job_id = 0;
                return;
            }
                ERR_POST(Error << "x_GetJobLB()::Status integrity violation "
                            << " job = " << *job_id
                            << " status = " << status
                            << " expected status = "
                            << (int)CNetScheduleAPI::ePending);
            *job_id = 0;
            return;
        }

        const char* fld_str = db.input;
        ::strcpy(input, fld_str);

        unsigned run_counter = db.run_counter;

        unsigned time_submit = db.time_submit;
        unsigned timeout = db.timeout;
        if (timeout == 0) {
            timeout = q->timeout;
        }
        *job_mask = db.mask;

        _ASSERT(timeout);

        // check if job already expired
        if (timeout && (time_submit + timeout < (unsigned)curr)) {
            *job_id = 0; 
            db.time_run = 0;
            db.time_done = curr;
            db.status = (int) CNetScheduleAPI::eFailed;
            db.err_msg = "Job expired and cannot be scheduled.";

            bguard.ReturnToStatus(CNetScheduleAPI::eFailed);

            if (q->monitor.IsMonitorActive()) {
                CTime tmp_t(CTime::eCurrent);
                string msg = tmp_t.AsString();
                msg += " CQueue::x_GetJobLB() timeout expired job id=";
                msg += NStr::IntToString(*job_id);
                q->monitor.SendString(msg);
            }

            cur.Update();
            trans.Commit();

            return;
        } 

        CNSLB_Coordinator::ENonLbHostsPolicy unk_host_policy =
            q->lb_coordinator->GetNonLBPolicy();

        // check if job must be scheduled immediatelly, 
        // because it's stalled for too long
        //
        unsigned time_lb_first_eval = db.time_lb_first_eval;
        unsigned stall_time = 0; 
        if (time_lb_first_eval) {
            stall_time = curr - time_lb_first_eval;
            if (lb_stall_time) {
                if (stall_time > lb_stall_time) {
                    // stalled for too long! schedule the job
                    if (unk_host_policy != CNSLB_Coordinator::eNonLB_Deny) {
                        goto grant_job;
                    }
                }
            } else {
                // exec delay not set, schedule the job
                goto grant_job;
            }
        } else { // first LB evaluation
            db.time_lb_first_eval = curr;
        }

        // job can be scheduled now, if load balancing situation is permitting
        {{
        CNSLB_Coordinator* coordinator = q->lb_coordinator;
        if (coordinator) {
            CNSLB_DecisionModule::EDecision lb_decision = 
                coordinator->Evaluate(worker_node, stall_time);
            switch (lb_decision) {
            case CNSLB_DecisionModule::eGrantJob:
                break;
            case CNSLB_DecisionModule::eDenyJob:
                *job_id = 0;
                break;
            case CNSLB_DecisionModule::eHostUnknown:
                if (unk_host_policy != CNSLB_Coordinator::eNonLB_Allow) {
                    *job_id = 0;
                }
                break;
            case CNSLB_DecisionModule::eNoLBInfo:
                break;
            case CNSLB_DecisionModule::eInsufficientInfo:
                break;
            default:
                _ASSERT(0);
            } // switch

            if (q->monitor.IsMonitorActive()) {
                CTime tmp_t(CTime::eCurrent);
                string msg = " CQueue::x_GetJobLB() LB decision = ";
                msg += CNSLB_DecisionModule::DecisionToStrint(lb_decision);
                msg += " job id = ";
                msg += NStr::IntToString(*job_id);
                msg += " worker_node=";
                msg += CSocketAPI::gethostbyaddr(worker_node);
                q->monitor.SendString(msg);
            }

        }
        }}
grant_job:
        // execution granted, update job record information
        if (*job_id) {
            db.status = (int) CNetScheduleAPI::eRunning;
            db.time_run = curr;
            db.run_timeout = 0;
            db.run_counter = ++run_counter;

            switch (run_counter) {
            case 1:
                db.worker_node1 = worker_node;
                break;
            case 2:
                db.worker_node2 = worker_node;
                break;
            case 3:
                db.worker_node3 = worker_node;
                break;
            case 4:
                db.worker_node4 = worker_node;
                break;
            case 5:
                db.worker_node5 = worker_node;
                break;
            default:

                bguard.ReturnToStatus(CNetScheduleAPI::eFailed);
                ERR_POST(Error << "Too many run attempts. job=" << *job_id);
                db.status = (int) CNetScheduleAPI::eFailed;
                db.err_msg = "Too many run attempts.";
                db.time_done = curr;
                db.run_counter = --run_counter;

                if (q->monitor.IsMonitorActive()) {
                    CTime tmp_t(CTime::eCurrent);
                    string msg = tmp_t.AsString();
                    msg += " CQueue::x_GetJobLB() Too many run attempts job id=";
                    msg += NStr::IntToString(*job_id);
                    q->monitor.SendString(msg);
                }

                *job_id = 0; 

            } // switch

        } // if (*job_id)

        cur.Update();

        }}
        trans.Commit();

    } 
    catch (exception&)
    {
        *job_id = 0;
        throw;
    }
    bguard.ReturnToStatus(CNetScheduleAPI::eRunning);

    if (q->monitor.IsMonitorActive() && *job_id) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::x_GetJobLB() job id=";
        msg += NStr::IntToString(*job_id);
        msg += " worker_node=";
        msg += CSocketAPI::gethostbyaddr(worker_node);
        q->monitor.SendString(msg);
    }


    x_AddToTimeLine(*job_id, curr);
}


unsigned 
CQueue::x_FindPendingJob(const string&  client_name,
                       unsigned       client_addr)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    unsigned job_id = 0;

    TNSBitVector blacklisted_jobs;

    // affinity: get list of job candidates
    // previous x_FindPendingJob() call may have precomputed candidate jobids
    {{
        CFastMutexGuard aff_guard(q->aff_map_lock);
        CWorkerNodeAffinity::SAffinityInfo* ai = 
            q->worker_aff_map.GetAffinity(client_addr, client_name);

        if (ai != 0) {  // established affinity association
            blacklisted_jobs = ai->blacklisted_jobs;
            do {
                // check for candidates
                if (!ai->candidate_jobs.any() && ai->aff_ids.any()) {
                    // there is an affinity association
                    {{
                        CFastMutexGuard guard(q->lock);
                        x_ReadAffIdx_NoLock(ai->aff_ids, &ai->candidate_jobs);
                    }}
                    if (!ai->candidate_jobs.any()) // no candidates
                        break;
                    q->status_tracker.PendingIntersect(&ai->candidate_jobs);
                    ai->candidate_jobs -= blacklisted_jobs;
                    ai->candidate_jobs.count(); // speed up any()
                    if (!ai->candidate_jobs.any())
                        break;
                    ai->candidate_jobs.optimize(0, TNSBitVector::opt_free_0);
                }
                if (!ai->candidate_jobs.any())
                    break;
                bool pending_jobs_avail = 
                    q->status_tracker.GetPendingJobFromSet(
                        &ai->candidate_jobs, &job_id);
                if (job_id)
                    return job_id;
                if (!pending_jobs_avail)
                    return 0;
            } while (0);
        }
    }}

    // no affinity association or there are no more jobs with 
    // established affinity

    // try to find a vacant(not taken by any other worker node) affinity id
    {{
        TNSBitVector assigned_aff;
        {{
            CFastMutexGuard aff_guard(q->aff_map_lock);
            q->worker_aff_map.GetAllAssignedAffinity(&assigned_aff);
        }}

        if (assigned_aff.any()) {
            // get all jobs belonging to other (already assigned) affinities,
            // ORing them with our own blacklisted jobs
            TNSBitVector assigned_candidate_jobs(blacklisted_jobs);
            {{
                CFastMutexGuard guard(q->lock);
                // x_ReadAffIdx_NoLock actually ORs into second argument
                x_ReadAffIdx_NoLock(assigned_aff, &assigned_candidate_jobs);
            }}
            // we got list of jobs we do NOT want to schedule
            bool pending_jobs_avail = 
                q->status_tracker.GetPendingJob(assigned_candidate_jobs,
                                               &job_id);
            if (job_id)
                return job_id;
            if (!pending_jobs_avail) {
                return 0;
            }
        }
    }}

    // We just take the first available job in the queue, taking into account
    // blacklisted jobs as usual.
    _ASSERT(job_id == 0);
    q->status_tracker.GetPendingJob(blacklisted_jobs, &job_id);

    return job_id;
}


void CQueue::GetJob(unsigned int   worker_node,
                    // ??? user SNS_SubmitRecord here
                    unsigned int*  job_id, 
                    char*          input,
                    const string&  client_name,
                    unsigned*      job_mask)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    if (q->lb_flag) {
        x_GetJobLB(worker_node, job_id, input, job_mask);
        return;
    }

    _ASSERT(worker_node && input);
    unsigned get_attempts = 0;
    const unsigned kMaxGetAttempts = 100;
    EGetJobUpdateStatus upd_status;

get_job_id:

    ++get_attempts;
    if (get_attempts > kMaxGetAttempts) {
        *job_id = 0;
        return;
    }
    time_t curr = time(0);

    // affinity: get list of job candidates
    // previous GetJob() call may have precomputed sutable job ids
    //

    *job_id = x_FindPendingJob(client_name, worker_node);
    if (!*job_id) {
        return;
    }

    unsigned job_aff_id = 0;

    try {
        SQueueDB& db = q->db;
        CBDB_Transaction trans(*db.GetEnv(), 
                               CBDB_Transaction::eTransASync,
                               CBDB_Transaction::eNoAssociation);


        {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *q->GetCursor(trans);
        CBDB_CursorGuard cg(cur);

        upd_status =
            x_UpdateDB_GetJobNoLock(db, curr, cur, trans,
                                    worker_node, *job_id, input, /*jout, jerr,*/
                                    &job_aff_id, job_mask);
        }}
        trans.Commit();

        switch (upd_status) {
        case eGetJobUpdate_JobFailed:
            q->status_tracker.ChangeStatus(*job_id,
                                           CNetScheduleAPI::eFailed);
            *job_id = 0;
            break;
        case eGetJobUpdate_JobStopped:
            *job_id = 0;
            break;
        case eGetJobUpdate_NotFound:
            *job_id = 0;
            break;
        case eGetJobUpdate_Ok:
            break;
        default:
            _ASSERT(0);
        } // switch

        if (*job_id) x_Count(SLockedQueue::eStatGetEvent);

        if (q->monitor.IsMonitorActive() && *job_id) {
            CTime tmp_t(CTime::eCurrent);
            string msg = tmp_t.AsString();
            msg += " CQueue::GetJob() job id=";
            msg += NStr::IntToString(*job_id);
            msg += " worker_node=";
            msg += CSocketAPI::gethostbyaddr(worker_node);
            q->monitor.SendString(msg);
        }

    } 
    catch (exception&)
    {
        q->status_tracker.ChangeStatus(*job_id, CNetScheduleAPI::ePending);
        *job_id = 0;
        throw;
    }

    // if we picked up expired job and need to re-get another job id    
    if (*job_id == 0) {
        goto get_job_id;
    }

    if (job_aff_id) {
        CFastMutexGuard aff_guard(q->aff_map_lock);
        q->worker_aff_map.AddAffinity(worker_node,
                                      client_name,
                                      job_aff_id);
    }

    x_AddToTimeLine(*job_id, curr);
}


CQueue::EGetJobUpdateStatus 
CQueue::x_UpdateDB_GetJobNoLock(SQueueDB&            db,
                                time_t               curr,
                                CBDB_FileCursor&     cur,
                                CBDB_Transaction&    trans,
                                unsigned int         worker_node,
                                unsigned             job_id,
                                // ??? use SNS_SubmitRecord here
                                char*                input,
                                unsigned*            aff_id,
                                unsigned*            job_mask)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    const unsigned kMaxGetAttempts = 100;

    for (unsigned fetch_attempts = 0; fetch_attempts < kMaxGetAttempts;
         ++fetch_attempts) {

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << job_id;

        if (cur.Fetch() != eBDB_Ok) {
            cur.Close();
            cur.ReOpen(&trans);
            continue;
        }
        int status = db.status;
        *aff_id = db.aff_id;

        // internal integrity check
        if (!(status == (int)CNetScheduleAPI::ePending ||
              status == (int)CNetScheduleAPI::eReturned)
            ) {
            if (q->status_tracker.IsCancelCode(
                (CNetScheduleAPI::EJobStatus) status)) {
                // this job has been canceled while i'm fetching
                return eGetJobUpdate_JobStopped;
            }
            ERR_POST(Error 
                << "x_UpdateDB_GetJobNoLock::Status integrity violation "
                << " job = "     << job_id
                << " status = "  << status
                << " expected status = "
                << (int)CNetScheduleAPI::ePending);
            return eGetJobUpdate_JobStopped;
        }

        const char* fld_str = db.input;
        ::strcpy(input, fld_str);

        unsigned run_counter = db.run_counter;
        *job_mask = db.mask;

        db.status = (int) CNetScheduleAPI::eRunning;
        db.time_run = curr;
        db.run_timeout = 0;
        db.run_counter = ++run_counter;

        unsigned time_submit = db.time_submit;
        unsigned timeout = db.timeout;
        if (timeout == 0) {
            timeout = q->timeout;
        }

        _ASSERT(timeout);
        // check if job already expired
        if (timeout && (time_submit + timeout < (unsigned)curr)) {
            db.time_run = 0;
            db.time_done = curr;
            db.run_counter = --run_counter;
            db.status = (int) CNetScheduleAPI::eFailed;
            db.err_msg = "Job expired and cannot be scheduled.";
            q->status_tracker.ChangeStatus(job_id, 
                                           CNetScheduleAPI::eFailed);

            cur.Update();

            if (q->monitor.IsMonitorActive()) {
                CTime tmp_t(CTime::eCurrent);
                string msg = tmp_t.AsString();
                msg += 
                 " CQueue::x_UpdateDB_GetJobNoLock() timeout expired job id=";
                msg += NStr::IntToString(job_id);
                q->monitor.SendString(msg);
            }
            return eGetJobUpdate_JobFailed;

        } else {  // job not expired
            switch (run_counter) {
            case 1:
                db.worker_node1 = worker_node;
                break;
            case 2:
                db.worker_node2 = worker_node;
                break;
            case 3:
                db.worker_node3 = worker_node;
                break;
            case 4:
                db.worker_node4 = worker_node;
                break;
            case 5:
                db.worker_node5 = worker_node;
                break;
            default:
                q->status_tracker.ChangeStatus(job_id, 
                                               CNetScheduleAPI::eFailed);
                ERR_POST(Error << "Too many run attempts. job=" << job_id);
                db.status = (int) CNetScheduleAPI::eFailed;
                db.err_msg = "Too many run attempts.";
                db.time_done = curr;
                db.run_counter = --run_counter;

                cur.Update();

                if (q->monitor.IsMonitorActive()) {
                    CTime tmp_t(CTime::eCurrent);
                    string msg = tmp_t.AsString();
                    msg += " CQueue::GetJob() Too many run attempts job id=";
                    msg += NStr::IntToString(job_id);
                    q->monitor.SendString(msg);
                }

                return eGetJobUpdate_JobFailed;
            } // switch

            // all checks passed successfully...
            cur.Update();
            return eGetJobUpdate_Ok;
        } // else
    } // for

    return eGetJobUpdate_NotFound;
}


void CQueue::GetJobKey(char* key_buf, unsigned job_id,
                       const string& host, unsigned port)
{
    sprintf(key_buf, NETSCHEDULE_JOBMASK, job_id, host.c_str(), port);
}

bool 
CQueue::GetJobDescr(unsigned int job_id,
                    int*         ret_code,
                    char*        input,
                    char*        output,
                    char*        err_msg,
                    char*        progress_msg,
                    CNetScheduleAPI::EJobStatus expected_status)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    SQueueDB& db = q->db;

    for (unsigned i = 0; i < 3; ++i) {
        {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(0);

        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {
            if (expected_status != CNetScheduleAPI::eJobNotFound) {
                CNetScheduleAPI::EJobStatus status =
                    (CNetScheduleAPI::EJobStatus)(int)db.status;
                if ((status != expected_status) 
                    // The 'Retuned' status does not get saved into db
                    // because it is temporary. (optimization).
                    // this condition reflects that logically Pending == Returned
                    && !(status ==  CNetScheduleAPI::ePending 
                         && expected_status == CNetScheduleAPI::eReturned)) {
                    goto wait_sleep;
                }
            }
            if (ret_code)
                *ret_code = db.ret_code;

            if (input) {
                ::strcpy(input, (const char* )db.input);
            }
            if (output) {
                ::strcpy(output, (const char* )db.output);
            }
            if (err_msg) {
                ::strcpy(err_msg, (const char* )db.err_msg);
            }
            if (progress_msg) {
                ::strcpy(progress_msg, (const char* )db.progress_msg);
            }

            return true;
        }
        }}
    wait_sleep:
        // failed to read the record (maybe looks like writer is late, so we 
        // need to retry a bit later)
        SleepMilliSec(300);
    }

    return false; // job not found
}

CNetScheduleAPI::EJobStatus 
CQueue::GetStatus(unsigned int job_id) const
{
    const CRef<SLockedQueue> q(x_GetLQueue());
    return q->status_tracker.GetStatus(job_id);
}

bool CQueue::CountStatus(CJobStatusTracker::TStatusSummaryMap* status_map,
                         const char*                           affinity_token)
{
    _ASSERT(status_map);
    CRef<SLockedQueue> q(x_GetLQueue());

    unsigned aff_id = 0;
    TNSBitVector aff_jobs;
    if (affinity_token && *affinity_token) {
        aff_id = q->affinity_dict.GetTokenId(affinity_token);
        if (!aff_id) {
            return false;
        }
        // read affinity vector
        {{
            CFastMutexGuard guard(q->lock);
            x_ReadAffIdx_NoLock(aff_id, &aff_jobs);
        }}
    }

    q->status_tracker.CountStatus(status_map, aff_id!=0 ? &aff_jobs : 0); 

    return true;
}


bool CQueue::CheckDelete(unsigned int job_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    SQueueDB& db = q->db;

    time_t curr = time(0);
    unsigned time_done, time_submit;
    int job_ttl;

    {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(0);
        
        db.id = job_id;

        if (db.Fetch() != eBDB_Ok) {
            // ? already deleted
            LOG_POST(Warning << "Attempt to delete non-existent job");
            return true;
        }

        int status = db.status;

        unsigned queue_ttl = q->timeout;
        job_ttl = db.timeout;
        if (job_ttl <= 0) {
            job_ttl = queue_ttl;
        }


        time_submit = db.time_submit;
        time_done = db.time_done;

        // pending jobs expire just as done jobs
        if (time_done == 0 && status == (int) CNetScheduleAPI::ePending) {
            time_done = time_submit;
        }

        if (time_done == 0) { 
            if (time_submit + (job_ttl * 10) > (unsigned)curr) {
                return false;
            }
        } else {
            if (time_done + job_ttl > (unsigned)curr) {
                return false;
            }
        }
    }}

    q->Erase(job_id);

    return true;
}

void CQueue::x_DeleteDBRec(SQueueDB&  db, CBDB_FileCursor& cur)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    // dump the record
    {{
        CFastMutexGuard dump_guard(q->rec_dump_lock);
        if (q->rec_dump_flag) {
            x_PrintJobDbStat(db,
                             q->rec_dump,
                             ";",               // field separator
                             false);            // no field names
        }
    }}

    cur.Delete(CBDB_File::eIgnoreError);
}


unsigned 
CQueue::CheckDeleteBatch(unsigned batch_size,
                         CNetScheduleAPI::EJobStatus status)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned dcnt;
    for (dcnt = 0; dcnt < batch_size; ++dcnt) {
        unsigned job_id = q->status_tracker.GetFirst(status);
        if (job_id == 0)
            break;
        bool deleted = CheckDelete(job_id);
        if (!deleted)
            break;
    }
    // TODO: flush jobs-to-delete vector into db
    return dcnt;
}


unsigned
CQueue::DoDeleteBatch(unsigned batch_size)
{

    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned del_rec = q->DeleteBatch(batch_size);
    // monitor this
    if (del_rec > 0 && q->monitor.IsMonitorActive()) {
        CTime tm(CTime::eCurrent);
        string msg = tm.AsString();
        msg += " CQueue::DeleteBatch: " +
                NStr::IntToString(del_rec) + " job(s) deleted";
        q->monitor.SendString(msg);
    }
    return del_rec;
}


void CQueue::ClearAffinityIdx()
{
    // read the queue database, find the first physically available job_id,
    // then scan all index records, delete all the old (obsolete) record 
    // (job_id) references

    CRef<SLockedQueue> q(x_GetLQueue());
    SQueueDB& db = q->db;

    unsigned first_job_id = 0;
    {{
        CFastMutexGuard guard(q->lock);
        CBDB_Transaction trans(*db.GetEnv(), 
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);

        db.SetTransaction(&trans);

        // get the first phisically available record in the queue database

        CBDB_FileCursor& cur = *q->GetCursor(trans);
        CBDB_CursorGuard cg(cur);
        cur.SetCondition(CBDB_FileCursor::eGE);
        cur.From << 0;

        if (cur.Fetch() == eBDB_Ok) {
            first_job_id = db.id;
        }
    }}

    if (first_job_id <= 1) {
        return;
    }


    TNSBitVector bv(bm::BM_GAP);

    // get list of all affinity tokens in the index
    {{
        CFastMutexGuard guard(q->lock);
        CBDB_Transaction trans(*db.GetEnv(), 
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);
        q->aff_idx.SetTransaction(&trans);
        CBDB_FileCursor cur(q->aff_idx);
        cur.SetCondition(CBDB_FileCursor::eGE);
        while (cur.Fetch() == eBDB_Ok) {
            unsigned aff_id = q->aff_idx.aff_id;
            bv.set(aff_id);
        }
    }}

    // clear all hanging references
    TNSBitVector::enumerator en(bv.first());
    for (; en.valid(); ++en) {
        unsigned aff_id = *en;
        CFastMutexGuard guard(q->lock);
        CBDB_Transaction trans(*db.GetEnv(), 
                                CBDB_Transaction::eTransASync,
                                CBDB_Transaction::eNoAssociation);
        q->aff_idx.SetTransaction(&trans);
        SQueueAffinityIdx::TParent::TBitVector bvect(bm::BM_GAP);
        q->aff_idx.aff_id = aff_id;
        /*EBDB_ErrCode ret = */
            q->aff_idx.ReadVector(&bvect, bm::set_OR, NULL);
        unsigned old_count = bvect.count();
        bvect.set_range(0, first_job_id-1, false);
        unsigned new_count = bvect.count();
        if (new_count == old_count) {
            continue;
        }
        bvect.optimize();
        q->aff_idx.aff_id = aff_id;
        if (bvect.any()) {
            q->aff_idx.WriteVector(bvect, SQueueAffinityIdx::eNoCompact);
        } else {
            q->aff_idx.Delete();
        }
        trans.Commit();
    } // for
}


void CQueue::Truncate(void)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    {{
        CFastMutexGuard db_guard(q->lock);
        CFastMutexGuard jtd_guard(q->m_JobsToDeleteLock);
        CWriteLockGuard rtl_guard(q->rtl_lock);
        q->status_tracker.ClearAll(&q->m_JobsToDelete);
        q->run_time_line->ReInit(0);
    }}
    // To update 'became_empty' timestamp
    IsExpired(); // locks q->lock
}


void CQueue::x_AddToTimeLine(unsigned job_id, time_t curr)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    if (job_id && q->run_time_line) {
        CJobTimeLine& tl = *q->run_time_line;
        time_t projected_time_done = curr + q->run_timeout;

        CWriteLockGuard guard(q->rtl_lock);
        tl.AddObject(projected_time_done, job_id);
    }
}


void CQueue::x_RemoveFromTimeLine(unsigned job_id)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    if (q->run_time_line) {
        CWriteLockGuard guard(q->rtl_lock);
        q->run_time_line->RemoveObject(job_id);
    }

    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::RemoveFromTimeLine: job id=";
        msg += NStr::IntToString(job_id);
        q->monitor.SendString(msg);
    }
}


void 
CQueue::x_TimeLineExchange(unsigned remove_job_id, 
                           unsigned add_job_id,
                           time_t   curr)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    if (q->run_time_line) {
        CWriteLockGuard guard(q->rtl_lock);
        q->run_time_line->RemoveObject(remove_job_id);

        if (add_job_id) {
            CJobTimeLine& tl = *q->run_time_line;
            time_t projected_time_done = curr + q->run_timeout;
            tl.AddObject(projected_time_done, add_job_id);
        }
    }
    if (q->monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::TimeLineExchange: job removed=";
        msg += NStr::IntToString(remove_job_id);
        msg += " job added=";
        msg += NStr::IntToString(add_job_id);
        q->monitor.SendString(msg);
    }
}

SLockedQueue::TListenerList::iterator
CQueue::x_FindListener(unsigned int    host_addr, 
                       unsigned short  udp_port)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    SLockedQueue::TListenerList& wnodes = q->wnodes;
    return find_if(wnodes.begin(), wnodes.end(),
        CQueueComparator(host_addr, udp_port));
}


void CQueue::RegisterNotificationListener(unsigned int    host_addr,
                                          unsigned short  udp_port,
                                          int             timeout,
                                          const string&   auth)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CWriteLockGuard guard(q->wn_lock);
    SLockedQueue::TListenerList::iterator it =
                                x_FindListener(host_addr, udp_port);
    
    time_t curr = time(0);
    if (it != q->wnodes.end()) {  // update registration timestamp
        SQueueListener& ql = *(*it);
        if ((ql.timeout = timeout) != 0) {
            ql.last_connect = curr;
            ql.auth = auth;
        }
        return;
    }

    // new element
    if (timeout) {
        q->wnodes.push_back(
            new SQueueListener(host_addr, udp_port, curr, timeout, auth));
    }
}

void 
CQueue::UnRegisterNotificationListener(unsigned int    host_addr,
                                       unsigned short  udp_port)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CWriteLockGuard guard(q->wn_lock);
    SLockedQueue::TListenerList::iterator it =
                                x_FindListener(host_addr, udp_port);
    if (it != q->wnodes.end()) {
        SQueueListener* node = *it;
        delete node;
        q->wnodes.erase(it);
    }
}

void CQueue::ClearAffinity(unsigned int  host_addr,
                           const string& auth)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    CFastMutexGuard guard(q->aff_map_lock);
    q->worker_aff_map.ClearAffinity(host_addr, auth);
}

void CQueue::SetMonitorSocket(SOCK sock)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    auto_ptr<CSocket> s(new CSocket());
    s->Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);
    q->monitor.SetSocket(s.get());
    s.release();
}

CNetScheduleMonitor* CQueue::GetMonitor(void)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    return &q->monitor;
}


void CQueue::NotifyListeners(bool unconditional)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    unsigned short udp_port = m_Db.GetUdpPort();
    if (udp_port == 0) {
        return;
    }

    SLockedQueue::TListenerList& wnodes = q->wnodes;

    time_t curr = time(0);

    if (!unconditional &&
        (q->notif_timeout == 0 ||
         !q->status_tracker.AnyPending())) {
        return;
    }

    SLockedQueue::TListenerList::size_type lst_size;
    {{
        CWriteLockGuard guard(q->wn_lock);
        lst_size = wnodes.size();
        if ((lst_size == 0) ||
            (!unconditional && q->last_notif + q->notif_timeout > curr)) {
            return;
        } 
        q->last_notif = curr;
    }}

    const char* msg = q->q_notif.c_str();
    size_t msg_len = q->q_notif.length()+1;
    
    for (SLockedQueue::TListenerList::size_type i = 0; 
         i < lst_size; 
         ++i) 
    {
        unsigned host;
        unsigned short port;
        {{
            CReadLockGuard guard(q->wn_lock);
            SQueueListener* ql = wnodes[i];
            if (ql->last_connect + ql->timeout < curr)
                continue;
            host = ql->host;
            port = ql->udp_port;
        }}

        if (port) {
            CFastMutexGuard guard(q->us_lock);
            //EIO_Status status = 
                q->udp_socket.Send(msg, msg_len, 
                                   CSocketAPI::ntoa(host), port);
        }
        // periodically check if we have no more jobs left
        if ((i % 10 == 0) && !q->status_tracker.AnyPending())
            break;
    }
}


void CQueue::CheckExecutionTimeout()
{
    CRef<SLockedQueue> q(x_GetLQueue());
    if (q->run_time_line == 0) {
        return;
    }
    CJobTimeLine& tl = *q->run_time_line;
    time_t curr = time(0);
    unsigned curr_slot;
    {{
        CReadLockGuard guard(q->rtl_lock);
        curr_slot = tl.TimeLineSlot(curr);
    }}
    if (curr_slot == 0) {
        return;
    }
    --curr_slot;

    TNSBitVector bv;
    {{
        CReadLockGuard guard(q->rtl_lock);
        tl.EnumerateObjects(&bv, curr_slot);
    }}
    TNSBitVector::enumerator en(bv.first());
    for ( ;en.valid(); ++en) {
        unsigned job_id = *en;
        unsigned exp_time = CheckExecutionTimeout(job_id, curr);

        // job may need to be moved in the timeline to some future slot
        
        if (exp_time) {
            CWriteLockGuard guard(q->rtl_lock);
            unsigned job_slot = tl.TimeLineSlot(exp_time);
            while (job_slot <= curr_slot) {
                ++job_slot;
            }
            tl.AddObjectToSlot(job_slot, job_id);
        }
    } // for

    CWriteLockGuard guard(q->rtl_lock);
    tl.HeadTruncate(curr_slot);
}

time_t CQueue::CheckExecutionTimeout(unsigned job_id, time_t curr_time)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    SQueueDB& db = q->db;

    CNetScheduleAPI::EJobStatus status = GetStatus(job_id);

    if (status != CNetScheduleAPI::eRunning) {
        return 0;
    }

    CBDB_Transaction trans(*db.GetEnv(), 
                        CBDB_Transaction::eTransASync,
                        CBDB_Transaction::eNoAssociation);

    // TODO: get current job status from the status index
    unsigned time_run, run_timeout;
    time_t   exp_time;
    {{
        CFastMutexGuard guard(q->lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *q->GetCursor(trans);
        CBDB_CursorGuard cg(cur);

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << job_id;
        if (cur.Fetch() != eBDB_Ok) {
            return 0;
        }
        int status = db.status;
        if (status != (int) CNetScheduleAPI::eRunning) {
            return 0;
        }

        time_run = db.time_run;
        _ASSERT(time_run);
        run_timeout = db.run_timeout;

        exp_time = x_ComputeExpirationTime(time_run, run_timeout);
        if (curr_time < exp_time) { 
            return exp_time;
        }
        db.status = (int) CNetScheduleAPI::ePending;
        db.time_done = 0;

        cur.Update();
    }}

    trans.Commit();

    q->status_tracker.SetStatus(job_id, CNetScheduleAPI::eReturned);

    {{
        CTime tm(CTime::eCurrent);
        string msg = tm.AsString();
        msg += " CQueue::CheckExecutionTimeout: Job rescheduled id=";
        msg += NStr::IntToString(job_id);
        tm.SetTimeT(time_run);
        tm.ToLocalTime();
        msg += " time_run=";
        msg += tm.AsString();

        tm.SetTimeT(exp_time);
        tm.ToLocalTime();
        msg += " exp_time=";
        msg += tm.AsString();

        msg += " run_timeout(sec)=";
        msg += NStr::IntToString(run_timeout);
        msg += " run_timeout(minutes)=";
        msg += NStr::IntToString(run_timeout/60);
        ERR_POST(msg);

        if (q->monitor.IsMonitorActive()) {
            q->monitor.SendString(msg);
        }
    }}

    return 0;
}

time_t 
CQueue::x_ComputeExpirationTime(unsigned time_run, unsigned run_timeout) const
{
    if (run_timeout == 0) {
        run_timeout = x_GetLQueue()->run_timeout;
    }
    if (run_timeout == 0) {
        return 0;
    }
    time_t exp_time = time_run + run_timeout;
    return exp_time;
}

void CQueue::x_AddToAffIdx_NoLock(unsigned aff_id, 
                                  unsigned job_id_from,
                                  unsigned job_id_to)

{
    CRef<SLockedQueue> q(x_GetLQueue());
    SQueueAffinityIdx::TParent::TBitVector bv(bm::BM_GAP);

    // check if vector is in the database

    // read vector from the file
    q->aff_idx.aff_id = aff_id;
    /*EBDB_ErrCode ret = */
    q->aff_idx.ReadVector(&bv, bm::set_OR, NULL);
    if (job_id_to == 0) {
        bv.set(job_id_from);
    } else {
        bv.set_range(job_id_from, job_id_to);
    }
    q->aff_idx.aff_id = aff_id;
    q->aff_idx.WriteVector(bv, SQueueAffinityIdx::eNoCompact);
}

void CQueue::x_AddToAffIdx_NoLock(const vector<SNS_SubmitRecord>& batch)
{
    CRef<SLockedQueue> q(x_GetLQueue());

    typedef SQueueAffinityIdx::TParent::TBitVector TBVector;
    typedef map<unsigned, TBVector*>               TBVMap;
    
    TBVMap  bv_map;
    try {
        unsigned bsize = batch.size();
        for (unsigned i = 0; i < bsize; ++i) {
            const SNS_SubmitRecord& bsub = batch[i];
            unsigned aff_id = bsub.affinity_id;
            unsigned job_id_start = bsub.job_id;

            TBVector* aff_bv;

            TBVMap::iterator aff_it = bv_map.find(aff_id);
            if (aff_it == bv_map.end()) { // new element
                auto_ptr<TBVector> bv(new TBVector(bm::BM_GAP));
                q->aff_idx.aff_id = aff_id;
                /*EBDB_ErrCode ret = */
                q->aff_idx.ReadVector(bv.get(), bm::set_OR, NULL);
                aff_bv = bv.get();
                bv_map[aff_id] = bv.release();
            } else {
                aff_bv = aff_it->second;
            }


            // look ahead for the same affinity id
            unsigned j;
            for (j=i+1; j < bsize; ++j) {
                if (batch[j].affinity_id != aff_id) {
                    break;
                }
                _ASSERT(batch[j].job_id == (batch[j-1].job_id+1));
                //job_id_end = batch[j].job_id;
            }
            --j;

            if ((i!=j) && (aff_id == batch[j].affinity_id)) {
                unsigned job_id_end = batch[j].job_id;
                aff_bv->set_range(job_id_start, job_id_end);
                i = j;
            } else { // look ahead failed
                aff_bv->set(job_id_start);
            }

        } // for

        // save all changes to the database
        NON_CONST_ITERATE(TBVMap, it, bv_map) {
            unsigned aff_id = it->first;
            TBVector* bv = it->second;
            bv->optimize();

            q->aff_idx.aff_id = aff_id;
            q->aff_idx.WriteVector(*bv, SQueueAffinityIdx::eNoCompact);

            delete it->second; it->second = 0;
        }
    } 
    catch (exception& )
    {
        NON_CONST_ITERATE(TBVMap, it, bv_map) {
            delete it->second; it->second = 0;
        }
        throw;
    }

}

void 
CQueue::x_ReadAffIdx_NoLock(const TNSBitVector& aff_id_set,
                            TNSBitVector*       job_candidates)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->aff_idx.SetTransaction(0);
    TNSBitVector::enumerator en(aff_id_set.first());
    for (; en.valid(); ++en) {  // for each affinity id
        unsigned aff_id = *en;
        x_ReadAffIdx_NoLock(aff_id, job_candidates);
    }
}


void 
CQueue::x_ReadAffIdx_NoLock(unsigned      aff_id,
                            TNSBitVector* job_candidates)
{
    CRef<SLockedQueue> q(x_GetLQueue());
    q->aff_idx.aff_id = aff_id;
    q->aff_idx.SetTransaction(0);
    q->aff_idx.ReadVector(job_candidates, bm::set_OR, NULL);
}

CRef<SLockedQueue> CQueue::x_GetLQueue(void)
{
    CRef<SLockedQueue> ref(m_LQueue.Lock());
    if (ref != NULL) {
        return ref;
    } else {
        NCBI_THROW(CNetScheduleException, eUnknownQueue, "Job queue deleted");
    }
}

const CRef<SLockedQueue> CQueue::x_GetLQueue(void) const
{
    const CRef<SLockedQueue> ref(m_LQueue.Lock());
    if (ref != NULL) {
        return ref;
    } else {
        NCBI_THROW(CNetScheduleException, eUnknownQueue, "Job queue deleted");
    }
}



END_NCBI_SCOPE
