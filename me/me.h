#ifndef __ME_HEADER
#define __ME_HEADER

#include <mqueue.h>
#include <omp.h>
#include <stdint.h>

static const char *me_in_queue_name = "/fintexmeincoming";
static const char *me_out_queue_name = "/fintexmeoutcoming";

typedef enum {
  ME_SIDE_BUY,
  ME_SIDE_SELL,
} MeSide;

typedef enum {
  ME_ORDER_MARKET,
  ME_ORDER_LIMIT,
} MeOrderType;

typedef enum {
  ME_MESSAGE_NEW_ORDER,
  ME_MESSAGE_CANCEL_ORDER,
  ME_MESSAGE_SET_MARKET_PRICE,
  ME_MESSAGE_TRADE,
  ME_MESSAGE_ORDER_EXECUTED,
  ME_MESSAGE_PANIC,
} MeMessageType;

/* We would usually say it has nanossecond precision but the client may actually
 * just make something up. We only care with comparing this value, and not it's
 * precision. */
typedef uint64_t MeTimestamp;

/* The engine itself does not use it except for cancelling orders. The clients
 * should set it and guarantee they're unique. */
typedef uint64_t MeOrderID;

typedef struct {
  MeSide side;
  int64_t quantity;
  MeOrderType ord_type;
  int64_t price;
  MeOrderID order_id;
  MeTimestamp timestamp;
} MeOrder;

typedef struct {
  MeOrder aggressor;
  MeOrderID matched_id;
} MeTrade;

/* NEW, CANCEL and SET_MARKET_PRICE are received by the matching engine and
 * propagated. SET_MARKET_PRICE is also used by the engine to inform a change in
 * the market price. TRADE is only used by the engine to inform a trade event
 * (i.e., orders matched). The TRADE DOES NOT IMPLY a cancellation of orders.
 * The cancellation will be informed by the engine if some order is fully
 * executed.
 *
 * The TRADE characteristics are informed in the aggressor field, which may not
 * be equal to the order itself as in the book. The values shall be applied to
 * the order specified by the matched_id field accordingly, so to update the
 * client with the information in the book (incrementally).
 *
 * I.e., if an order to sell 200 is the aggressor in a trade with an order to
 * buy 300, the aggressor field will have the quantity set to 200, and the order
 * identified by the matched_id should have it's own quantity updated to 100
 * (300 - 200). */
typedef struct {
  MeMessageType msg_type;
  int64_t security_id;
  union {
    MeOrder order;
    int64_t set_market_price;
    MeTrade trade;
    MeOrderID to_cancel;
  } message;
} MeMessage;

#define ME_MINIMUM_MEMORY(n_secs) \
  (sizeof(MeContext) + n_secs * (sizeof(MeSecurityContext) + sizeof(MeOrder)))

/* "Server" (engine) side. */

typedef struct MeBook {
  /* An int64_t for convenience. Signed indexes are such a great idea. */
  int64_t used;
  struct MeBook *next;
  /* Size is MeContext.buf_size. In indexes, not bytes. */
  MeOrder orders[];
} MeBook;

typedef struct {
  MeBook *buy;
  MeBook *sell;
  int64_t market_price;
  omp_lock_t lock;
} MeSecurityContext;

typedef struct {
  int64_t n_securities;
  int64_t buf_size;
  MeSecurityContext *contexts;
  mqd_t incoming;
  mqd_t outcoming;
} MeContext;

/* clang-format off */
/* Propagates the allocator errno if it returns NULL. If l2_s is less than the
 * minimum amount needed by the engine, returns NULL and sets errno to EDOM.
 * Also sets EDOM if n_securities == 0. Other errors may be propagated (mq_open,
 * etc). In this cases, the memory IS NOT FREE'D AND IT'S POINTER IS RETURNED.
 *
 * Of course, this mean the caller SHOULD ALWAYS check the errno value and
 * operate accordingly.
 *
 * This also means that if the actual L2 cache size is less than the minimum
 * required to operate the engine properly, the caller should make something up.
 *
 * Example:
 * 
 * MeContext *context = me_alloc_context(1024*1024*1024 + 512*1024*1024, 400, malloc);
 * if (context == NULL) {
 *   printf("buy more ram\n");
 *   exit(1);
 * }
 * if (errno != 0) {
 *   printf("something went wrong\n");
 *   free(context);
 *   exit(1);
 * }
 */
/* clang-format on */
MeContext *me_alloc_context(size_t l2_s, int64_t n_secs,
                            void *allocate(size_t));
void me_dealloc_context(MeContext *context, void deallocate(void *));
void *me_run(MeContext *context, void *paralell_job(void *), void *job_arg);

/* "Client" side. */

typedef struct {
  mqd_t incoming;
  mqd_t outcoming;
} MeClientContext;

int me_client_init_context(MeClientContext *context);
void me_client_close_context(MeClientContext *context);
int me_client_send_message(MeClientContext *context, MeMessage *message);
int me_client_get_message(MeClientContext *context, MeMessage *message);

#endif /* __ME_HEADER */
