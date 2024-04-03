#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <omp.h>
#include "me.h"

void print_message(MeMessage *message)
{
	switch (message->msg_type) {
	case ME_MESSAGE_PANIC:
		fprintf(stderr, "Engine shutdown via panic. Bailing out.\n");
		exit(0);
	case ME_MESSAGE_SET_MARKET_PRICE:
		printf("%8u: SET MARKET PRICE: PRICE=%lu\n", (unsigned int) message->security_id, (unsigned long) message->message.set_market_price);
		break;
	case B3_MESSAGE_TYPE_SIMPLE_NEW_ORDER:
	case B3_MESSAGE_TYPE_NEW_ORDER_SINGLE:
		if (message->order.ord_type == B3_ORD_MARKET)
			print_market_order(&message);
		else
			print_limit_order(&message);
		break;
	/* TODO */
	}
}

int main(void)
{
	MeClientContext context;
	MeMessage message;
	if (me_client_init_context(&context) != 0) {
		fprintf(stderr, "Could not init client context. Is the engine running?\n");
		fprintf(stderr, "Opening queues %s and %s failed ", me_in_queue_name, me_out_queue_name);
		perror("with");
		return errno;
	}

	while (!me_client_get_message(&context, &message))
		print_message(&message);

	perror("Retriving message failed");

	return 1;
}
