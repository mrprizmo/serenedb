#pragma once

#ifdef SDB_CLUSTER
#include "cluster/serened_cluster.h"
#else
#include "rest_server/serened_single.h"
#endif
