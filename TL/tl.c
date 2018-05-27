#define TL
#include "../common.h"
static struct runicast_conn runicast;
static struct broadcast_conn broadcast;
PROCESS(traffic_sense_light_process, "TL_TRAFFIC_CONTROL_PROCESS");
AUTOSTART_PROCESSES(&traffic_sense_light_process);

int battery = MAX_BATTERY;
bool crossing = false;					// INDICATES WHETHER A VEHICLE IS CROSSING THE EXCHANGE OR NOT
cross_request_t* main_road_req = 0;		// POINTER TO NEXT REQUEST FROM MAIN ROAD FOR SCHEDULING
cross_request_t* second_road_req = 0;	// POINTER TO NEXT REQUEST FROM SECOND ROAD FOR SCHEDULING
int me;									// VIRTUAL ADDRESS OF THIS NODE INSIDE THE NETWORK (0 FOR TL1, 2 FOR TL2)
linkaddr_t ref_ground;					// ADDRESS TO THE RELATIVE GROUND MOTE (G1 FOR TL1, G2 FOR TL2)
cross_request_t** ref_request;			// POINTER TO THE REQUEST-POINTER COMING FROM RELATIVE GROUND MOTE  (main_road_req for TL1, second_road_req for TL2)
cross_request_t** oth_request;			// POINTER TO THE OTHER REQUEST-POINTER (main_road_req for TL2, second_road_req for TL1)

/*
	INITIALIZE THE NODE DETECTING IF IT IS TL1 OR TL2 BASING ON VIRTUAL ADDRESS
*/
void init() {
	me = whoami();
	ref_ground = (me == TL1_INDEX)?g1:g2;
	ref_request = (me == TL1_INDEX)?&main_road_req:&second_road_req;
	oth_request = (me == TL1_INDEX)?&second_road_req:&main_road_req;
}

/*
	PROCESS THE PENDING REQUESTS, IF THERE ANY.
*/
void process_requests(struct etimer* cross_timer) {
	if(!main_road_req && !second_road_req) return;
	if(crossing) return;
	unsigned char ledv;
	cross_request_t* decided = decide(main_road_req,second_road_req);
	char* v = (decided->req_v == NORMAL)?"normal":(decided->req_v == EMERGENCY)?"emergency":"ERROR";
	char* s = (decided->req_r == MAIN)?"main":(decided->req_r == SECOND)?"second":"ERROR";
	// IF THE SCHEDULED REQUEST IS COMING FROM MY ROAD SHOW GREEN LIGHT
	if(decided == *ref_request) {  
		ledv = LEDS_GREEN;
		printf("%s VEHICLE ALLOWED CROSSING %s STREET\n",v,s);
		free(*ref_request);
		*ref_request = 0;
	// OTHERWISE SHOW RED LIGHT
	} else {	
		ledv = LEDS_RED;
		printf("%s VEHICLE ALLOWED CROSSING %s STREET\n",v,s);	
		free(*oth_request);			
		*oth_request = 0; 
	}
	shut_leds(&battery);
	do_toggle(&battery,ledv);
	crossing = true;
	etimer_set(cross_timer,CLOCK_SECOND*CROSS_PERIOD);
}

static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {}
static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){}
static void timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){}
static void broadcast_sent(struct broadcast_conn *c, int status, int num_tx){}
/*
	A BROADCAST MESSAGE MEANS A CROSSING REQUEST FROM EITHER G1 OR G2. 
	WE ALLOW AT MOST ONE REQUEST IN QUEUE, SO WE'LL DROP OTHER REQUEST
	COMING WHEN QUEUE IS FULL - THAT IS, POINTER TO REQUEST NOT NULL
*/
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from){
	char* v_type = (char*)packetbuf_dataptr();
	printf("RECEIVED CROSSING REQUEST FROM: %d.%d FOR VEHICLE: %s\n",from->u8[0],from->u8[1],v_type);
	if(get_index(from) == G1_INDEX && main_road_req) printf("IGNORED, THERE'S ALREADY ONE FOR MAIN\n");
	if(get_index(from) == G2_INDEX && second_road_req) printf("IGNORED, THERE'S ALREADY ONE FOR SECOND\n");
	if(get_index(from) == G1_INDEX && !main_road_req) {
		main_road_req = malloc(sizeof(cross_request_t));
		main_road_req->req_v = (strcmp(v_type,"n")==0)?NORMAL:EMERGENCY;
		main_road_req->req_r = MAIN;
	}
	else if(get_index(from) == G2_INDEX && !second_road_req) {
		second_road_req = malloc(sizeof(cross_request_t));
		second_road_req->req_v = (strcmp(v_type,"n")==0)?NORMAL:EMERGENCY;
		second_road_req->req_r = SECOND;
	}
	process_post(&traffic_sense_light_process,CROSS_REQUEST,packetbuf_dataptr());			
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv, broadcast_sent};
static const struct runicast_callbacks runicast_calls = {recv_runicast, sent_runicast, timedout_runicast};

