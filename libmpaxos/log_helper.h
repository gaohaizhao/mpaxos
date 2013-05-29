

#include "utils/logger.h"
#include "internal_types.h"

static void log_message_rid(const char *action, const char *type,
        Mpaxos__RoundidT **rids, int rids_len) {
    char *log_buf = (char*)malloc(200);
    sprintf(log_buf, "%s %s message", action, type);

    int i;
    for (i = 0; i < rids_len; i++) {
        const Mpaxos__RoundidT *rid_p = rids[i];
        sprintf(log_buf, "%s [gid:%d, sid:%d, bid:%d]", log_buf,
                rid_p->gid, rid_p->sid, rid_p->bid);
    }
    LOG_DEBUG(log_buf);

    free(log_buf);
}

static void log_message_res(const char *action, const char *type,
        Mpaxos__ResponseT **ress, int ress_len) {
    char *log_buf = (char*)malloc(200);
    sprintf(log_buf, "%s %s message", action, type);

    int i;
    for (i = 0; i < ress_len; i++) {
        const Mpaxos__RoundidT *rid_p = ress[i]->rid;
        sprintf(log_buf, "%s [gid:%d, sid:%d, bid:%d]", log_buf,
                rid_p->gid, rid_p->sid, rid_p->bid);
    }
    LOG_DEBUG(log_buf);

    free(log_buf);
}
