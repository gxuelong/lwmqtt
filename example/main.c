#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/client.h"
#include "unix.h"

char *topic = "hello";

#define PAYLOAD_LEN 256
char payload[PAYLOAD_LEN + 1];

volatile int counter;

static void message_arrived(lwmqtt_client_t *c, lwmqtt_string_t *t, lwmqtt_message_t *m) {
  if (lwmqtt_strcmp(t, topic) != 0) {
    printf("topic does not match\n");
    exit(1);
  }

  if (strncmp(payload, m->payload, (size_t)m->payload_len) != 0) {
    printf("payload does not match\n");
    exit(1);
  }

  counter++;
}

static void test(lwmqtt_qos_t qos) {
  unsigned char buf1[512], buf2[512];

  lwmqtt_unix_network_t n;
  lwmqtt_unix_timer_t t1, t2;

  lwmqtt_client_t c = lwmqtt_default_client;

  lwmqtt_init(&c, buf1, 512, buf2, 512);

  lwmqtt_set_network(&c, &n, lwmqtt_unix_network_read, lwmqtt_unix_network_write);
  lwmqtt_set_timers(&c, &t1, &t2, lwmqtt_unix_timer_set, lwmqtt_unix_timer_get);
  lwmqtt_set_callback(&c, message_arrived);

  int rc = lwmqtt_unix_network_connect(&n, "127.0.0.1", 1883);
  if (rc != LWMQTT_SUCCESS) {
    printf("failed lwmqtt_unix_network_connect: %d\n", rc);
    exit(1);
  }

  lwmqtt_options_t data = lwmqtt_default_options;
  data.client_id.c_string = "lwmqtt";

  rc = lwmqtt_connect(&c, &data, NULL, NULL, 1000);
  if (rc != LWMQTT_SUCCESS) {
    printf("failed lwmqtt_connect: %d\n", rc);
    exit(1);
  }

  rc = lwmqtt_subscribe(&c, "hello", qos, 1000);
  if (rc != LWMQTT_SUCCESS) {
    printf("failed lwmqtt_subscribe: %d\n", rc);
    exit(1);
  }

  counter = 0;

  while (counter < 5) {
    lwmqtt_message_t msg = lwmqtt_default_message;
    msg.qos = qos;
    msg.payload = payload;
    msg.payload_len = PAYLOAD_LEN;

    rc = lwmqtt_publish(&c, "hello", &msg, 1000);
    if (rc != LWMQTT_SUCCESS) {
      printf("failed lwmqtt_publish: %d (%d)\n", rc, counter);
      exit(1);
    }

    rc = lwmqtt_yield(&c, 10);
    if (rc != LWMQTT_SUCCESS) {
      printf("failed lwmqtt_yield: %d (%d)\n", rc, counter);
      exit(1);
    }
  }

  rc = lwmqtt_unsubscribe(&c, "hello", 1000);
  if (rc != LWMQTT_SUCCESS) {
    printf("failed lwmqtt_unsubscribe: %d\n", rc);
    exit(1);
  }

  rc = lwmqtt_disconnect(&c, 1000);
  if (rc != LWMQTT_SUCCESS) {
    printf("failed lwmqtt_disconnect: %d\n", rc);
    exit(1);
  }

  lwmqtt_unix_network_disconnect(&n);
}

int main() {
  for (int i = 0; i < PAYLOAD_LEN; i++) {
    payload[i] = 'x';
  }

  payload[PAYLOAD_LEN] = 0;

  printf("Running QoS 0 tests...\n");
  test(LWMQTT_QOS0);

  printf("Running QoS 1 tests...\n");
  test(LWMQTT_QOS1);

  printf("Running QoS 2 tests...\n");
  test(LWMQTT_QOS2);

  printf("Finished all tests.\n");

  return 0;
}
