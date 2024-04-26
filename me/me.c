#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <mqueue.h>
#include <omp.h>
#include "me.h"
#include <stdio.h>

/* Most of the matching logic was implemented for buy orders and them
 * copy-pasted to sell ones, which is not exactly a good pratice but does the
 * job. */

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
	if ((dumb_q = mq_open("/fintexmedumb", O_CREAT | O_RDWR | O_NONBLOCK,
			      0777, NULL)) == -1) {
		return context;
	}

	mq_getattr(dumb_q, &qattr);
	mq_close(dumb_q);
	mq_unlink("/fintexmedumb");
	qattr.mq_msgsize = sizeof(MeMessage);

	if ((context->incoming = mq_open(me_in_queue_name, O_CREAT | O_RDWR,
					 0777, &qattr)) == -1) {
		return context;
	}
	if ((context->outcoming = mq_open(me_out_queue_name, O_CREAT | O_RDWR,
					  0777, &qattr)) == -1) {
		return context;
	}

	size_t headers_s =
		sizeof(MeContext) + n_secs * sizeof(MeSecurityContext);
	size_t full_book_s = (l2_s - headers_s) / (2 * n_secs);
	context->buf_size = (full_book_s - sizeof(MeBook)) / sizeof(MeOrder);
	context->contexts =
		(MeSecurityContext *)(((size_t)context) + sizeof(MeContext));

	register MeBook *book = (MeBook *)(((size_t)context) + headers_s);

	/* Init first security context. */
	context->contexts[0].market_price = 0;
	omp_init_lock(&context->contexts[0].lock);

	context->contexts[0].buy = book;
	book = (MeBook *)(((size_t)context->contexts[0].buy) + full_book_s);
	context->contexts[0].sell = book;
	book = (MeBook *)(((size_t)context->contexts[0].sell) + full_book_s);

	context->contexts[0].buy->used = 0;
	context->contexts[0].sell->used = 0;
	context->contexts[0].buy->next = NULL;
	context->contexts[0].sell->next = NULL;

	/* Init the rest. */
	for (size_t i = 1; i < n_secs; i++) {
		context->contexts[i].market_price = i;
		omp_init_lock(&context->contexts[i].lock);

		context->contexts[i].buy = book;
		book = (MeBook *)(((size_t)context->contexts[i].buy) +
				  full_book_s);
		context->contexts[i].sell = book;
		book = (MeBook *)(((size_t)context->contexts[i].sell) +
				  full_book_s);

		context->contexts[i].buy->used = 0;
		context->contexts[i].sell->used = 0;
		context->contexts[i].buy->next = NULL;
		context->contexts[i].sell->next = NULL;
	}

	return context;
}

void me_dealloc_context(MeContext *context, void deallocate(void *))
{
	mq_close(context->incoming);
	mq_close(context->outcoming);
	mq_unlink(me_in_queue_name);
	mq_unlink(me_out_queue_name);

	for (size_t i = 0; i < context->n_securities; i++)
		omp_destroy_lock(&context->contexts[i].lock);

	deallocate(context);
}

#define sendmsg(context, msg) \
	(mq_send((context)->outcoming, (char *)(msg), sizeof(MeMessage), 1))

static inline void set_market_price(MeContext *context, MeSecurityContext *ctx,
				    MeMessage *msg)
{
	omp_set_lock(&ctx->lock);
	ctx->market_price = msg->message.set_market_price;
	omp_unset_lock(&ctx->lock);
	/* Propagate the message to the outcoming. */
	sendmsg(context, msg);
}

#define LEFT(a) (2 * (a) + 1)
#define RIGHT(a) (2 * (a) + 2)
#define PARENT(a) (((a) - ((a) + 1) % 2) / 2)

#define BUY_GREATER_THAN(a, b)    \
	((a).price > (b).price || \
	 ((a).price == (b).price && (a).timestamp < (b).timestamp))
#define SELL_GREATER_THAN(a, b)   \
	((a).price < (b).price || \
	 ((a).price == (b).price && (a).timestamp < (b).timestamp))

