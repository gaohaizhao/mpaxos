

#include "utils/logger.h"
#include "internal_types.h"

static void log_message_rid(const char *action, const char *type,
        msg_header_t *h, roundid_t **rids, size_t sz_rids, size_t sz_msg) {

//    int i;
//    for (i = 0; i < sz_rids; i++) {
//        const roundid_t *rid = rids[i];
//        sprintf(log_buf, "%s [gid:%"PRId64", sid:%"PRId64", bid:%"PRId64"]", log_buf,
//                rid->gid, rid->sid, rid->bid);
//    }
    LOG_DEBUG("%s %s message. tid: %"PRIx64", nid: %u, size: %lu, bid: %lu", 
            action, type, h->tid, h->nid, sz_msg, rids[0]->bid);
}

static void log_message_res(const char *action, const char *type,
        msg_header_t *h, response_t **ress, int ress_len, size_t sz_msg) {

//    int i;
//    for (i = 0; i < ress_len; i++) {
//        const Mpaxos__RoundidT *rid_p = ress[i]->rid;
//        sprintf(log_buf, "%s [gid:%"PRId64", sid:%"PRId64", bid:%"PRId64"]", log_buf,
//                rid_p->gid, rid_p->sid, rid_p->bid);
//    }
    LOG_DEBUG("%s %s message. tid: %"PRIx64", nid: %u, size: %lu", 
            action, type, h->tid, h->nid, sz_msg);
}
