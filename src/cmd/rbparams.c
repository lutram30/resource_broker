/*
 */

#include "../resb.h"
#include "../lib/libresb.h"

int
main(int argc, char **argv)
{
    struct rb_message req;
    struct rb_message rep;
    struct rb_header *hdr;
    struct rb_header *hdr2;
    int cc;
    struct rb_server r;

    cc = get_server_data(&r);
    if (cc < 0) {
	return -1;
    }

    hdr = calloc(1, sizeof(struct rb_header));
    hdr->opcode = BROKER_PARAMS;
    hdr->len = 0;
    req.header = hdr;

    cc = nio_client_rw(&r, &req, &rep);
    if (cc < 0) {
	free(hdr);
	return -1;
    }

    hdr2 = rep.header;
    if (hdr2->opcode != BROKER_OK) {
	return -1;
    }

    printf("%s", rep.msg_buf);

    return 0;
}
