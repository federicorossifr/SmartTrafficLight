#include "../common.h"
static struct runicast_conn runicast;
static struct broadcast_conn broadcast;
PROCESS(traffic_sense_light_process, "TRAFFIC_CONTROL_PROCESS");
AUTOSTART_PROCESSES(&traffic_sense_light_process);

int battery = MAX_BATTERY;

bool crossing = false;
bool need_notification = false;
cross_request_t* main_road_req = 0;
cross_request_t* second_road_req = 0;
int me;
linkaddr_t ref_ground;
cross_request_t** ref_request;
cross_request_t** oth_request;

void init() {
	me = whoami();
	ref_ground = (me == TL1_INDEX)?g1:g2;
	ref_request = (me == TL1_INDEX)?&main_road_req:&second_road_req;
	oth_request = (me == TL1_INDEX)?&second_road_req:&main_road_req;
}

static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {
}

static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){
}

static void timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){
}


static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from){
	char* v_type = (char*)packetbuf_dataptr();
	printf("RECEIVED CROSSING REQUEST FROM: %d.%d FOR VEHICLE: %s\n",from->u8[0],from->u8[1],v_type);
	if(get_index(from) == G1_INDEX && !main_road_req) {
		main_road_req = malloc(sizeof(cross_request_t));
		main_road_req->req_v = (strcmp(v_type,"n")==0)?NORMAL:EMERGENCY;
	}
	else if(get_index(from) == G2_INDEX && !second_road_req) {
		second_road_req = malloc(sizeof(cross_request_t));
		second_road_req->req_v = (strcmp(v_type,"n")==0)?NORMAL:EMERGENCY;
	}
	process_post(&traffic_sense_light_process,CROSS_REQUEST,packetbuf_dataptr());			
}

static void broadcast_sent(struct broadcast_conn *c, int status, int num_tx){
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv, broadcast_sent};
static const struct runicast_callbacks runicast_calls = {recv_runicast, sent_runicast, timedout_runicast};

static void close_all() {
	runicast_close(&runicast);
	broadcast_close(&broadcast);
}




PROCESS_THREAD(traffic_sense_light_process, ev, data) {
	static struct etimer led_toggle_timer;
	static struct etimer base_sense_timer;	
	static struct etimer reduced_sense_timer;	
	static struct etimer constrained_sense_timer;	
	static struct etimer cross_timer;
	static bool full_active = true;
	static bool high_active = false;
	static bool low_active = false;

  	PROCESS_EXITHANDLER(close_all());		
	PROCESS_BEGIN();
	SENSORS_ACTIVATE(button_sensor);
	etimer_set(&led_toggle_timer,CLOCK_SECOND*TOGGLE_INTERVAL);
	etimer_set(&base_sense_timer,CLOCK_SECOND*PERIOD_DEFAULT);

	runicast_open(&runicast, 144, &runicast_calls);
	broadcast_open(&broadcast, 129, &broadcast_call);  	
	init();
	while(true) {
		PROCESS_WAIT_EVENT();
		if(etimer_expired(&led_toggle_timer)) {
			if(battery <= THRESHOLD_LOW) do_toggle(&battery,LEDS_BLUE);
			else if(!crossing) do_toggle(&battery,LEDS_GREEN|LEDS_RED);
			etimer_reset(&led_toggle_timer);
		}

		if(etimer_expired(&base_sense_timer) && full_active) {
			if(battery > THRESHOLD_HIGH) {
				do_sense(&runicast,&battery);
				etimer_reset(&base_sense_timer);
			}
		}

		if(etimer_expired(&reduced_sense_timer) && high_active) {
			if(battery <= THRESHOLD_HIGH && battery > THRESHOLD_LOW) {
				do_sense(&runicast,&battery);
				etimer_reset(&reduced_sense_timer);
			}
		}

		if(etimer_expired(&constrained_sense_timer) && low_active) {
			if(battery <= THRESHOLD_LOW && battery > 0) {
				do_sense(&runicast,&battery);
				etimer_reset(&constrained_sense_timer);
			}
		}

		if(ev == sensors_event && data == &button_sensor) {
			if(battery <= THRESHOLD_LOW) {
				shut_leds(&battery);
				battery = 100;
				high_active = low_active = false;
				full_active = true;
				etimer_set(&base_sense_timer,CLOCK_SECOND*PERIOD_DEFAULT);
			}
		}

		if(!high_active && THRESHOLD_LOW < battery && battery <= THRESHOLD_HIGH) {
			etimer_set(&reduced_sense_timer,CLOCK_SECOND*PERIOD_HIGH);
			full_active = false;
			high_active = true;
		}

		if(crossing && etimer_expired(&cross_timer)) {
			crossing = false;
			measurement_t m = {0,0,1};
			packetbuf_copyfrom(&m,sizeof(m));
			if(!runicast_is_transmitting(&runicast))
				runicast_send(&runicast, &ref_ground, MAX_RETRANSMISSIONS);
			shut_leds(&battery);
		}

		if(!low_active && 0 < battery && battery <= THRESHOLD_LOW) {
			etimer_set(&constrained_sense_timer,CLOCK_SECOND*PERIOD_LOW);
			high_active = false;	
			low_active = true;	
		}

		if(main_road_req || second_road_req) {
			if(crossing) continue;
			unsigned char ledv;
			cross_request_t* decided = decide(main_road_req,second_road_req);
			char* v = (decided->req_v == NORMAL)?"normal":(decided->req_v == EMERGENCY)?"emergency":"ERROR";
			if(decided == *ref_request) { 
				ledv = LEDS_GREEN;
				printf("%s VEHICLE ALLOWED CROSSING MAIN STREET\n",v);
				*ref_request = 0;
			} else {
				ledv = LEDS_RED;
				printf("%s VEHICLE ALLOWED CROSSING SECOND STREET\n",v);				
				*oth_request = 0; 
			}
			shut_leds(&battery);
			do_toggle(&battery,ledv);
			crossing = true;
			etimer_set(&cross_timer,CLOCK_SECOND*CROSS_PERIOD);
		}
	}
	PROCESS_END();
}
