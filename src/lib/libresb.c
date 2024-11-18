/* libresb client side
 */

#include "libresb.h"

int
get_server_data(struct rb_server *r)
{
    r->port = 41200;
    r->ip = "127.0.0.1";
    strcpy(r->name, "buntu24");

    return 0;
}
