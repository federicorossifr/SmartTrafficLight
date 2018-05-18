#include "../common.h"
static struct runicast_conn runicast;
static struct broadcast_conn broadcast;

const linkaddr_t* last_runicast_recv;

PROCESS(keyboard_emergency_process, "SENSE_LIGHT_PROCESS");
PROCESS(sense_traffic_control_process, "TRAFFIC_CONTROL_PROCESS");
AUTOSTART_PROCESSES(&sense_traffic_control_process,&keyboard_emergency_process);

int temp_m[4];
int last_tmp_avg; 
int hum_m[4];
bool inserted[4] = {false,false,false,false};
int last_hum_avg; 
int samples = 0;

bool pending_request = false;
bool crossing = false;
vehicle_t pending_vehicle;

char* emergency_message = 0;

static void display_string() {
	char* em_msg = (emergency_message!=0)?emergency_message:"";
	printf("%s + %d + %d\n",em_msg,last_tmp_avg,last_hum_avg);
	free(emergency_message); emergency_message = 0;
}

static void insert_measurement(measurement_t measurement,const linkaddr_t* sender) {
	int index = -1;
	if(sender == 0) index = 2;
	else index = get_index(sender);
	temp_m[index] = measurement.temperature;
	hum_m[index] = measurement.humidity;
	if(!inserted[index]) {
		samples++;
		inserted[index] = true;
	}
}

static void compute_averages() {
	last_tmp_avg = (temp_m[0]+temp_m[1]+temp_m[2]+temp_m[3])/4;
	last_hum_avg = (hum_m[0]+hum_m[1]+hum_m[2]+hum_m[3])/4;	
	samples=0;
	inserted[0] = inserted[1] = inserted[2] = inserted[3] = false;
}



static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {
	last_runicast_recv = from;
	//ALL RUNICAST MESSAGE ARE OF TYPE measurement_t
	//IF is_cross FIELD is equal to 1 it is a CROSS ACK FROM attached TL sensor
	measurement_t* m = (measurement_t*)packetbuf_dataptr();
	if(m->is_cross) {
		process_post(&sense_traffic_control_process,CROSS_COMPLETED,packetbuf_dataptr());			
	} else {
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
  	
  	PROCESS_EXITHANDLER(close_all());	
	PROCESS_BEGIN();
	SENSORS_ACTIVATE(button_sensor);
	SENSORS_ACTIVATE(sht11_sensor);
	runicast_open(&runicast, 144, &runicast_calls);
	broadcast_open(&broadcast, 129, &broadcast_call);  	
	while(true) {
		//printf("WAITING\n");
		PROCESS_WAIT_EVENT();
		
		if(ev == sensors_event && data == &button_sensor) { //PRESSED BUTTON
			//printf("BUTTON PRESSED\n");			
			if(pending_request) continue;
			etimer_set(&second_click_timer,CLOCK_SECOND*SECOND_CLICK_WAIT);
			PROCESS_WAIT_EVENT();
			if(ev == sensors_event && data == &button_sensor) {//BUTTON PRESSED SECOND TIME
				pending_vehicle = EMERGENCY;
				//printf("BUTTON PRESSED 2 TIMES\n");
		    }
			else { //BUTTON NOT PRESSED SECOND TIME
				//printf("BUTTON PRESSED 1 TIME\n");
				pending_vehicle = NORMAL;
			}
			if(!crossing) { //If a vehicle is not being consiered for crossing send the request
				cross_request_t req = {pending_vehicle,MAIN};
				packetbuf_copyfrom(&req,sizeof(cross_request_t));	
				printf("SENDING BROADCAST REQUEST TO TLs\n");
				broadcast_send(&broadcast);				
				crossing = true;
			}
			else { // If a vehicle is already considered for crossing enqueue it
				pending_request = true;
			}
		} else if(ev == VAL_RECEIVED_EVENT) {
			measurement_t* measurement = (measurement_t*)data;
			printf("Received temperature %d\n",measurement->temperature);
			insert_measurement(*measurement,last_runicast_recv);
			if(samples == 3) {
				int tmp = (sht11_sensor.value(SHT11_SENSOR_TEMP)/10-396)/10;
				int hum = sht11_sensor.value(SHT11_SENSOR_HUMIDITY)/41;
				measurement_t m = {tmp,hum};
				insert_measurement(m,0);
				compute_averages();
				display_string();
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
	}
	PROCESS_END();
}

PROCESS_THREAD(keyboard_emergency_process, ev, data) {
	int correct;

	PROCESS_BEGIN();

	correct = 0;
	SENSORS_ACTIVATE(button_sensor);
	//uart1_set_input(serial_line_input_byte); IT GIVES A WARNING INSTEAD
	serial_line_init();

	while(1){
		while(correct == 0){
			printf("Please, type the password\n");
			PROCESS_WAIT_EVENT_UNTIL(ev==serial_line_event_message);
			if(strcmp((char *)data, "NES") != 0){
				printf("Incorrect password.\n");
			} else {
				correct=1;
			}
		}
		printf("Type the emergency warning\n");
		PROCESS_WAIT_EVENT_UNTIL(ev==serial_line_event_message);
		char* em = (char*)data;
		if(strlen(em) == 0) continue;
		emergency_message = malloc(sizeof(char)*(strlen(em)+1));
		strcpy(emergency_message,em);
		printf("Emergency message set to %s\n",emergency_message);
		correct = 0;
	}

	PROCESS_END();
}
