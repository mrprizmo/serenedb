////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "descriptions.h"

#include <vpack/builder.h>

#include "app/app_server.h"
#include "basics/physical_memory.h"
#include "basics/process-utils.h"
#include "general_server/scheduler.h"
#include "general_server/scheduler_feature.h"
#include "metrics/counter.h"
#include "metrics/metrics_feature.h"
#include "statistics/connection_statistics.h"
#include "statistics/request_statistics.h"
#include "statistics/server_statistics.h"

using namespace sdb;

std::string stats::FromGroupType(stats::GroupType gt) {
  switch (gt) {
    case stats::GroupType::System:
      return "system";
    case stats::GroupType::Client:
      return "client";
    case stats::GroupType::ClientUser:
      return "clientUser";
    case stats::GroupType::Http:
      return "http";
    case stats::GroupType::Server:
      return "server";
  }
  SDB_ASSERT(false);
  SDB_THROW(ERROR_BAD_PARAMETER);
}

void stats::Group::toVPack(vpack::Builder& b) const {
  b.add("group", stats::FromGroupType(type));
  b.add("name", name);
  b.add("description", description);
}

std::string stats::FromFigureType(stats::FigureType t) {
  switch (t) {
    case stats::FigureType::Current:
      return "current";
    case stats::FigureType::Accumulated:
      return "accumulated";
    case stats::FigureType::Distribution:
      return "distribution";
  }
  SDB_ASSERT(false);
  SDB_THROW(ERROR_BAD_PARAMETER);
}

std::string stats::FromUnit(stats::Unit u) {
  switch (u) {
    case stats::Unit::Seconds:
      return "seconds";
    case stats::Unit::Bytes:
      return "bytes";
    case stats::Unit::Percent:
      return "percent";
    case stats::Unit::Number:
      return "number";
  }
  SDB_ASSERT(false);
  SDB_THROW(ERROR_BAD_PARAMETER);
}

void stats::Figure::toVPack(vpack::Builder& b) const {
  b.add("group", stats::FromGroupType(group_type));
  b.add("identifier", identifier);
  b.add("name", name);
  b.add("description", description);
  b.add("type", stats::FromFigureType(type));
  if (type == stats::FigureType::Distribution) {
    SDB_ASSERT(!cuts.empty());
    b.add("cuts", vpack::Value(vpack::ValueType::Array, true));
    for (double cut : cuts) {
      b.add(cut);
    }
    b.close();
  }
  b.add("units", stats::FromUnit(units));
}

