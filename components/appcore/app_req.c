#include "app_req.h"

#include <stdatomic.h>

static atomic_uint s_pending;

void app_req_post(app_req_t req)
{
    atomic_fetch_or(&s_pending, 1u << req);
}

bool app_req_take(app_req_t req)
{
    const unsigned bit = 1u << req;
    return (atomic_fetch_and(&s_pending, ~bit) & bit) != 0;
}

bool app_req_pending(app_req_t req)
{
    return (atomic_load(&s_pending) & (1u << req)) != 0;
}
