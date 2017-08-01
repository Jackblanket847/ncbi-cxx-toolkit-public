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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: Worker node automation implementation.
 *
 */

#include <ncbi_pch.hpp>

#include "wn_automation.hpp"

USING_NCBI_SCOPE;

using namespace NAutomation;

const string SWorkerNode::kName = "wn";

SWorkerNode::SWorkerNode(
        CAutomationProc* automation_proc, CNetScheduleAPI ns_api) :
    SNetService(automation_proc, CNetService::eSingleServerService),
    m_NetScheduleAPI(ns_api)
{
    m_Service = m_NetScheduleAPI.GetService();

    m_WorkerNode = m_Service.Iterate().GetServer();

    if (m_Service.GetServiceType() != CNetService::eSingleServerService) {
        NCBI_THROW(CAutomationException, eCommandProcessingError,
                "WorkerNode constructor: 'wn_address' "
                "must be a host:port combination");
    }
}

CCommand SWorkerNode::NewCommand()
{
    return CCommand(kName, ExecNew<TSelf>, {
            { "wn_address", CJsonNode::eString, },
            { "client_name", "", },
        });
}

CAutomationObject* SWorkerNode::Create(const TArguments& args, CAutomationProc* automation_proc)
{
    _ASSERT(args.size() == 2);

    const auto wn_address = args[0].Value().AsString();
    const auto client_name  = args[1].Value().AsString();

    CNetScheduleAPIExt ns_api(CNetScheduleAPIExt::CreateWnCompat(
                wn_address, client_name));

    return new SWorkerNode(automation_proc, ns_api);
}

const void* SWorkerNode::GetImplPtr() const
{
    return m_NetScheduleAPI;
}

CCommand SWorkerNode::CallCommand()
{
    return CCommand(kName, CallCommands);
}

TCommands SWorkerNode::CallCommands()
{
    TCommands cmds =
    {
        { "version", },
        { "wn_info", },
        { "suspend", {
                { "pullback_mode", false, },
                { "timeout", 0, },
            }},
        { "resume", },
        { "shutdown", {
                { "level", 0 },
            }},
    };

    TCommands base_cmds = SNetService::CallCommands();
    cmds.insert(cmds.end(), base_cmds.begin(), base_cmds.end());

    return cmds;
}

void SWorkerNode::ExecVersion(const TArguments& args, SInputOutput& io)
{
    auto& reply = io.reply;
    reply.Append(g_ServerInfoToJson(m_Service, m_ActualServiceType, false));
}

void SWorkerNode::ExecWnInfo(const TArguments& args, SInputOutput& io)
{
    auto& reply = io.reply;
    reply.Append(g_WorkerNodeInfoToJson(m_WorkerNode));
}

void SWorkerNode::ExecSuspend(const TArguments& args, SInputOutput& io)
{
    auto& arg_array = io.arg_array;
    auto pullback_mode = arg_array.NextBoolean(false);
    auto timeout = (unsigned int) arg_array.NextInteger(0);
    g_SuspendWorkerNode(m_WorkerNode, pullback_mode, timeout);
}

void SWorkerNode::ExecResume(const TArguments& args, SInputOutput& io)
{
    g_ResumeWorkerNode(m_WorkerNode);
}

CNetScheduleAdmin::EShutdownLevel s_GetWorkerNodeShutdownMode(Int8 level)
{
    enum EWorkerNodeShutdownMode { eNormalShutdown, eShutdownNow, eKillNode };

    switch (level) {
    case eShutdownNow: return CNetScheduleAdmin::eShutdownImmediate;
    case eKillNode:    return CNetScheduleAdmin::eDie;
    default:           return CNetScheduleAdmin::eNormalShutdown;
    }
}

void SWorkerNode::ExecShutdown(const TArguments& args, SInputOutput& io)
{
    auto& arg_array = io.arg_array;
    auto level = arg_array.NextInteger(0);
    m_NetScheduleAPI.GetAdmin().ShutdownServer(s_GetWorkerNodeShutdownMode(level));
}
     
bool SWorkerNode::Call(const string& method, SInputOutput& io)
{
    if (method == "version")
        ExecVersion(TArguments(), io);
    else if (method == "wn_info")
        ExecWnInfo(TArguments(), io);
    else if (method == "suspend") {
        ExecSuspend(TArguments(), io);
    } else if (method == "resume")
        ExecResume(TArguments(), io);
    else if (method == "shutdown")
        ExecShutdown(TArguments(), io);
    else
        return SNetService::Call(method, io);

    return true;
}
