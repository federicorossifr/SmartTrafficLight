#include "../common.h"
static struct runicast_conn runicast;
static struct broadcast_conn broadcast;
PROCESS(traffic_sense_light_process, "TRAFFIC_CONTROL_PROCESS");
AUTOSTART_PROCESSES(&traffic_sense_light_process);

int battery = MAX_BATTERY;

bool crossing = false;
cross_request_t* main_road_req = 0;
cross_request_t* second_road_req = 0;

static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {
}

static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){
}

static void timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){
}


static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from){
	if(get_index(from) == 2) //REQUEST FROM G1
		main_road_req = (cross_request_t*)packetbuf_dataptr();
	else if(get_index(from) == 4)
		second_road_req = (cross_request_t*)packetbuf_dataptr();
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

static void do_toggle(unsigned char ledv) {
	if(battery > 20)
		battery=max(0,battery-ON_TOGGLE_DRAIN);		
	leds_toggle(ledv);
}

static void do_sense() {
	SENSORS_ACTIVATE(sht11_sensor);		
	battery=max(0,battery-ON_SENSE_DRAIN);
	int tmp = (sht11_sensor.value(SHT11_SENSOR_TEMP)/10-396)/10;
	int hum = sht11_sensor.value(SHT11_SENSOR_HUMIDITY)/41;
	measurement_t m = {tmp,hum};
	packetbuf_copyfrom(&m,sizeof(measurement_t));
	runicast_send(&runicast, &g1, MAX_RETRANSMISSIONS);
	SENSORS_DEACTIVATE(sht11_sensor);		
	printf("SENSE DONE %d\n",battery);
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
	while(true) {
		PROCESS_WAIT_EVENT();
		if(etimer_expired(&led_toggle_timer)) {
			if(battery <= THRESHOLD_LOW) do_toggle(LEDS_BLUE);
			else if(!crossing) do_toggle(LEDS_GREEN|LEDS_RED);
			etimer_reset(&led_toggle_timer);
		}

		if(etimer_expired(&base_sense_timer) && full_active) {
			if(battery > THRESHOLD_HIGH) {
				do_sense();
				etimer_reset(&base_sense_timer);
			}
		}
		if(etimer_expired(&reduced_sense_timer) && high_active) {
			if(battery <= THRESHOLD_HIGH && battery > THRESHOLD_LOW) {
				do_sense();
				etimer_reset(&reduced_sense_timer);
			}
		}
		if(etimer_expired(&constrained_sense_timer) && low_active) {
			if(battery <= THRESHOLD_LOW && battery > 0) {
				do_sense();
				etimer_reset(&constrained_sense_timer);
			}
		}

		if(ev == sensors_event && data == &button_sensor) {
			if(battery <= THRESHOLD_LOW) {
				battery = 100;
				high_active = low_active = false;
				full_active = true;
				etimer_set(&base_sense_timer,CLOCK_SECOND*PERIOD_DEFAULT);
			}
		}

		if(ev == CROSS_REQUEST) {
		} 

		if(!high_active && THRESHOLD_LOW < battery && battery <= THRESHOLD_HIGH) {
			etimer_set(&reduced_sense_timer,CLOCK_SECOND*PERIOD_HIGH);
			full_active = false;
			high_active = true;
		}
		if(!low_active && 0 < battery && battery <= THRESHOLD_LOW) {
			etimer_set(&constrained_sense_timer,CLOCK_SECOND*PERIOD_LOW);
			high_active = false;	
			low_active = true;	
		}
	}
	PROCESS_END();
}
