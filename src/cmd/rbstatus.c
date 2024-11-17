/*
 */

#include "../lib/libresb.h"

int
main(int argc, char **argv)
{
    request_t r;
    struct op_request op_req;
    struct op_reply op_rep;
    int cc;

    cc = nio_client_rw(&op_req, &op_rep);


    return 0;
}