static inline void trade(MeContext *context, MeSecurityContext *ctx,
			 MeOrder *aggressor, MeOrder *other, size_t id,
			 int64_t price)
{
	MeMessage send;
	int64_t last_price = ctx->market_price;
	ctx->market_price = price;

	send.msg_type = ME_MESSAGE_TRADE;
	send.security_id = id;
	send.message.trade.aggressor = *aggressor;
	send.message.trade.matched_id = other->order_id;
	sendmsg(context, &send);

	if (last_price != price) {
		send.msg_type = ME_MESSAGE_SET_MARKET_PRICE;
		send.message.set_market_price = price;
		sendmsg(context, &send);
	}
}

static inline void order_executed(MeContext *context, MeOrder *order, size_t id)
{
	MeMessage to_send;
	to_send.msg_type = ME_MESSAGE_ORDER_EXECUTED;
	to_send.security_id = id;
	to_send.message.order = *order;
	sendmsg(context, &to_send);
}

static inline void remove_first_sell(MeContext *context, MeSecurityContext *ctx)
{
	/* TODO: next books. */
	(void)context;

	MeBook *book = ctx->sell;
	book->orders[0] = book->orders[book->used - 1];
	book->used--;
	MeOrder tmp;

	/* I don't know how to optimize this and keep it readable. */
	int64_t i = 0;
	uint8_t swapped = 1;
	do {
		if (book->used > LEFT(i) &&
		    SELL_GREATER_THAN(book->orders[LEFT(i)], book->orders[i])) {
			tmp = book->orders[i];
			book->orders[i] = book->orders[LEFT(i)];
			book->orders[LEFT(i)] = tmp;
			i = LEFT(i);
		} else if (book->used > RIGHT(i) &&
			   SELL_GREATER_THAN(book->orders[RIGHT(i)],
					     book->orders[i])) {
			tmp = book->orders[i];
			book->orders[i] = book->orders[RIGHT(i)];
			book->orders[RIGHT(i)] = tmp;
			i = RIGHT(i);
		} else {
			swapped = !swapped;
		}
	} while (!swapped);
}

static inline void remove_first_buy(MeContext *context, MeSecurityContext *ctx)
{
	/* TODO: next books. */
	(void)context;

	MeBook *book = ctx->buy;

	book->orders[0] = book->orders[book->used - 1];
	book->used--;
	MeOrder tmp;

	int64_t i = 0;
	uint8_t swapped = 1;
	do {
		if (book->used > LEFT(i) &&
		    BUY_GREATER_THAN(book->orders[LEFT(i)], book->orders[i])) {
			tmp = book->orders[i];
			book->orders[i] = book->orders[LEFT(i)];
			book->orders[LEFT(i)] = tmp;
			i = LEFT(i);
		} else if (book->used > RIGHT(i) &&
			   BUY_GREATER_THAN(book->orders[RIGHT(i)],
					    book->orders[i])) {
			tmp = book->orders[i];
			book->orders[i] = book->orders[RIGHT(i)];
			book->orders[RIGHT(i)] = tmp;
			i = RIGHT(i);
		} else {
			swapped = !swapped;
		}
	} while (!swapped);
}

static inline void new_limit_buy(MeBook *book, MeOrder *order, size_t buf_size)
{
	int64_t new_pos = book->used;
	/* TODO: handle this. */
	if (((size_t)new_pos) > buf_size)
		return;

	int64_t parent = PARENT(new_pos);
	/* PARENT(0) = 0. */
	while (parent != new_pos &&
	       BUY_GREATER_THAN(*order, book->orders[parent])) {
		book->orders[new_pos] = book->orders[parent];
		new_pos = parent;
		parent = PARENT(new_pos);
	}
	book->orders[new_pos] = *order;

	book->used++;
}

static inline void new_limit_sell(MeBook *book, MeOrder *order, size_t buf_size)
{
	int64_t new_pos = book->used;
	/* TODO: handle this. */
	if (((size_t)new_pos) > buf_size)
		return;

	int64_t parent = PARENT(new_pos);
	/* PARENT(0) = 0. */
	while (parent != new_pos &&
	       SELL_GREATER_THAN(*order, book->orders[parent])) {
		book->orders[new_pos] = book->orders[parent];
		new_pos = parent;
		parent = PARENT(new_pos);
	}
	book->orders[new_pos] = *order;

	book->used++;
}

