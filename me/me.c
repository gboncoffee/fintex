#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <mqueue.h>
#include <omp.h>
#include "me.h"
#include <stdio.h>

MeContext *me_alloc_context(size_t l2_s, size_t n_secs, void *allocate(size_t))
{
	MeContext *context;
	struct mq_attr qattr;
	mqd_t dumb_q;

	errno = 0;

	if (l2_s < ME_MINIMUM_MEMORY(n_secs) || n_secs == 0) {
		errno = EDOM;
		return NULL;
	}

	if (!(context = allocate(l2_s)))
		return NULL;

	context->n_securities = n_secs;

	/* Create a dumb queue to get the attributes. */
	if ((dumb_q = mq_open("/fintexmedumb", O_CREAT|O_RDWR|O_NONBLOCK, 0777, NULL)) == -1)
		return context;
	mq_getattr(dumb_q, &qattr);
	mq_close(dumb_q);
	mq_unlink("/fintexmedumb");
	qattr.mq_msgsize = sizeof(MeMessage);

	if ((context->incoming = mq_open(me_in_queue_name, O_CREAT|O_RDWR, 0777, &qattr)) == -1)
		return context;
	if ((context->outcoming = mq_open(me_out_queue_name, O_CREAT|O_RDWR, 0777, &qattr)) == -1)
		return context;

	size_t headers_s = sizeof(MeContext) + n_secs*sizeof(MeSecurityContext);
	size_t full_book_s = (l2_s - headers_s) / (2 * n_secs);
	context->buf_size = full_book_s - sizeof(MeBook);
	context->contexts = (MeSecurityContext*) (((size_t) context) + sizeof(MeContext));

	register MeBook *book = (MeBook*) (((size_t) context) + headers_s);

	/* Init first security context. */
	context->contexts[0].market_price = 0;
	omp_init_lock(&context->contexts[0].lock);

	context->contexts[0].buy = book;
	book = (MeBook*) (((size_t) context->contexts[0].buy) + full_book_s);
	context->contexts[0].sell = book;
	book = (MeBook*) (((size_t) context->contexts[0].sell) + full_book_s);

	context->contexts[0].buy->used = 0;
	context->contexts[0].sell->used = 0;
	context->contexts[0].buy->next = NULL;
	context->contexts[0].sell->next = NULL;

	/* Init the rest. */
	for (size_t i = 1; i < n_secs; i++) {
		context->contexts[i].market_price = i;
		omp_init_lock(&context->contexts[i].lock);

		context->contexts[i].buy = book;
		book = (MeBook*) (((size_t) context->contexts[i].buy) + full_book_s);
		context->contexts[i].sell = book;
		book = (MeBook*) (((size_t) context->contexts[i].sell) + full_book_s);

		context->contexts[i].buy->used = i;
		context->contexts[i].sell->used = i;
		context->contexts[i].buy->next = NULL;
		context->contexts[i].sell->next = NULL;
	}

	return context;
}

void me_dealloc_context(MeContext *context, void deallocate(void*))
{
	mq_close(context->incoming);
	mq_close(context->outcoming);
	mq_unlink(me_in_queue_name);
	mq_unlink(me_out_queue_name);

	for (size_t i = 0; i < context->n_securities; i++)
		omp_destroy_lock(&context->contexts[i].lock);

	deallocate(context);
}

static inline void set_market_price(MeContext *context, MeSecurityContext *ctx, MeMessage *msg)
{
	omp_set_lock(&ctx->lock);
	ctx->market_price = msg->message.set_market_price;
	omp_unset_lock(&ctx->lock);
	/* Propagate the message to the outcoming. */
	mq_send(context->outcoming, (char*) msg, sizeof(MeMessage), 1);
}

#define LEFT(a) (2*a + 1)
#define RIGHT(a) (2*a + 2)

static inline void trade(MeContext *context, MeOrder *aggressor, MeOrder *other)
{
	(void) context;
	(void) aggressor;
	(void) other;
	/* TODO */
}

static inline void new_market_buy(MeContext *context, MeSecurityContext *ctx, MeMessage *msg)
{
	MeMessage to_send;
	to_send.msg_type = ME_MESSAGE_TRADE;
	to_send.security_id = msg->security_id;
	B3Quantity new_aggressor_quantity = msg->quantity;
	B3Quantity new_matched_quantity = ctx->sell->orders[0].quantity;

	while (msg->message.order.quantity > 0 && ctx->sell->used > 0) {
		new_aggressor_quantity -= ctx->sell->orders[0].quantity;
		new_matched_quantity -= msg->message.order.quantity;
		trade(context, &msg->message.order, &ctx->sell->orders[0]);
		msg->message.order.quantity = new_aggressor_quantity;
		ctx->sell->orders[0].quantity = new_matched_quantity;

		to_send.message.trade.aggressor = msg->message.order;
		to_send.message.trade.matched_id = ctx->sell->orders[0].order_id;

		if (new_matched_quantity <= 0)
			remove_first_sell_order(context, ctx);
	}

	/* TODO */

	/* Propagate the message. */
	mq_send(context->outcoming, (char*) msg, sizeof(MeMessage), 1);
}

static inline void new_limit_buy(MeContext *context, MeSecurityContext *ctx, MeMessage *msg)
{
	/* Propagate the message. */
	mq_send(context->outcoming, (char*) msg, sizeof(MeMessage), 1);
}