stats::Descriptions::Descriptions(SerenedServer& server)
  : _server(server),
    _request_time_cuts(statistics::kRequestTimeDistributionCuts),
    _connection_time_cuts(statistics::kConnectionTimeDistributionCuts),
    _bytes_send_cuts(statistics::kBytesSentDistributionCuts),
    _bytes_received_cuts(statistics::kBytesReceivedDistributionCuts) {
  _groups.emplace_back(Group{stats::GroupType::System, "Process Statistics",
                             "Statistics about the SereneDB process"});
  _groups.emplace_back(Group{stats::GroupType::Client,
                             "Client Connection Statistics",
                             "Statistics about the connections."});
  _groups.emplace_back(Group{stats::GroupType::ClientUser,
                             "Client User Connection Statistics",
                             "Statistics about the connections, only user "
                             "traffic (ignoring superuser JWT traffic)."});
  _groups.emplace_back(Group{stats::GroupType::Http, "HTTP Request Statistics",
                             "Statistics about the HTTP requests."});
  _groups.emplace_back(Group{stats::GroupType::Server, "Server Statistics",
                             "Statistics about the SereneDB server"});

  _figures.emplace_back(Figure{stats::GroupType::System,
                               "userTime",
                               "User Time",
                               "Amount of time that this process has been "
                               "scheduled in user mode, measured in seconds.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Seconds,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::System,
                               "systemTime",
                               "System Time",
                               "Amount of time that this process has been "
                               "scheduled in kernel mode, measured in seconds.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Seconds,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::System,
                               "numberOfThreads",
                               "Number of Threads",
                               "Number of threads in the serened process.",
                               stats::FigureType::Current,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{
    stats::GroupType::System,
    "residentSize",
    "Resident Set Size",
    "The total size of the number of pages the process has in real memory. "
    "This is just the pages which count toward text, data, or stack space. "
    "This does not include pages which have not been demand-loaded in, or "
    "which are swapped out. The resident set size is reported in bytes.",
    stats::FigureType::Current,
    stats::Unit::Bytes,
    {}});

  _figures.emplace_back(Figure{stats::GroupType::System,
                               "residentSizePercent",
                               "Resident Set Size",
                               "The percentage of physical memory used by the "
                               "process as resident set size.",
                               stats::FigureType::Current,
                               stats::Unit::Percent,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::System,
                               "virtualSize",
                               "Virtual Memory Size",
                               "This figure contains The size of the "
                               "virtual memory the process is using.",
                               stats::FigureType::Current,
                               stats::Unit::Bytes,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::System,
                               "minorPageFaults",
                               "Minor Page Faults",
                               "The number of minor faults the process has "
                               "made which have not required loading a memory "
                               "page from disk.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{
    stats::GroupType::System,
    "majorPageFaults",
    "Major Page Faults",
    "This figure contains the number of major faults the "
    "process has made which have required loading a memory page from disk.",
    stats::FigureType::Accumulated,
    stats::Unit::Number,
    {}});

  // .............................................................................
  // client statistics
  // .............................................................................

  _figures.emplace_back(
    Figure{stats::GroupType::Client,
           "httpConnections",
           "Client Connections",
           "The number of connections that are currently open.",
           stats::FigureType::Current,
           stats::Unit::Number,
           {}});

  _figures.emplace_back(Figure{
    stats::GroupType::Client, "totalTime", "Total Time",
    "Total time needed to answer a request.", stats::FigureType::Distribution,
    // cuts: internal.requestTimeDistribution,
    stats::Unit::Seconds, _request_time_cuts});

  _figures.emplace_back(Figure{
    stats::GroupType::Client, "requestTime", "Request Time",
    "Request time needed to answer a request.", stats::FigureType::Distribution,
    // cuts: internal.requestTimeDistribution,
    stats::Unit::Seconds, _request_time_cuts});

  _figures.emplace_back(Figure{
    stats::GroupType::Client, "queueTime", "Queue Time",
    "Queue time needed to answer a request.", stats::FigureType::Distribution,
    // cuts: internal.requestTimeDistribution,
    stats::Unit::Seconds, _request_time_cuts});

  _figures.emplace_back(Figure{stats::GroupType::Client, "bytesSent",
                               "Bytes Sent", "Bytes sents for a request.",
                               stats::FigureType::Distribution,
                               // cuts: internal.bytesSentDistribution,
                               stats::Unit::Bytes, _bytes_send_cuts});

  _figures.emplace_back(
    Figure{stats::GroupType::Client, "bytesReceived", "Bytes Received",
           "Bytes received for a request.", stats::FigureType::Distribution,
           // cuts: internal.bytesReceivedDistribution,
           stats::Unit::Bytes, _bytes_received_cuts});

  _figures.emplace_back(Figure{
    stats::GroupType::Client, "connectionTime", "Connection Time",
    "Total connection time of a client.", stats::FigureType::Distribution,
    // cuts: internal.connectionTimeDistribution,
    stats::Unit::Seconds, _connection_time_cuts});

  // Only user traffic:

  _figures.emplace_back(Figure{
    stats::GroupType::ClientUser,
    "httpConnections",
    "Client Connections",
    "The number of connections that are currently open (only user traffic).",
    stats::FigureType::Current,
    stats::Unit::Number,
    {}});

  _figures.emplace_back(
    Figure{stats::GroupType::ClientUser, "totalTime", "Total Time",
           "Total time needed to answer a request (only user traffic).",
           stats::FigureType::Distribution,
           // cuts: internal.requestTimeDistribution,
           stats::Unit::Seconds, _request_time_cuts});

  _figures.emplace_back(
    Figure{stats::GroupType::ClientUser, "requestTime", "Request Time",
           "Request time needed to answer a request (only user traffic).",
           stats::FigureType::Distribution,
           // cuts: internal.requestTimeDistribution,
           stats::Unit::Seconds, _request_time_cuts});

  _figures.emplace_back(
    Figure{stats::GroupType::ClientUser, "queueTime", "Queue Time",
           "Queue time needed to answer a request (only user traffic).",
           stats::FigureType::Distribution,
           // cuts: internal.requestTimeDistribution,
           stats::Unit::Seconds, _request_time_cuts});

  _figures.emplace_back(Figure{stats::GroupType::ClientUser, "bytesSent",
                               "Bytes Sent",
                               "Bytes sents for a request (only user traffic).",
                               stats::FigureType::Distribution,
                               // cuts: internal.bytesSentDistribution,
                               stats::Unit::Bytes, _bytes_send_cuts});

  _figures.emplace_back(
    Figure{stats::GroupType::ClientUser, "bytesReceived", "Bytes Received",
           "Bytes received for a request (only user traffic).",
           stats::FigureType::Distribution,
           // cuts: internal.bytesReceivedDistribution,
           stats::Unit::Bytes, _bytes_received_cuts});

  _figures.emplace_back(
    Figure{stats::GroupType::ClientUser, "connectionTime", "Connection Time",
           "Total connection time of a client (only user traffic).",
           stats::FigureType::Distribution,
           // cuts: internal.connectionTimeDistribution,
           stats::Unit::Seconds, _connection_time_cuts});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsTotal",
                               "Total requests",
                               "Total number of HTTP requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(
    Figure{stats::GroupType::Http,
           "requestsSuperuser",
           "Total superuser requests",
           "Total number of HTTP requests executed by superuser/JWT.",
           stats::FigureType::Accumulated,
           stats::Unit::Number,
           {}});

  _figures.emplace_back(
    Figure{stats::GroupType::Http,
           "requestsUser",
           "Total user requests",
           "Total number of HTTP requests executed by clients.",
           stats::FigureType::Accumulated,
           stats::Unit::Number,
           {}});

  _figures.emplace_back(
    Figure{stats::GroupType::Http,
           "requestsAsync",
           "Async requests",
           "Number of asynchronously executed HTTP requests.",
           stats::FigureType::Accumulated,
           stats::Unit::Number,
           {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsGet",
                               "HTTP GET requests",
                               "Number of HTTP GET requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsHead",
                               "HTTP HEAD requests",
                               "Number of HTTP HEAD requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsPost",
                               "HTTP POST requests",
                               "Number of HTTP POST requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsPut",
                               "HTTP PUT requests",
                               "Number of HTTP PUT requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsPatch",
                               "HTTP PATCH requests",
                               "Number of HTTP PATCH requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsDelete",
                               "HTTP DELETE requests",
                               "Number of HTTP DELETE requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsOptions",
                               "HTTP OPTIONS requests",
                               "Number of HTTP OPTIONS requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Http,
                               "requestsOther",
                               "other HTTP requests",
                               "Number of other HTTP requests.",
                               stats::FigureType::Accumulated,
                               stats::Unit::Number,
                               {}});

  // .............................................................................
  // server statistics
  // .............................................................................

  _figures.emplace_back(Figure{stats::GroupType::Server,
                               "uptime",
                               "Server Uptime",
                               "Number of seconds elapsed since server start.",
                               stats::FigureType::Current,
                               stats::Unit::Seconds,
                               {}});

  _figures.emplace_back(Figure{stats::GroupType::Server,
                               "physicalMemory",
                               "Physical Memory",
                               "Physical memory in bytes.",
                               stats::FigureType::Current,
                               stats::Unit::Bytes,
                               {}});
}

void stats::Descriptions::serverStatistics(vpack::Builder& b) const {
  const ServerStatistics& info =
    _server.getFeature<metrics::MetricsFeature>().serverStatistics();
  b.add("uptime", info.uptime());
  b.add("physicalMemory", physical_memory::GetValue());

  b.add("transactions", vpack::Value(vpack::ValueType::Object));
  b.add("started", info.transactions_statistics.transactions_started.load());
  b.add("aborted", info.transactions_statistics.transactions_aborted.load());
  b.add("committed",
        info.transactions_statistics.transactions_committed.load());
  b.add("intermediateCommits",
        info.transactions_statistics.intermediate_commits.load());
  b.add("readOnly", info.transactions_statistics.read_transactions.load());
  b.add("dirtyReadOnly",
        info.transactions_statistics.dirty_read_transactions.load());
  b.close();
}

////////////////////////////////////////////////////////////////////////////////
/// fills the distribution
////////////////////////////////////////////////////////////////////////////////

static void FillDistribution(vpack::Builder& b, const std::string& name,
                             const statistics::Distribution& dist) {
  b.add(name, vpack::Value(vpack::ValueType::Object, true));
  b.add("sum", dist.total);
  b.add("count", dist.count);
  b.add("counts", vpack::Value(vpack::ValueType::Array, true));
  for (const auto& it : dist.counts) {
    b.add(it);
  }
  b.close();
  b.close();
}

void stats::Descriptions::clientStatistics(
  vpack::Builder& b, RequestStatisticsSource source) const {
  // FIXME why are httpConnections in here ?
  ConnectionStatistics::Snapshot connection_stats;
  ConnectionStatistics::getSnapshot(connection_stats);

  b.add("httpConnections", connection_stats.http_connections.get());
  FillDistribution(b, "connectionTime", connection_stats.connection_time);

  RequestStatistics::Snapshot request_stats;
  RequestStatistics::getSnapshot(request_stats, source);

  FillDistribution(b, "totalTime", request_stats.total_time);
  FillDistribution(b, "requestTime", request_stats.request_time);
  FillDistribution(b, "queueTime", request_stats.queue_time);
  FillDistribution(b, "ioTime", request_stats.io_time);
  FillDistribution(b, "bytesSent", request_stats.bytes_sent);
  FillDistribution(b, "bytesReceived", request_stats.bytes_received);
}

void stats::Descriptions::httpStatistics(vpack::Builder& b) const {
  ConnectionStatistics::Snapshot stats;
  ConnectionStatistics::getSnapshot(stats);

  // request counters
  b.add("requestsTotal", stats.total_requests.get());
  b.add("requestsSuperuser", stats.total_requests_superuser.get());
  b.add("requestsUser", stats.total_requests_user.get());
  b.add("requestsAsync", stats.async_requests.get());
  b.add("requestsGet",
        stats.method_requests[(int)rest::RequestType::Get].get());
  b.add("requestsHead",
        stats.method_requests[(int)rest::RequestType::Head].get());
  b.add("requestsPost",
        stats.method_requests[(int)rest::RequestType::Post].get());
  b.add("requestsPut",
        stats.method_requests[(int)rest::RequestType::Put].get());
  b.add("requestsPatch",
        stats.method_requests[(int)rest::RequestType::Patch].get());
  b.add("requestsDelete",
        stats.method_requests[(int)rest::RequestType::DeleteReq].get());
  b.add("requestsOptions",
        stats.method_requests[(int)rest::RequestType::Options].get());
  b.add("requestsOther",
        stats.method_requests[(int)rest::RequestType::Illegal].get());
}

void stats::Descriptions::processStatistics(vpack::Builder& b) const {
  ProcessInfo info = GetProcessInfoSelf();
  double rss = (double)info.resident_size;
  double rssp = 0;

  if (physical_memory::GetValue() != 0) {
    rssp = rss / physical_memory::GetValue();
  }

  b.add("minorPageFaults", info.minor_page_faults);
  b.add("majorPageFaults", info.major_page_faults);
  b.add("userTime", (double)info.user_time / (double)info.sc_clk_tck);
  b.add("systemTime", (double)info.system_time / (double)info.sc_clk_tck);
  b.add("numberOfThreads", info.number_threads);
  b.add("residentSize", rss);
  b.add("residentSizePercent", rssp);
  b.add("virtualSize", info.virtual_size);
}
