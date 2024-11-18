/*
 */

#include "../lib/libresb.h"

int
main(int argc, char **argv)
{
    struct rb_request req;
    struct rb_reply rep;
    int cc;
    int l;
    char *buf;
    struct rb_server r;

    cc = get_server_data(&r);
    if (cc < 0) {
	return -1;
    }

    req.op = BROKER_STATUS;
    buf = calloc(64, sizeof(char));
    sprintf(buf, "%d", req.op);
    /* Length of request and the following length
     */
    l = sizeof(int) + strlen(buf);
    req.buf_request = calloc(64, sizeof(char));
    sprintf(req.buf_request, "%d%s", l, buf);
    req.len = l;

    free(buf);

    cc = nio_client_rw(&r, &req, &rep);
    if (cc < 0) {
	free(req.buf_request);
	free(rep.buf_reply);
	return -1;
    }

    if (rep.op_status == BROKER_ERROR) {
	fprintf(stderr, "BROKER_ERROR: %s\n", rep.buf_reply);
	free(req.buf_request);
	free(rep.buf_reply);
	return -1;
    }

    printf("BROKER_OK: %s\n", rep.buf_reply);

    free(req.buf_request);
    free(rep.buf_reply);

    return 0;
}
