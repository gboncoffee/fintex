#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <omp.h>
#include "me.h"

static const char *help =
	"FinTEx Matching Engine CLI\n"
	"Copyright (C) 2024  Gabriel de Brito\n"
	"\n"
	"Usage: %s <security ID> <message type> [message arguments]\n"
	"Message arguments are in the form key=value\n"
	"\n"
	"Message types:\n"
	"buy\n"
	"	quantity=<number>\n"
	"	price=<number (zero or unset for market order)>\n"
	"sell\n"
	"	quantity=<number>\n"
	"	price=<number (zero or unset for market order)>\n"
	"set\n"
	"	price=<number>\n"
	"cancel\n"
	"	id=<order ID>\n"
	"panic\n"
	"	no arguments.\n"
	"\n"
	"The panic message ignores the security ID.\n"
	"Examples:\n"
	"$ %s 0 panic # to shutdown the engine\n"
	"$ %s 3 buy quantity=30 # buy 30 from security 3, market order\n"
	"$ %s 5 sell quantity=20 price=10 # sell 20 of security 5, limit of 10\n";

void build_order(MeMessage *message, char *argv[], int argc, MeSide side)
{
	struct timespec time;
	message->msg_type = ME_MESSAGE_NEW_ORDER;
	message->message.order.side = side;
	message->message.order.quantity = 0;
	message->message.order.price = 0;
	if (clock_gettime(CLOCK_REALTIME, &time)) {
		perror("Order build failed due to failure in retrieving timestamp");
		exit(1);
	}
	message->message.order.timestamp = (MeTimestamp)time.tv_nsec;

	for (int i = 3; i < argc; i++) {
		if (sscanf(argv[i], "quantity=%lu",
			   (unsigned long *)&message->message.order.quantity))
			continue;
		if (sscanf(argv[i], "price=%lu",
			   (unsigned long *)&message->message.order.price))
			continue;
	}

	if (message->message.order.price == 0)
		message->message.order.ord_type = ME_ORDER_MARKET;
	else
		message->message.order.ord_type = ME_ORDER_LIMIT;
}

void build_set_price(MeMessage *message, char *argv[], int argc)
{
	message->msg_type = ME_MESSAGE_SET_MARKET_PRICE;
	message->message.set_market_price = 0;

	for (int i = 3; i < argc; i++) {
		if (sscanf(argv[i], "price=%lu",
			   (unsigned long *)&message->message.set_market_price))
			continue;
	}
}

void build_cancel(MeMessage *message, char *argv[], int argc)
{
	message->msg_type = ME_MESSAGE_CANCEL_ORDER;
	message->message.to_cancel = 0;

	for (int i = 3; i < argc; i++) {
		if (sscanf(argv[i], "id=%u",
			   (unsigned int *)&message->message.to_cancel))
			continue;
	}

	if (message->message.to_cancel == 0) {
		fprintf(stderr,
			"Refusing to send cancellation order to ID 0\n");
		exit(1);
	}
}

void build_panic(MeMessage *message)
{
	message->msg_type = ME_MESSAGE_PANIC;
}

int main(int argc, char *argv[])
{
	MeClientContext context;
	MeMessage message;

	if (argc < 3) {
		printf(help, argv[0], argv[0], argv[0], argv[0]);
		return 1;
	}

	sscanf(argv[1], "%zu", &message.security_id);

	if (!strcmp(argv[2], "buy")) {
		build_order(&message, argv, argc, ME_SIDE_BUY);
	} else if (!strcmp(argv[2], "sell")) {
		build_order(&message, argv, argc, ME_SIDE_SELL);
	} else if (!strcmp(argv[2], "set")) {
		build_set_price(&message, argv, argc);
	} else if (!strcmp(argv[2], "cancel")) {
		build_cancel(&message, argv, argc);
	} else if (!strcmp(argv[2], "panic")) {
		build_panic(&message);
	} else {
		fprintf(stderr, "Unknown message type: %s\n", argv[2]);
		printf(help, argv[0], argv[0], argv[0], argv[0]);
		return 1;
	}

	if (me_client_init_context(&context) != 0) {
		fprintf(stderr,
			"Could not init client context. Is the engine running?\n");
		fprintf(stderr, "Opening queues %s and %s failed ",
			me_in_queue_name, me_out_queue_name);
		perror("with");
		return errno;
	}

	me_client_send_message(&context, &message);
	if (errno) {
		perror("shit: ");
		printf("\n");
	}

	me_client_close_context(&context);

	return 0;
}
