// Exercise the real queue/encoder with a simulated panel and virtual time.
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

#ifndef PROGRAMMER_SOURCE
#define PROGRAMMER_SOURCE "../source/iaqtouch_aq_programmer.c"
#endif
#include PROGRAMMER_SOURCE

static unsigned int elapsed_ms;
static unsigned int consume_at_ms;
static unsigned char received[AQ_MAXPKTLEN_SEND];
static int received_len;
static pthread_barrier_t race_barrier;
static bool race_on_timeout;

static int take_command(unsigned char *dest)
{
#ifdef TEST_ORIGINAL
  unsigned char *src;
  int len = ref_iaqt_control_cmd(&src);
  memcpy(dest, src, len);
  rem_iaqt_control_cmd(src);
  return len;
#else
  return pop_iaqt_control_cmd(dest);
#endif
}

void LOG(const logmask_t from, const int level, const char *format, ...) {}
int getLogLevel(logmask_t from) { return LOG_ERR; }
int beautifyPacket(char *b, int size, const unsigned char *p, int len, bool read)
{ return 0; }

void delay(unsigned int ms)
{
  elapsed_ms += ms;
  if (consume_at_ms && elapsed_ms >= consume_at_ms) {
    consume_at_ms = 0;
    assert(pop_iaqt_cmd(CMD_IAQ_POLL) == ACK_CMD_READY_CTRL);
    received_len = take_command(received);
  }
  if (race_on_timeout && elapsed_ms == 5000)
    pthread_barrier_wait(&race_barrier);
}

static void reset_clock(void)
{
  elapsed_ms = 0;
  consume_at_ms = 0;
  received_len = 0;
}

static void expect_rpm(const unsigned char *packet, int len, const char *rpm)
{
  assert(len == 19);
  assert(packet[0] == DEV_MASTER && packet[1] == 0x24 && packet[2] == 0x31);
  assert(memcmp(packet + 3, rpm, 4) == 0);
}

static void *late_panel(void *unused)
{
  pthread_barrier_wait(&race_barrier);
  received_len = take_command(received);
  return NULL;
}

int main(void)
{
  unsigned char copy[AQ_MAXPKTLEN_SEND];
  set_iaq_cansend(true);

  // The actual incident: announcement sent, panel's ready response lost.
  queue_iaqt_control_command(icct_setrpm, 1315);
  assert(pop_iaqt_cmd(CMD_IAQ_POLL) == ACK_CMD_READY_CTRL);
  assert(!waitfor_iaqt_ctrl_queue2empty());
  assert(elapsed_ms == 5000);
  if (_iaqt_control_cmd_len != 0) {
    fprintf(stderr, "FAIL: timed-out command still blocks the queue\n");
    return 1;
  }
  assert(take_command(copy) == 0); // late ready must not send stale RPM

  reset_clock();
  queue_iaqt_control_command(icct_setrpm, 3450);
  consume_at_ms = 50;
  assert(waitfor_iaqt_ctrl_queue2empty());
  expect_rpm(received, received_len, "3450");

  // Also cancel a ready announcement that was never polled by the panel.
  reset_clock();
  queue_iaqt_control_command(icct_setrpm, 1315);
  assert(!waitfor_iaqt_ctrl_queue2empty());
  assert(pop_iaqt_cmd(CMD_IAQ_POLL) == NUL);
  assert(take_command(copy) == 0);

  // A late ready before the next announcement cannot consume the new payload.
  queue_iaqt_control_command(icct_setrpm, 3450);
  assert(take_command(copy) == 0);
  assert(pop_iaqt_cmd(CMD_IAQ_POLL) == ACK_CMD_READY_CTRL);
  int len = take_command(copy);
  expect_rpm(copy, len, "3450");
  assert(take_command(received) == 0); // duplicate ready

  // Reusing/expiring the shared queue cannot mutate a consumer's local copy.
  reset_clock();
  queue_iaqt_control_command(icct_setrpm, 1315);
  assert(!waitfor_iaqt_ctrl_queue2empty());
  expect_rpm(copy, len, "3450");

  // Expiration must not remove an unrelated navigation command.
  queue_iaqt_control_command(icct_setrpm, 1315);
  assert(pop_iaqt_cmd(CMD_IAQ_POLL) == ACK_CMD_READY_CTRL);
  assert(iaqt_queue_cmd(KEY_IAQTCH_HOME));
  assert(!waitfor_iaqt_ctrl_queue2empty());
  assert(pop_iaqt_cmd(CMD_IAQ_POLL) == KEY_IAQTCH_HOME);

#ifndef TEST_ORIGINAL
  // Discovering a stale command while enqueueing aborts that operation, too.
  queue_iaqt_control_command(icct_setrpm, 1315);
  assert(!queue_iaqt_control_command(icct_setrpm, 3450));
  assert(take_command(copy) == 0);
  assert(queue_iaqt_control_command(icct_setrpm, 3450));
  reset_clock();
  consume_at_ms = 5000; // ready arriving at the timeout boundary succeeds
  assert(waitfor_iaqt_ctrl_queue2empty());
  expect_rpm(received, received_len, "3450");
#endif

  // The same queue carries date/time payloads.
  reset_clock();
  assert(queue_iaqt_control_command_str(icct_setdate, "09/21/26"));
  assert(!waitfor_iaqt_ctrl_queue2empty());
  assert(queue_iaqt_control_command_str(icct_settime, "09:45"));
  assert(pop_iaqt_cmd(CMD_IAQ_POLL) == ACK_CMD_READY_CTRL);
  assert(take_command(copy) == 19);
  assert(memcmp(copy + 3, "09:45", 5) == 0);
  assert(copy[9] == 0x30 && copy[10] == 0x32 && copy[11] == 0);

  // Race the serial consumer against expiry; accept a whole command or none.
  pthread_barrier_init(&race_barrier, NULL, 2);
  for (int i = 0; i < 1000; ++i) {
    reset_clock();
    queue_iaqt_control_command(icct_setrpm, 3450);
    assert(pop_iaqt_cmd(CMD_IAQ_POLL) == ACK_CMD_READY_CTRL);
    pthread_t panel;
    race_on_timeout = true;
    assert(pthread_create(&panel, NULL, late_panel, NULL) == 0);
    bool sent = waitfor_iaqt_ctrl_queue2empty();
    pthread_join(panel, NULL);
    race_on_timeout = false;
    assert(sent == (received_len > 0));
    if (sent)
      expect_rpm(received, received_len, "3450");
    assert(take_command(copy) == 0);
  }
  pthread_barrier_destroy(&race_barrier);
  puts("PASS: lost response recovery, cancellation, late/duplicate responses,");
  puts("payload ownership, timeout boundary, date/time and 1000 expiry races");
  return 0;
}
