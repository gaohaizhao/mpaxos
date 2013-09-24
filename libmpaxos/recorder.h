
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <json/json.h>
#include <apr_hash.h>
#include <apr_queue.h>
#include <apr_thread_pool.h>
#include "mpaxos/mpaxos.h"
#include "proposer.h"
#include "acceptor.h"
#include "view.h"
#include "slot_mgr.h"
#include "utils/logger.h"
#include "utils/mtime.h"
#include "utils/mlock.h"
#include "comm.h"
#include "async.h"

slotid_t get_newest_sid(groupid_t gid);

void recorder_init();

void recorder_init();

int put_instval(groupid_t gid, slotid_t sid, uint8_t *data, size_t sz_data); 
