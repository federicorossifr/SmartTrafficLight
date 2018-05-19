#include "../common.h"
static struct runicast_conn runicast;
static struct broadcast_conn broadcast;

const linkaddr_t* last_runicast_recv;

PROCESS(sense_traffic_control_process, "SENSE_TRAFFIC_CONTROL_PROCESS");
AUTOSTART_PROCESSES(&sense_traffic_control_process);
bool pending_request = false;
bool crossing = false;
vehicle_t pending_vehicle;
static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {
	last_runicast_recv = from;
	//ALL RUNICAST MESSAGE ARE OF TYPE measurement_t
	//IF is_cross FIELD is equal to 1 it is a CROSS ACK FROM attached TL sensor
	measurement_t* m = (measurement_t*)packetbuf_dataptr();
	if(m->is_cross) {
		process_post(&sense_traffic_control_process,CROSS_COMPLETED,packetbuf_dataptr());			
	} else {
		//THIS WILL NEVER FIRE BUT WHO KNOWS
		process_post(&sense_traffic_control_process,VAL_RECEIVED_EVENT,packetbuf_dataptr());							
	}
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



PROCESS_THREAD(sense_traffic_control_process, ev, data) {
	static struct etimer second_click_timer;
	static struct etimer base_sense_timer;
  	
  	PROCESS_EXITHANDLER(close_all());	
	PROCESS_BEGIN();
	SENSORS_ACTIVATE(button_sensor);
	SENSORS_ACTIVATE(sht11_sensor);
	runicast_open(&runicast, 144, &runicast_calls);
	broadcast_open(&broadcast, 129, &broadcast_call);  	
	etimer_set(&base_sense_timer,CLOCK_SECOND*PERIOD_DEFAULT);
	while(true) {
		//printf("WAITING\n");
		PROCESS_WAIT_EVENT();
		
		if(ev == sensors_event && data == &button_sensor) { //PRESSED BUTTON
			if(pending_request) continue;
			etimer_set(&second_click_timer,CLOCK_SECOND*SECOND_CLICK_WAIT*2);
			PROCESS_WAIT_EVENT();
			if(ev == sensors_event && data == &button_sensor) {//BUTTON PRESSED SECOND TIME
				pending_vehicle = EMERGENCY;
				printf("EMERGENCY ON SECOND!!\n");
		    }
			else if(etimer_expired(&second_click_timer)){ //BUTTON NOT PRESSED SECOND TIME
				//printf("BUTTON PRESSED 1 TIME\n");
				pending_vehicle = NORMAL;
			}
			if(!crossing) { //If a vehicle is not being consiered for crossing send the request
				cross_request_t req = {pending_vehicle,MAIN};
				char* v_type = (pending_vehicle == NORMAL)?"n":"e";
				packetbuf_copyfrom(v_type,sizeof(char)*(strlen(v_type)+1));	
				broadcast_send(&broadcast);				
				crossing = true;
			}
			else { // If a vehicle is already considered for crossing enqueue it
				//printf("A VEHICLE IS ALREADY CROSSING, SAVING THE REQUEST FOR LATER\n");
				pending_request = true;
			}
		} else if(ev == CROSS_COMPLETED) {//IF PENDING REQUEST SEND IT
			crossing = false;
			if(pending_request) {
				cross_request_t req = {pending_vehicle,MAIN};				
				packetbuf_copyfrom(&req,sizeof(cross_request_t));				
				broadcast_send(&broadcast);								
				pending_request = false;
			}
		}

		if(etimer_expired(&base_sense_timer)) {
			do_sense(&runicast,&battery);
			etimer_reset(&base_sense_timer);
		}
	}
	PROCESS_END();
}


