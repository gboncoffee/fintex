#include <errno.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "me.h"

static inline void print_market_order(MeOrder *o, int64_t id) {
  printf("%8ld: NEW ORDER (MARKET): SIDE=%s QUANTITY=%ld ID=%ld\n", id,
         o->side == ME_SIDE_BUY ? "BUY" : "SELL", o->quantity, o->order_id);
}

static inline void print_limit_order(MeOrder *o, int64_t id) {
  printf("%8ld: NEW ORDER (LIMIT): SIDE=%s QUANTITY=%ld PRICE=%ld ID=%ld\n", id,
         o->side == ME_SIDE_BUY ? "BUY" : "SELL", o->quantity, o->price,
         o->order_id);
}

static inline void print_trade(MeTrade *t, int64_t id) {
  MeOrder *ag = &t->aggressor;
  printf("%8ld: TRADE: AGGRESSOR_SIDE=%s QUANTITY=%ld PRICE=%ld ID=%lu MATCHED_ID=%lu\n",
         id, ag->side == ME_SIDE_BUY ? "BUY" : "SELL", ag->quantity, ag->price,
         ag->order_id, t->matched_id);
}

static inline void print_cancel(MeOrderID id, int64_t security_id) {
  printf("%8ld: CANCEL ORDER: ID=%ld\n", security_id, id);
}

static inline void print_executed(MeOrder *order, int64_t id) {
  printf("%8ld: ORDER EXECUTED: ID=%ld\n", id, order->order_id);
}

static inline void print_message(MeMessage *message) {
  switch (message->msg_type) {
    case ME_MESSAGE_PANIC:
      fprintf(stderr, "Engine shutdown via panic. Bailing out.\n");
      exit(0);
    case ME_MESSAGE_SET_MARKET_PRICE:
      printf("%8ld: SET MARKET PRICE: PRICE=%ld\n", message->security_id,
             message->message.set_market_price);
      break;
    case ME_MESSAGE_NEW_ORDER:
      if (message->message.order.ord_type == ME_ORDER_MARKET) {
        print_market_order(&message->message.order, message->security_id);
      } else {
        print_limit_order(&message->message.order, message->security_id);
      }
      break;
    case ME_MESSAGE_TRADE:
      print_trade(&message->message.trade, message->security_id);
      break;
    case ME_MESSAGE_CANCEL_ORDER:
      print_cancel(message->message.to_cancel, message->security_id);
      break;
    case ME_MESSAGE_ORDER_EXECUTED:
      print_executed(&message->message.order, message->security_id);
      break;
  }
}

int main(void) {
  MeClientContext context;
  MeMessage message;
  if (me_client_init_context(&context) != 0) {
    fprintf(stderr, "Could not init client context. Is the engine running?\n");
    fprintf(stderr, "Opening queues %s and %s failed ", me_in_queue_name,
            me_out_queue_name);
    perror("with");
    return errno;
  }

  while (!me_client_get_message(&context, &message)) print_message(&message);

  perror("Retriving message failed");

  return 1;
}
