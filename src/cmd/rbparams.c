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
	fprintf(stderr, "Network I/O error with the broker\n");
	free(hdr);
	return -1;
    }

    hdr2 = rep.header;
    if (hdr2->opcode != BROKER_OK) {
	fprintf(stderr, "Broker returned error\n");
	free(hdr);
	return -1;
    }

    printf("%s\n", rep.msg_buf);

    free(hdr);
    free(rep.header);

    return 0;
}
