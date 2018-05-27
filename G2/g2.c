#include "../common.h"
static struct runicast_conn runicast;
static struct broadcast_conn broadcast;

PROCESS(sense_traffic_control_process, "G2_SENSING_TRAFFIC_PROCESS");
AUTOSTART_PROCESSES(&sense_traffic_control_process);

vehicle_t pending_vehicle;

static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {}
static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){}
static void timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){}
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from){}
static void broadcast_sent(struct broadcast_conn *c, int status, int num_tx){}
static const struct broadcast_callbacks broadcast_calls = {broadcast_recv, broadcast_sent};
static const struct runicast_callbacks runicast_calls = {recv_runicast, sent_runicast, timedout_runicast};

static void close_all() {
	runicast_close(&runicast);
	broadcast_close(&broadcast);
}

/*
	THIS PROCESS HANDLES:
	1) SENSING
	2) TRAFFIC COMMUNICATION TO TLs
*/
PROCESS_THREAD(sense_traffic_control_process, ev, data) {
	static struct etimer second_click_timer;
	static struct etimer base_sense_timer;
  	PROCESS_EXITHANDLER(close_all());	
	PROCESS_BEGIN();
	SENSORS_ACTIVATE(button_sensor);
	SENSORS_ACTIVATE(sht11_sensor);
	runicast_open(&runicast, 144, &runicast_calls);
	broadcast_open(&broadcast, 129, &broadcast_calls);  	
	printf("I AM NODE: %d\n",whoami());	
	etimer_set(&base_sense_timer,CLOCK_SECOND*PERIOD_DEFAULT);
	while(true) {
		PROCESS_WAIT_EVENT();
		if(ev == sensors_event && data == &button_sensor) {
			leds_on(LEDS_RED);
			etimer_set(&second_click_timer,CLOCK_SECOND*SECOND_CLICK_WAIT);
			PROCESS_WAIT_EVENT();
			if(ev == sensors_event && data == &button_sensor) {
				leds_off(LEDS_ALL);
				leds_on(LEDS_BLUE);
				pending_vehicle = EMERGENCY;
				printf("EMERGENCY ON SECOND!!\n");
				etimer_stop(&second_click_timer);
		    }
			else {
				pending_vehicle = NORMAL; 
			}
			leds_off(LEDS_ALL);
			char* v_type = (pending_vehicle == NORMAL)?"n":"e";
			packetbuf_copyfrom(v_type,sizeof(char)*(strlen(v_type)+1));	
			broadcast_send(&broadcast);				
		}

		if(etimer_expired(&base_sense_timer)) {
			do_sense(&runicast,0);
			etimer_reset(&base_sense_timer);
		}
	}
	PROCESS_END();
}
