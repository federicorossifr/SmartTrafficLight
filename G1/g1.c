#include "../common.h"
static struct runicast_conn runicast;
static struct broadcast_conn broadcast;

const linkaddr_t* last_runicast_recv;

int temp_m[4];
bool temp_v[4] = {false,false,false,false};
int last_tmp_avg;
int hum_m[4];
bool hum_v[4] = {false,false,false,false};
int last_hum_avg;

bool pending_request = false;
bool crossing = false;
vehicle_t pending_vehicle;

static bool insert_temp(int temp,const linkaddr_t* sender) {
	int index = -1;
	if(sender == 0) index = 3;
	else index = get_index(sender);
	temp_m[index] = temp;
	temp_v[index] = true;
	return (temp_v[0] & temp_v[1] & temp_v[2] & temp_v[3]);
}

static bool insert_hum(int hum,const linkaddr_t* sender) {
	int index = -1;
	if(sender == 0) index = 3;
	else index = get_index(sender);
	hum_m[index] = hum;
	hum_v[index] = true;
	return (hum_v[0] & hum_v[1] & hum_v[2] & hum_v[3]);	
}


static int compute_temp_average() {
	int avg = (temp_m[0]+temp_m[1]+temp_m[2]+temp_m[3])/4;
	temp_v[0] = temp_v[1] = temp_v[2] = temp_v[3] = false;
	return avg;
}

static int compute_hum_average() {
	int avg = (hum_m[0]+hum_m[1]+hum_m[2]+hum_m[3])/4;
	hum_v[0] = hum_v[1] = hum_v[2] = hum_v[3] = false;
	return avg;
}




static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {
	last_runicast_recv = from;
	//DETECT CORRECT EVENT FROM STRUCT
	//process_post(&runicast_process,TEMP_RECEIVED,packetbuf_dataptr());	
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


PROCESS(keyboard_emergency_process, "SENSE_LIGHT_PROCESS");
PROCESS(sense_traffic_control_process, "TRAFFIC_CONTROL_PROCESS");
AUTOSTART_PROCESSES(&sense_traffic_control_process,&keyboard_emergency_process);

PROCESS_THREAD(sense_traffic_control_process, ev, data) {
	static struct etimer sense_timer;
	static struct etimer second_click_timer;
  	
  	PROCESS_EXITHANDLER(close_all());	
	PROCESS_BEGIN();
	while(true) {
		PROCESS_WAIT_EVENT();
		if(ev == sensors_event && data == &button_sensor) { //PRESSED BUTTON
			if(pending_request) continue;
			etimer_set(&second_click_timer,CLOCK_SECOND*SECOND_CLICK_WAIT);
			PROCESS_WAIT_EVENT();
			if(ev == sensors_event && data == &button_sensor) //BUTTON PRESSED SECOND TIME
				pending_vehicle = EMERGENCY;
			else //BUTTON NOT PRESSED SECOND TIME
				pending_vehicle = NORMAL;
			if(!crossing) { //If a vehicle is not being consiered for crossing send the request
				//SEND BROADCAST
				crossing = true;
			}
			else { // If a vehicle is already considered for crossing enqueue it
				pending_request = true;
			}
		} else if(ev == TEMP_RECEIVED_EVENT) {
			int temp = atoi((char*)data);
			if(insert_temp(temp,last_runicast_recv)) {
				compute_temp_average(); 
				//DISPLAY STRING
			}
		} else if(ev == HUM_RECEIVED_EVENT) {
			int hum = atoi((char*)data);
			if(insert_hum(hum,last_runicast_recv)) {
				compute_hum_average(); 
				//DISPLAY STRING
			}			
		} else if(ev == CROSS_COMPLETED) {//IF PENDING REQUEST SEND IT
			crossing = false;
			if(pending_request) {
				//SEND BROADCAST
				pending_request = false;
			}
		} else if(etimer_expired(&sense_timer)) {
			//SENSE AND INSERT IN ARRAY
			// SENSE HUM AND TEMP 
			// INSERT IN ARRAY
			// EVENTUALLY PRINT STRING
		}



	}
	PROCESS_END();
}

PROCESS_THREAD(keyboard_emergency_process, ev, data) {
	PROCESS_BEGIN();

	PROCESS_END();
}
