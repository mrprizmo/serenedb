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

#include "daemon_feature.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <chrono>
#include <stdexcept>
#include <thread>

#include "app/app_server.h"
#include "app/options/option.h"
#include "app/options/parameters.h"
#include "app/options/program_options.h"
#include "basics/application-exit.h"
#include "basics/debugging.h"
#include "basics/exceptions.h"
#include "basics/file_result.h"
#include "basics/file_result_string.h"
#include "basics/file_utils.h"
#include "basics/files.h"
#include "basics/logger/appender.h"
#include "basics/logger/logger.h"
#include "basics/operating-system.h"
#include "basics/process-utils.h"
#include "basics/string_utils.h"
#include "basics/system-functions.h"
#include "basics/threads.h"

#ifdef SERENEDB_HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef SERENEDB_HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef SERENEDB_HAVE_UNISTD_H
#include <unistd.h>
#endif

using namespace sdb::app;
using namespace sdb::basics;
using namespace sdb::options;

namespace sdb {

DaemonFeature::DaemonFeature(Server& server) : SerenedFeature{server, name()} {
  setOptional(true);

  working_directory = "/var/tmp";
}

void DaemonFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption(
    "--daemon",
    "Start the server as a daemon (background process). Requires --pid-file "
    "to be set.",
    new BooleanParameter(&daemon),
    sdb::options::MakeFlags(
      sdb::options::Flags::DefaultNoOs, sdb::options::Flags::OsLinux,
      sdb::options::Flags::OsMac, sdb::options::Flags::Uncommon));

  options->addOption(
    "--pid-file",
    "The name of the process ID file to use if the server runs as a daemon.",
    new StringParameter(&pid_file),
    sdb::options::MakeFlags(
      sdb::options::Flags::DefaultNoOs, sdb::options::Flags::OsLinux,
      sdb::options::Flags::OsMac, sdb::options::Flags::Uncommon));

  options->addOption(
    "--working-directory", "The working directory in daemon mode.",
    new StringParameter(&working_directory),
    sdb::options::MakeFlags(
      sdb::options::Flags::DefaultNoOs, sdb::options::Flags::OsLinux,
      sdb::options::Flags::OsMac, sdb::options::Flags::Uncommon));
}

void DaemonFeature::validateOptions(
  std::shared_ptr<ProgramOptions> /*options*/) {
  if (!daemon) {
    return;
  }

  if (pid_file.empty()) {
    SDB_FATAL("xxxxx", sdb::Logger::FIXME, "need --pid-file in --daemon mode");
  }

  // make the pid filename absolute
  std::string current_dir = file_utils::CurrentDirectory().result();
  std::string absolute_file = SdbGetAbsolutePath(pid_file, current_dir);

  if (!absolute_file.empty()) {
    pid_file = absolute_file;
    SDB_DEBUG("xxxxx", sdb::Logger::FIXME, "using absolute pid file '",
              pid_file, "'");
  } else {
    SDB_FATAL("xxxxx", sdb::Logger::FIXME, "cannot determine absolute path");
  }
}

void DaemonFeature::daemonize() {
  if (!daemon) {
    return;
  }

  SDB_INFO("xxxxx", Logger::STARTUP, "starting up in daemon mode");

  checkPidFile();

  int pid = forkProcess();

  // main process
  if (pid != 0) {
    SetProcessTitle("serenedb [daemon]");
    writePidFile(pid);

    int result = waitForChildProcess(pid);

    exit(result);
  }

  // child process
  else {
    SDB_DEBUG("xxxxx", Logger::STARTUP, "daemon mode continuing within child");
  }
}

void DaemonFeature::unprepare() {
  if (!daemon) {
    return;
  }

  // remove pid file
  if (file_utils::Remove(pid_file) != ERROR_OK) {
    SDB_ERROR("xxxxx", sdb::Logger::FIXME, "cannot remove pid file '", pid_file,
              "'");
  }
}