/*
	UTILITY FUNCTION TO CLOSE BOTH RUNICAST AND BROADCAST 
	ON EXITHANDLER
*/
static void close_all() {
	runicast_close(&runicast);
	broadcast_close(&broadcast);
}
/*
	THIS PROCESS HANDLES:
	1) SENSING AND BATTERY-RELATED TIMING
	2) TRAFFIC MANAGEMENT AND CROSSING TIMING
*/
PROCESS_THREAD(traffic_sense_light_process, ev, data) {
	static struct etimer led_toggle_timer;
	static struct etimer sense_timer;	
	static struct etimer cross_timer;
	static bool high_active = false;
	static bool low_active = false;
	static int sensing_period = PERIOD_DEFAULT;
  	PROCESS_EXITHANDLER(close_all());		
	PROCESS_BEGIN();
	SENSORS_ACTIVATE(button_sensor);
	etimer_set(&led_toggle_timer,CLOCK_SECOND*TOGGLE_INTERVAL);
	etimer_set(&sense_timer,CLOCK_SECOND*PERIOD_DEFAULT);
	runicast_open(&runicast, 144, &runicast_calls);
	broadcast_open(&broadcast, 129, &broadcast_call);  	
	printf("I AM NODE: %d\n",whoami());	
	init();
	while(true) {
		PROCESS_WAIT_EVENT();

		// 1 SECOND TIMER EXPIRATION FOR LED TOGGLING
		if(etimer_expired(&led_toggle_timer)) {
			if(battery <= THRESHOLD_LOW) do_toggle(&battery,LEDS_BLUE);
			if(!crossing) do_toggle(&battery,LEDS_GREEN|LEDS_RED);
			etimer_reset(&led_toggle_timer);
		}

		// TIMER EXPIRATION FOR SENSING, PERIOD MAY VARY DURING EXECUTION
		// DUE TO BATTERY LEVELS
		if(etimer_expired(&sense_timer) && battery > 0) {
			do_sense(&runicast,&battery);
			etimer_set(&sense_timer,CLOCK_SECOND*sensing_period);
		}

		// BUTTON CLICK RECHARGE BATTERY ONLY IF UNDER LOW LEVEL
		if(ev == sensors_event && data == &button_sensor) {
			if(battery <= THRESHOLD_LOW) {
				shut_leds_val(&battery,LEDS_BLUE);
				battery = MAX_BATTERY;
				sensing_period = PERIOD_DEFAULT;
				etimer_set(&sense_timer,CLOCK_SECOND*sensing_period);
			}
		}

		// IF BATTERY GOES UNDER FIRST THRESHOLD INCREASE SENSING PERIOD TO "PERIOD_HIGH"
		if(!high_active && THRESHOLD_LOW < battery && battery <= THRESHOLD_HIGH) {
			sensing_period = PERIOD_HIGH;
			etimer_stop(&sense_timer);			
			etimer_set(&sense_timer,CLOCK_SECOND*sensing_period);
			high_active = true;
		}

		// IF BATTERY GOES UNDER SECOND THRESHOLD INCREASE SENING PERIOD TO "PERIOD_LOW"
		if(!low_active && 0 < battery && battery <= THRESHOLD_LOW) {
			sensing_period = PERIOD_LOW;		
			etimer_stop(&sense_timer);
			etimer_set(&sense_timer,CLOCK_SECOND*sensing_period);
			high_active = false;	
			low_active = true;	
		}

		// IF BATTERY IS DISCHARGED TO NOT SENSE ANYMORE
		if(low_active && battery == 0) {
			etimer_stop(&sense_timer);
			low_active = false;
		}

		// CROSS COMPLETED
		if(crossing && etimer_expired(&cross_timer)) {
			crossing = false;
			shut_leds(&battery);
		}

		// PROCESS REQUESTS UPON RECEIVING ANY EVENT
		// !!! NOTE THAT INSIDE THE FUNCTION WE FILTER EVENT OCCURING WHEN CROSSING IS ACTIVE !!!
		process_requests(&cross_timer);
	}
	PROCESS_END();
}