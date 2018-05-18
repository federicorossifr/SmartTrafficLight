
#include "../common.h"
static struct runicast_conn runicast;
static struct broadcast_conn broadcast;

static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {
}

static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){
}

static void timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){
}


static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from){
}

static void broadcast_sent(struct broadcast_conn *c, int status, int num_tx){
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv, broadcast_sent};
static const struct runicast_callbacks runicast_calls = {recv_runicast, sent_runicast, timedout_runicast};

static void close_all() {
	runicast_close(&runicast);
	broadcast_close(&broadcast);
}


PROCESS(sense_light_process, "SENSE_LIGHT_PROCESS");
PROCESS(traffic_control_process, "TRAFFIC_CONTROL_PROCESS");
AUTOSTART_PROCESSES(&sense_light_process,&traffic_control_process);

PROCESS_THREAD(sense_light_process, ev, data) {
  	PROCESS_EXITHANDLER(close_all());		
	PROCESS_BEGIN();

	PROCESS_END();
}

PROCESS_THREAD(traffic_control_process, ev, data) {
  	PROCESS_EXITHANDLER(close_all());		
	PROCESS_BEGIN();

	PROCESS_END();
}