void DaemonFeature::checkPidFile() {
  // check if the pid-file exists
  if (!pid_file.empty()) {
    if (file_utils::IsDirectory(pid_file)) {
      SDB_FATAL("xxxxx", sdb::Logger::FIXME, "pid-file '", pid_file,
                "' is a directory");
    } else if (file_utils::Exists(pid_file) &&
               file_utils::Size(pid_file.c_str()) > 0) {
      SDB_INFO("xxxxx", Logger::STARTUP, "pid-file '", pid_file,
               "' already exists, verifying pid");
      std::string old_pid_s;
      try {
        old_pid_s = sdb::basics::file_utils::Slurp(pid_file);
      } catch (const sdb::basics::Exception& ex) {
        SDB_FATAL("xxxxx", sdb::Logger::FIXME, "Couldn't read PID file '",
                  pid_file, "' - ", ex.what());
      }

      basics::string_utils::TrimInPlace(old_pid_s);

      if (!old_pid_s.empty()) {
        pid_t old_pid;

        try {
          old_pid = std::stol(old_pid_s);
        } catch (const std::invalid_argument&) {
          SDB_FATAL("xxxxx", sdb::Logger::FIXME, "pid-file '", pid_file,
                    "' doesn't contain a number.");
        }
        if (old_pid == 0) {
          SDB_FATAL("xxxxx", sdb::Logger::FIXME, "pid-file '", pid_file,
                    "' is unreadable");
        }

        SDB_DEBUG("xxxxx", Logger::STARTUP, "found old pid: ", old_pid);

        int r = kill(old_pid, 0);

        if (r == 0 || errno == EPERM) {
          SDB_FATAL("xxxxx", sdb::Logger::FIXME, "pid-file '", pid_file,
                    "' exists and process with pid ", old_pid,
                    " is still running, refusing to start twice");
        } else if (errno == ESRCH) {
          SDB_ERROR("xxxxx", Logger::STARTUP, "pid-file '", pid_file,
                    " exists, but no process with pid ", old_pid, " exists");

          if (file_utils::Remove(pid_file) != ERROR_OK) {
            SDB_FATAL("xxxxx", sdb::Logger::FIXME, "pid-file '", pid_file,
                      "' exists, no process with pid ", old_pid,
                      " exists, but pid-file cannot be removed");
          }

          SDB_INFO("xxxxx", Logger::STARTUP, "removed stale pid-file '",
                   pid_file, "'");
        } else {
          SDB_FATAL("xxxxx", sdb::Logger::FIXME, "pid-file '", pid_file,
                    "' exists and kill ", old_pid, " failed");
        }
      }

      // failed to open file
      else {
        SDB_FATAL("xxxxx", sdb::Logger::FIXME, "pid-file '", pid_file,
                  "' exists, but cannot be opened");
      }
    }

    SDB_DEBUG("xxxxx", Logger::STARTUP, "using pid-file '", pid_file, "'");
  }
}

int DaemonFeature::forkProcess() {
  // fork off the parent process
  pid_t pid = fork();

  if (pid < 0) {
    SDB_FATAL("xxxxx", sdb::Logger::FIXME, "cannot fork");
  }

  // Upon successful completion, fork() shall return 0 to the child process and
  // shall return the process ID of the child process to the parent process.

  // if we got a good PID, then we can exit the parent process
  if (pid > 0) {
    SDB_DEBUG("xxxxx", Logger::STARTUP, "started child process with pid ", pid);
    return pid;
  }

  SDB_ASSERT(pid == 0);  // we are in the child

  // child
  log::Appender::allowStdLogging(false);
  log::ClearCachedPid();

  // change the file mode mask
  umask(0);

  // create a new SID for the child process
  pid_t sid = setsid();

  if (sid < 0) {
    SDB_FATAL("xxxxx", sdb::Logger::FIXME, "cannot create sid");
  }

  // store current working directory
  FileResultString cwd = file_utils::CurrentDirectory();

  if (!cwd.ok()) {
    SDB_FATAL("xxxxx", sdb::Logger::FIXME,
              "cannot get current directory: ", cwd.errorMessage());
  }

  _current = cwd.result();

  // change the current working directory
  if (!working_directory.empty()) {
    FileResult res = file_utils::ChangeDirectory(working_directory.c_str());

    if (!res.ok()) {
      SDB_FATAL("xxxxx", sdb::Logger::STARTUP,
                "cannot change into working directory '", working_directory,
                "': ", res.errorMessage());
    } else {
      SDB_INFO("xxxxx", sdb::Logger::STARTUP,
               "changed working directory for child process to '",
               working_directory, "'");
    }
  }

  remapStandardFileDescriptors();

  return pid;
}

