#ifndef __ME_HEADER
#define __ME_HEADER

#include <stdint.h>
#include <mqueue.h>
#include "../b3.h"

static const char *me_in_queue_name = "/fintexmeincoming";
static const char *me_out_queue_name = "/fintexmeoutcoming";

typedef struct {
	B3Side side;
	B3Quantity quantity;
	B3OrdType ord_type;
	B3Price price;
	B3OrderID order_id;
	B3UTCTimestampNanos timestamp;
} MeOrder;

typedef struct {
	MeOrder aggressor;
	B3OrderID matched_id;
} MeTrade;

/* me_msg_type is set according to the B3MessageType send by the client, being
 * it 102 for NEW and 105 for CANCEL. Otherwise it's one of the defined
 * below.
 *
 * NEW, CANCEL and SET_MARKET_PRICE are received by the matching engine and
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
	B3MessageType msg_type;
	B3SecurityID security_id;
	union {
		MeOrder order;
		B3Price set_market_price;
		MeTrade trade;
		B3OrderID to_cancel;
	} message;
} MeMessage;

#define ME_MINIMUM_MEMORY(n_secs) (sizeof(MeContext) + n_secs*(sizeof(MeSecurityContext) + sizeof(MeOrder)))

/* "Server" (engine) side. */

/* There's no guarantee this will not conflict with other message types in the
 * future. */
#define ME_MESSAGE_SET_MARKET_PRICE 255
#define ME_MESSAGE_TRADE 254
#define ME_MESSAGE_PANIC 253

typedef struct {
	size_t used;
	struct MeBook *next;
	/* Size is MeContext.buf_size */
	MeOrder orders[];
} MeBook;

typedef struct {
	MeBook *buy;
	MeBook *sell;
	B3Price market_price;
	omp_lock_t lock;
} MeSecurityContext;

typedef struct {
	size_t n_securities;
	size_t buf_size;
	MeSecurityContext *contexts;
	mqd_t incoming;
	mqd_t outcoming;
} MeContext;

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
 *     MeContext *context = me_alloc_context(1024*1024*1024 + 512*1024*1024, 400, malloc);
 *     if (context == NULL) {
 *         printf("buy more ram\n");
 *         exit(1);
 *     }
 *     if (errno != 0) {
 *         printf("something went wrong\n");
 *         free(context);
 *         exit(1);
 *     }
 */
MeContext *me_alloc_context(size_t l2_s, size_t n_secs, void *allocate(size_t));
void me_dealloc_context(MeContext *context, void deallocate(void*));
void *me_run(MeContext *context, void *paralell_job(void*), void *job_arg);

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