static inline void swipe_market_buy(MeContext *context, MeSecurityContext *ctx,
				    MeMessage *msg)
{
	int64_t new_aggressor_quantity = msg->message.order.quantity;
	/* Propagate the new order message. */
	sendmsg(context, msg);

	/* Book swipe. */
	while (ctx->sell->used > 0) {
		int64_t new_matched_quantity = ctx->sell->orders[0].quantity;
		new_aggressor_quantity -= new_matched_quantity;
		new_matched_quantity -= msg->message.order.quantity;
		trade(context, ctx, &msg->message.order, &ctx->sell->orders[0],
		      msg->security_id, ctx->sell->orders[0].price);
		msg->message.order.quantity = new_aggressor_quantity;
		ctx->sell->orders[0].quantity = new_matched_quantity;

		if (new_matched_quantity <= 0) {
			order_executed(context, &ctx->sell->orders[0],
				       msg->security_id);
			remove_first_sell(context, ctx);
			/* new_aggressor_quantity <= 0 */
		} else {
			order_executed(context, &msg->message.order,
				       msg->security_id);
			return;
		}
	}

	if (new_aggressor_quantity > 0) {
		msg->message.order.ord_type = ME_ORDER_LIMIT;
		msg->message.order.price = ctx->market_price;
		/* Propagate again as limit. */
		sendmsg(context, msg);

		new_limit_buy(ctx->buy, &msg->message.order, context->buf_size);
	}
}

static inline void swipe_market_sell(MeContext *context, MeSecurityContext *ctx,
				     MeMessage *msg)
{
	int64_t new_aggressor_quantity = msg->message.order.quantity;
	/* Propagate the new order message. */
	sendmsg(context, msg);

	/* Book swipe. */
	while (ctx->buy->used > 0) {
		int64_t new_matched_quantity = ctx->buy->orders[0].quantity;
		new_aggressor_quantity -= new_matched_quantity;
		new_matched_quantity -= msg->message.order.quantity;
		trade(context, ctx, &msg->message.order, &ctx->buy->orders[0],
		      msg->security_id, ctx->buy->orders[0].price);
		msg->message.order.quantity = new_aggressor_quantity;
		ctx->buy->orders[0].quantity = new_matched_quantity;

		if (new_matched_quantity <= 0) {
			order_executed(context, &ctx->buy->orders[0],
				       msg->security_id);
			remove_first_buy(context, ctx);
			/* new_aggressor_quantity <= 0 */
		} else {
			order_executed(context, &msg->message.order,
				       msg->security_id);
			return;
		}
	}

	if (new_aggressor_quantity > 0) {
		msg->message.order.ord_type = ME_ORDER_LIMIT;
		msg->message.order.price = ctx->market_price;
		/* Propagate again as limit. */
		sendmsg(context, msg);

		new_limit_sell(ctx->sell, &msg->message.order,
			       context->buf_size);
	}
}

static inline void swipe_limit_buy(MeContext *context, MeSecurityContext *ctx,
				   MeMessage *msg)
{
	int64_t new_aggressor_quantity = msg->message.order.quantity;
	/* Propagate the new order message. */
	sendmsg(context, msg);

	while (ctx->sell->used > 0 &&
	       msg->message.order.price >= ctx->sell->orders[0].price) {
		int64_t new_matched_quantity = ctx->sell->orders[0].quantity;
		new_aggressor_quantity -= new_matched_quantity;
		new_matched_quantity -= msg->message.order.quantity;
		trade(context, ctx, &msg->message.order, &ctx->sell->orders[0],
		      msg->security_id, ctx->sell->orders[0].price);
		msg->message.order.quantity = new_aggressor_quantity;
		ctx->sell->orders[0].quantity = new_matched_quantity;

		if (new_matched_quantity <= 0) {
			order_executed(context, &ctx->sell->orders[0],
				       msg->security_id);
			remove_first_sell(context, ctx);
			/* new_aggressor_quantity <= 0 */
		} else {
			order_executed(context, &msg->message.order,
				       msg->security_id);
			return;
		}
	}

	/* Don't need to propagate again. */
	new_limit_buy(ctx->buy, &msg->message.order, context->buf_size);
}