void DaemonFeature::remapStandardFileDescriptors() {
  // we're a daemon so there won't be a terminal attached
  // close the standard file descriptors and re-open them mapped to /dev/null

  // close all descriptors
  for (int i = getdtablesize(); i >= 0; --i) {
    close(i);
  }

  // open fd /dev/null
  int fd = open("/dev/null", O_RDWR | O_CREAT, 0644);

  if (fd < 0) {
    SDB_FATAL("xxxxx", sdb::Logger::FIXME, "cannot open /dev/null");
  }

  // the following calls silently close and repoen the given fds
  // to avoid concurrency issues
  if (dup2(fd, STDIN_FILENO) != STDIN_FILENO) {
    SDB_FATAL("xxxxx", sdb::Logger::FIXME, "cannot re-map stdin to /dev/null");
  }

  if (dup2(fd, STDOUT_FILENO) != STDOUT_FILENO) {
    SDB_FATAL("xxxxx", sdb::Logger::FIXME, "cannot re-map stdout to /dev/null");
  }

  if (dup2(fd, STDERR_FILENO) != STDERR_FILENO) {
    SDB_FATAL("xxxxx", sdb::Logger::FIXME, "cannot re-map stderr to /dev/null");
  }

  // Do not close one of the recently opened fds
  if (fd > 2) {
    close(fd);
  }
}

void DaemonFeature::writePidFile(int pid) {
  try {
    sdb::basics::file_utils::Spit(pid_file, std::to_string(pid), true);
  } catch (const sdb::basics::Exception& ex) {
    SDB_FATAL("xxxxx", sdb::Logger::FIXME, "cannot write pid-file '", pid_file,
              "' - ", ex.what());
  }
}

int DaemonFeature::waitForChildProcess(int pid) {
  if (!isatty(STDIN_FILENO)) {
    // during system boot, we don't have a tty, and we don't want to delay
    // the boot process
    return EXIT_SUCCESS;
  }

  // in case a tty is present, this is probably a manual invocation of the start
  // procedure
  const double end = utilities::GetMicrotime() + 10.0;

  while (utilities::GetMicrotime() < end) {
    int status;
    int res = waitpid(pid, &status, WNOHANG);

    if (res == -1) {
      // error in waitpid. don't know what to do
      break;
    }

    if (res != 0 && WIFEXITED(status)) {
      // give information about supervisor exit code
      if (WEXITSTATUS(status) == 0) {
        // exit code 0
        return EXIT_SUCCESS;
      } else if (WIFSIGNALED(status)) {
        switch (WTERMSIG(status)) {
          case 2:
          case 9:
          case 15:
            // terminated normally
            return EXIT_SUCCESS;
          default:
            break;
        }
      }

      // failure!
      SDB_ERROR(
        "xxxxx", sdb::Logger::FIXME,
        "unable to start serened. please check the logfiles for errors");
      return EXIT_FAILURE;
    }

    // sleep a while and retry
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  // enough time has elapsed... we now abort our loop
  return EXIT_SUCCESS;
}

}  // namespace sdb