static inline void new_market_sell(MeContext *context, MeSecurityContext *ctx, MeMessage *msg)
{
	/* Propagate the message. */
	mq_send(context->outcoming, (char*) msg, sizeof(MeMessage), 1);
}

static inline void new_limit_sell(MeContext *context, MeSecurityContext *ctx, MeMessage *msg)
{
	/* Propagate the message. */
	mq_send(context->outcoming, (char*) msg, sizeof(MeMessage), 1);
}

static inline void new_order(MeContext *context, MeSecurityContext *ctx, MeMessage *msg)
{
	MeMessage to_send;
	MeBook *book;
	if (msg->order.side == B3_BUY) {
		if (msg->order.ord_type == B3_ORD_MARKET)
			new_market_buy(context, ctx, msg);
		else
			new_limit_buy(context, ctx, msg);
	} else {
		if (msg->order.ord_type == B3_ORD_MARKET)
			new_market_sell(context, ctx, msg);
		else
			new_limit_sell(context, ctx, msg);
	}

}

void *me_run(MeContext *context, void *paralell_job(void*), void *job_arg)
{
	void *r = NULL;
	MeMessage msg;
	unsigned int p;
	MeSecurityContext *ctx;

	if (paralell_job != NULL) {
		#pragma omp task
		{
			r = paralell_job(job_arg);
		}
	}

	#pragma omp parallel private(msg, p, ctx)
	{
		do {
			mq_receive(context->incoming, (char*) &msg, sizeof(MeMessage), &p);
			if (msg.security_id < context->n_securities) {
				ctx = &context->contexts[msg.security_id];
				switch (msg.msg_type) {
				case ME_MESSAGE_SET_MARKET_PRICE:
					set_market_price(context, ctx, &msg);
					break;
				case B3_MESSAGE_TYPE_SIMPLE_NEW_ORDER:
				case B3_MESSAGE_TYPE_NEW_ORDER_SINGLE:
					new_order(context, ctx, &msg);
					break;
				}
			}
		} while (msg.msg_type != ME_MESSAGE_PANIC);

		/* Send a panic to the next thread. */
		mq_send(context->incoming, (char*) &msg, sizeof(MeMessage), 1);
	}

	/* Inform those listening on outcoming that we're bailing out. */
	msg.msg_type = ME_MESSAGE_PANIC;
	mq_send(context->outcoming, (char*) &msg, sizeof(MeMessage), 1);
	return r;
}

int me_client_init_context(MeClientContext *context)
{
	if ((context->incoming = mq_open(me_in_queue_name, O_WRONLY)) == -1)
		return errno;
	if ((context->outcoming = mq_open(me_out_queue_name, O_RDONLY)) == -1)
		return errno;

	return 0;
}

void me_client_close_context(MeClientContext *context)
{
	mq_close(context->incoming);
	mq_close(context->outcoming);
}

int me_client_send_message(MeClientContext *context, MeMessage *message)
{
	mq_send(context->incoming, (char*) message, sizeof(MeMessage), 1);
	return errno;
}

int me_client_get_message(MeClientContext *context, MeMessage *message)
{
	unsigned int _p;
	mq_receive(context->outcoming, (char*) message, sizeof(MeMessage), &_p);
	return errno;
}

#ifdef ME_BINARY

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static const char *help =
	"FinTEx Matching Engine\n"
	"Copyright (C) 2024  Gabriel de Brito\n"
	"\n"
	"Usage: %s [options]\n"
	"All options are in the form -o=v or --option=v\n"
	"Options:\n"
	"\n"
	"-c --cache-size\n"
	"	The engine tries to be friendly to a L2 cache instance. This option\n"
	"	specifies it's size. If the size specified is less than the minimum\n"
	"	memory size used by the engine, it will fail. In the case there's no\n"
	"	enough cache, this option should be set to a big value to compensate the\n"
	"	lack of cache with huge memory buffers. Defaults to 1610612736.\n"
	"-s --securities\n"
	"	Amount of securities to match. Can be very big. IDs are 0-<this size-1>.\n"
	"	Defaults to 400.\n";

int main(int argc, char *argv[])
{
	size_t l2_s = 1024*1024*1024 + 512*1024*1024;
	size_t n_securities = 400;

	for (int i = 1; i < argc; i++) {
		if (sscanf(argv[i], "-c=%zu", &l2_s) == 1
			|| sscanf(argv[i], "--cache-size=%zu", &l2_s) == 1
			|| sscanf(argv[i], "-s=%zu", &n_securities) == 1
			|| sscanf(argv[i], "--securities=%zu", &n_securities) == 1)
		{
			continue;
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf(help, argv[0]);
			return 0;
		}
	}

	MeContext *context = me_alloc_context(l2_s, n_securities, malloc);
	if (errno != 0) {
		if (errno == 33)
			fprintf(stderr, "No enough memory set for security amount. Minimum of %zu, configured %zu\n", ME_MINIMUM_MEMORY(n_securities), l2_s);

		return errno;
	}
	printf("Booting engine with %zu of cache size and %zu securities.\n", l2_s, n_securities);
	me_run(context, NULL, NULL);
	me_dealloc_context(context, free);

	printf("Engine bailing out.\n");

	return 0;
}

#endif /* ME_BINARY */
