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
 * File Description: Network scheduler daemon
 *
 */

#include <ncbi_pch.hpp>
#include <stdio.h>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbidiag.hpp>

#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp> // FIXME: remove

#include <util/bitset/ncbi_bitset.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/server.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <db/bdb/bdb_expt.hpp>

#include "queue_coll.hpp"
#include "ns_types.hpp"
#include "ns_util.hpp"
#include "ns_server_params.hpp"
#include "ns_handler.hpp"
#include "ns_server.hpp"
#include "netschedule_version.hpp"

#if defined(NCBI_OS_UNIX)
# include <corelib/ncbi_process.hpp>
# include <signal.h>
# include <unistd.h>
#endif


USING_NCBI_SCOPE;


/// @internal
extern "C" void Threaded_Server_SignalHandler(int signum)
{
    CNetScheduleServer* server = CNetScheduleServer::GetInstance();

    if (server &&
        (!server->ShutdownRequested()) ) {
        server->SetShutdownFlag(signum);
    }
}


/// NetSchedule daemon application
///
/// @internal
///
class CNetScheduleDApp : public CNcbiApplication
{
public:
    CNetScheduleDApp()
        : CNcbiApplication()
    {}
    void Init(void);
    int Run(void);
private:
    STimeout m_ServerAcceptTimeout;
};


void CNetScheduleDApp::Init(void)
{
    SetDiagPostFlag(eDPF_DateTime);

    // Convert multi-line diagnostic messages into one-line ones by default.
    SetDiagPostFlag(eDPF_PreMergeLines);
    SetDiagPostFlag(eDPF_MergeLines);


    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "netscheduled");

    arg_desc->AddFlag("reinit",       "Recreate the storage directory.");
    arg_desc->AddFlag("version-full", "Version");

    SetupArgDescriptions(arg_desc.release());
    //CONNECT_Init(&GetConfig());
    SOCK_InitializeAPI();
}


int CNetScheduleDApp::Run(void)
{
    const CArgs& args = GetArgs();

    if (args["version-full"]) {
        printf(NETSCHEDULED_FULL_VERSION "\n");
        return 0;
    }

    LOG_POST(NETSCHEDULED_FULL_VERSION);

    const CNcbiRegistry& reg = GetConfig();

    try {
        #if defined(NCBI_OS_UNIX)
            // attempt to get server gracefully shutdown on signal
            signal( SIGINT, Threaded_Server_SignalHandler);
            signal( SIGTERM, Threaded_Server_SignalHandler);
        #endif

        // [bdb] section
        SNSDBEnvironmentParams bdb_params;

        if (!bdb_params.Read(reg, "bdb")) {
            ERR_POST("Failed to read BDB initialization section");
            return 1;
        }

        {{
            string str_params;
            unsigned nParams = bdb_params.GetNumParams();
            for (unsigned n = 0; n < nParams; ++n) {
                if (n > 0) str_params += ';';
                str_params += bdb_params.GetParamName(n) + '=' +
                              bdb_params.GetParamValue(n);
            }
            LOG_POST(Info << "Effective [bdb] parameters: " << str_params);
        }}

        // [server] section
        SNS_Parameters params;
        params.Read(reg, "server");

        {{
            string str_params;
            unsigned nParams = params.GetNumParams();
            for (unsigned n = 0; n < nParams; ++n) {
                if (n > 0) str_params += ';';
                str_params += params.GetParamName(n) + '=' +
                              params.GetParamValue(n);
            }
            LOG_POST(Info << "Effective [server] parameters: " << str_params);
        }}

        bool reinit = params.reinit || args["reinit"];

        m_ServerAcceptTimeout.sec = 1;
        m_ServerAcceptTimeout.usec = 0;
        params.accept_timeout  = &m_ServerAcceptTimeout;

        auto_ptr<CNetScheduleServer>    server(new CNetScheduleServer());

        server->SetNSParameters(params);

        // Use port passed through parameters
        server->AddDefaultListener(new CNetScheduleConnectionFactory(&*server));

        server->StartListening();

        NcbiCout << "Server listening on port " << params.port << NcbiEndl;
        LOG_POST(Info << "Server listening on port " << params.port);

        // two transactions per thread should be enough
        bdb_params.max_trans = params.max_threads * 2;

        auto_ptr<CQueueDataBase> qdb(
            new CQueueDataBase(server->GetBackgroundHost(),
                               server->GetRequestExecutor()));

        NcbiCout << "Mounting database at " << bdb_params.db_path << NcbiEndl;
        LOG_POST(Info << "Mounting database at " << bdb_params.db_path);

        {{
            #if defined(NCBI_OS_UNIX)
                // To be able to open pollable sockets we need to allocate
                // all of file descriptors < 1024 before opening the database.
                int fds[1024];
                int fd = 0;
                int n = 0;
                do {
                    fd = dup(0);
                    if (fd < 0) break;
                    fds[n++] = fd;
                } while (n < 1024  &&  fd < 1024);
                if (fd < 0) {
                    for (int i = 0; i < n; ++i) close(fds[i]);
                    ERR_POST("Too few file descriptors, use \"ulimit -n\""
                             " to expand the number");
                    return 1;
                }
            #endif

            #if defined(NCBI_OS_UNIX)
                bool res = qdb->Open(bdb_params, reinit);
            #endif

            for (int i = 0; i < n; ++i) close(fds[i]);

            if (!res) return 1;
        }}

        if (params.udp_port > 0) {
            qdb->SetUdpPort(params.udp_port);
        }

        #if defined(NCBI_OS_UNIX)
            if (params.is_daemon) {
                LOG_POST("Entering UNIX daemon mode...");
                bool daemon = CProcess::Daemonize(0, CProcess::fDontChroot);
                if (!daemon) {
                    return 0;
                }
            }
        #else
            params.is_daemon = false;
        #endif

        // [queue_*], [qclass_*] and [queues] sections
        // Scan and mount queues
        unsigned min_run_timeout = qdb->Configure(reg);

        min_run_timeout = min_run_timeout > 0 ? min_run_timeout : 2;
        LOG_POST(Info << "Running execution control every "
                      << min_run_timeout << " seconds");


        qdb->RunExecutionWatcherThread(min_run_timeout);
        qdb->RunPurgeThread();
        qdb->RunNotifThread();

        server->SetQueueDB(qdb.release());

        if (!params.is_daemon)
            NcbiCout << "Server started" << NcbiEndl;
        LOG_POST("Server started");

        server->Run();

        if (!params.is_daemon)
            NcbiCout << "Server stopped" << NcbiEndl;
        LOG_POST("Server stopped");

    }
    catch (CBDB_ErrnoException& ex)
    {
        LOG_POST(Error << "Error: DBD errno exception:" << ex.what());
        return ex.BDB_GetErrno();
    }
    catch (CBDB_LibException& ex)
    {
        LOG_POST(Error << "Error: DBD library exception:" << ex.what());
        return 1;
    }
    catch (exception& ex)
    {
        LOG_POST(Error << "Error: STD exception:" << ex.what());
        return 1;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    g_Diag_Use_RWLock();
    CDiagContext::SetOldPostFormat(false);
    CRequestContext::SetDefaultAutoIncRequestIDOnPost(true);
    // Main thread request context already created, so is not affected
    // by just set default, so set it manually.
    CDiagContext::GetRequestContext().SetAutoIncRequestIDOnPost(true);
    return CNetScheduleDApp().AppMain(argc, argv, NULL, eDS_ToStdlog);
}

