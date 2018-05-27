#define SINK
#include "../common.h"
static struct runicast_conn runicast;
static struct broadcast_conn broadcast;

PROCESS(keyboard_emergency_process, "G1_KEYBOARD_PROCESS");
PROCESS(sense_traffic_control_process, "G1_SENSING_TRAFFIC_PROCESS");
AUTOSTART_PROCESSES(&sense_traffic_control_process,&keyboard_emergency_process);

int temp_m[4];
int last_tmp_avg; 
int hum_m[4];
bool inserted_temp[4] = {false,false,false,false};
bool inserted_hum[4] = {false,false,false,false};
int last_hum_avg; 
int samples_temp = 0;
int samples_hum = 0;

bool pending_request = false;
vehicle_t pending_vehicle;

char* emergency_message = 0;

static void display_string() {
	char* em_msg = (emergency_message!=0)?emergency_message:"";
	printf("%s + %d + %d\n",em_msg,last_tmp_avg,last_hum_avg);
}

/*
	INSERT MEASURE IN ARRAY OF MEASURES INDEXED BY THE VIRTUAL INDEX OF THE SENDER
*/
static void insert_measurement(measurement_t measurement,const linkaddr_t* sender) {
	int index = -1;
	if(sender == 0) index = G1_INDEX;
	else index = get_index(sender);
	temp_m[index] = measurement.temperature;
	hum_m[index] = measurement.humidity;
	if(!inserted_temp[index]) {
		samples_temp++;
		inserted_temp[index] = true;
	}
	if(!inserted_hum[index]) {
		samples_hum++;
		inserted_hum[index] = true;
	}
}

/*
	COMPUTE AVERAGES AND EMPTY STORED VALUES. AGAIN IF MEASUREMENTS WERE SENT APART WE SHOULD
	CHECK FOR BOTH COUNTERS AND EMPTY ONLY COMPLETED STORED VALUES.
*/
static void compute_averages() {
	if(samples_temp == 4) {
		last_tmp_avg = (temp_m[0]+temp_m[1]+temp_m[2]+temp_m[3])/4;
		samples_temp = 0;
		inserted_temp[0] = inserted_temp[1] = inserted_temp[2] = inserted_temp[3] = false;
	}
	if(samples_hum == 4) {
		last_hum_avg = (hum_m[0]+hum_m[1]+hum_m[2]+hum_m[3])/4;	
		samples_hum = 0;
		inserted_hum[0] = inserted_hum[1] = inserted_hum[2] = inserted_hum[3] = false;
	}
}

/*
	UPON RECEIVING A RUNICAST WE MUST UNDERSTAND IF IT IS A CROSS ACK OR A MEASURMENT
	TO DO SO, WE HAVE A "COMPRESSED" MEASUREMENT MESSAGE THAT ONLY CONTAINS THE FIRST
	FIELD OF THE "UNCOMPRESSED" ONE. IF THIS FIELD IS 1 THE OTHER 2 ARE NOT PRESENT
	(NOT SENT AT ALL) THUS BEING A CROSS ACK. IN THE OTHER CASE WE MUST READ ALSO THE OTHER
	2 FIELDS REPRESENTING HUMIDITY AND TEMPERATURE.
*/
static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {
	comp_measurement_t* m = (comp_measurement_t*)packetbuf_dataptr();
	if(m->is_cross) {
		process_post(&sense_traffic_control_process,CROSS_COMPLETED,packetbuf_dataptr());			
	} else {
		measurement_t* measurement = (measurement_t*)m;
		insert_measurement(*measurement,from);		
		process_post(&sense_traffic_control_process,VAL_RECEIVED_EVENT,packetbuf_dataptr());							
	}
}

static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){}
static void timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){}
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from){}
static void broadcast_sent(struct broadcast_conn *c, int status, int num_tx){}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv, broadcast_sent};
static const struct runicast_callbacks runicast_calls = {recv_runicast, sent_runicast, timedout_runicast};
static void close_all() {
	runicast_close(&runicast);
	broadcast_close(&broadcast);
}

/*
	THIS PROCESS HANDLES:
	1) TRAFFIC COMMUNICATION WITH TLs
	2) SENSED DATA RECEPTION FROM ALL OTHER SENSORS
*/
PROCESS_THREAD(sense_traffic_control_process, ev, data) {
	static struct etimer second_click_timer;
  	PROCESS_EXITHANDLER(close_all());	
	PROCESS_BEGIN();
	SENSORS_ACTIVATE(button_sensor);
	SENSORS_ACTIVATE(sht11_sensor);
	runicast_open(&runicast, 144, &runicast_calls);
	broadcast_open(&broadcast, 129, &broadcast_call);  	
	printf("I AM NODE: %d\n",whoami());
	while(true) {
		PROCESS_WAIT_EVENT();
		if(ev == sensors_event && data == &button_sensor) {
			etimer_set(&second_click_timer,CLOCK_SECOND*SECOND_CLICK_WAIT);
			PROCESS_WAIT_EVENT();
			if(ev == sensors_event && data == &button_sensor) {
				leds_on(LEDS_BLUE);				
				pending_vehicle = EMERGENCY;
				printf("EMERGENCY ON MAIN!!\n");
		    }
			else {
				leds_on(LEDS_RED);
				pending_vehicle = NORMAL; 
				etimer_stop(&second_click_timer);
			}
			char* v_type = (pending_vehicle == NORMAL)?"n":"e";
			packetbuf_copyfrom(v_type,sizeof(char)*(strlen(v_type)+1));	
			broadcast_send(&broadcast);				
		} else if(ev == CROSS_COMPLETED) {
			leds_off(LEDS_ALL);
		} else if(ev == VAL_RECEIVED_EVENT) {
			if(samples_hum == 3 || samples_temp == 3) {
				int tmp = (sht11_sensor.value(SHT11_SENSOR_TEMP)/10-396)/10;
				int hum = sht11_sensor.value(SHT11_SENSOR_HUMIDITY)/41;
				measurement_t m = {0,tmp,hum};
				insert_measurement(m,0);
				compute_averages();
				display_string();
			}
		}
	}
	PROCESS_END();
}

/*
	THIS PROCESS HANDLES:
	1) KEYBOARD INPUT FROM USER
*/
PROCESS_THREAD(keyboard_emergency_process, ev, data) {
	int correct;
	PROCESS_BEGIN();
	correct = 0;
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
		emergency_message = malloc(sizeof(char)*(strlen(em)+1));
		strcpy(emergency_message,em);
		printf("Emergency message set to %s\n",emergency_message);
		correct = 0;
	}
	PROCESS_END();
}