static inline void swipe_limit_sell(MeContext *context, MeSecurityContext *ctx,
				    MeMessage *msg)
{
	int64_t new_aggressor_quantity = msg->message.order.quantity;
	/* Propagate the new order message. */
	sendmsg(context, msg);

	while (ctx->buy->used > 0 &&
	       msg->message.order.price <= ctx->buy->orders[0].price) {
		int64_t new_matched_quantity = ctx->buy->orders[0].quantity;
		new_aggressor_quantity -= new_matched_quantity;
		new_matched_quantity -= msg->message.order.quantity;
		trade(context, ctx, &msg->message.order, &ctx->buy->orders[0],
		      msg->security_id, ctx->buy->orders[0].price);
		msg->message.order.quantity = new_aggressor_quantity;
		ctx->buy->orders[0].quantity = new_matched_quantity;

		if (new_matched_quantity <= 0) {
			order_executed(context, &ctx->buy->orders[0],
				       msg->security_id);
			remove_first_buy(context, ctx);
			/* new_aggressor_quantity <= 0 */
		} else {
			order_executed(context, &msg->message.order,
				       msg->security_id);
			return;
		}
	}

	/* Don't need to propagate again. */
	new_limit_sell(ctx->sell, &msg->message.order, context->buf_size);
}

static inline void new_order(MeContext *context, MeSecurityContext *ctx,
			     MeMessage *msg)
{
	omp_set_lock(&ctx->lock);
	if (msg->message.order.side == ME_SIDE_BUY) {
		if (msg->message.order.ord_type == ME_ORDER_MARKET)
			swipe_market_buy(context, ctx, msg);
		else
			swipe_limit_buy(context, ctx, msg);
	} else {
		if (msg->message.order.ord_type == ME_ORDER_MARKET)
			swipe_market_sell(context, ctx, msg);
		else
			swipe_limit_sell(context, ctx, msg);
	}
	omp_unset_lock(&ctx->lock);
}

void *me_run(MeContext *context, void *paralell_job(void *), void *job_arg)
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
			mq_receive(context->incoming, (char *)&msg,
				   sizeof(MeMessage), &p);
			if (msg.security_id < context->n_securities) {
				ctx = &context->contexts[msg.security_id];
				switch (msg.msg_type) {
				case ME_MESSAGE_SET_MARKET_PRICE:
					set_market_price(context, ctx, &msg);
					break;
				case ME_MESSAGE_NEW_ORDER:
					new_order(context, ctx, &msg);
					break;
				case ME_MESSAGE_CANCEL_ORDER:
				case ME_MESSAGE_TRADE:
				case ME_MESSAGE_ORDER_EXECUTED:
				case ME_MESSAGE_PANIC:
					break;
				}
			}
		} while (msg.msg_type != ME_MESSAGE_PANIC);

		/* Send a panic to the next thread. */
		sendmsg(context, &msg);
	}

	/* Inform those listening on outcoming that we're bailing out. */
	msg.msg_type = ME_MESSAGE_PANIC;
	sendmsg(context, &msg);
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
	mq_send(context->incoming, (char *)message, sizeof(MeMessage), 1);
	return errno;
}

int me_client_get_message(MeClientContext *context, MeMessage *message)
{
	unsigned int _p;
	mq_receive(context->outcoming, (char *)message, sizeof(MeMessage), &_p);
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
	size_t l2_s = 1024 * 1024 * 1024 + 512 * 1024 * 1024;
	size_t n_securities = 400;

	for (int i = 1; i < argc; i++) {
		if (sscanf(argv[i], "-c=%zu", &l2_s) == 1 ||
		    sscanf(argv[i], "--cache-size=%zu", &l2_s) == 1 ||
		    sscanf(argv[i], "-s=%zu", &n_securities) == 1 ||
		    sscanf(argv[i], "--securities=%zu", &n_securities) == 1) {
			continue;
		} else if (strcmp(argv[i], "-h") == 0 ||
			   strcmp(argv[i], "--help") == 0) {
			printf(help, argv[0]);
			return 0;
		}
	}

	MeContext *context = me_alloc_context(l2_s, n_securities, malloc);
	if (errno != 0) {
		if (errno == 33) {
			fprintf(stderr,
				"No enough memory set for security amount. Minimum of %zu, configured %zu\n",
				ME_MINIMUM_MEMORY(n_securities), l2_s);
		}

		return errno;
	}
	printf("Booting engine with %zu of cache size and %zu securities.\n",
	       l2_s, n_securities);
	me_run(context, NULL, NULL);
	me_dealloc_context(context, free);

	printf("Engine bailing out.\n");

	return 0;
}

#endif /* ME_BINARY */
