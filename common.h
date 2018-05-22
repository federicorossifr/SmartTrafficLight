#include "random.h"
#include "dev/leds.h"
#include "stdio.h"
#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/button-sensor.h"
#include "dev/sht11/sht11-sensor.h"
#include <stdlib.h>
#include "dev/leds.h"
#include "dev/serial-line.h"
#include <string.h>
#define MAX_RETRANSMISSIONS 5
	
/*
	ADDRESSES OF SKY MOTES IN COOJA FOLLOWING CREATION ORDER
	TO BE MODIFIED IN CASE OF DEPLOY ON PHYSICAL NODES
*/	
#define TL1_ADDR 			2 	//2.0 		A004 	TL1
#define G1_ADDR 			50	//50.0		A003	G1
#define TL2_ADDR 			45  //45.0  	8157	TL2
#define G2_ADDR 			51  //51.0  	8156	G2
	
/*
	VIRTUAL INDEXES INSIDE THE APPLICATION DOMAIN
*/	
#define TL1_INDEX 			0
#define G1_INDEX 			1 
#define TL2_INDEX 			2
#define G2_INDEX 			3

#define MAX_BATTERY 		100
#define ON_TOGGLE_DRAIN 	5
#define ON_SENSE_DRAIN 		10
#define THRESHOLD_HIGH		50
#define PERIOD_DEFAULT		5
#define	PERIOD_HIGH			10 //SECONDS OF SENSING PERIOD
#define THRESHOLD_LOW		20 
#define PERIOD_LOW			20 //SECONDS OF SENSING PERIOD
#define CROSS_PERIOD		5
#define TOGGLE_INTERVAL     1 // 1 SECOND ON, 1 SECOND OFF

#define SECOND_CLICK_WAIT   0.5 //SECOND BEFORE NEXT BUTTON CLICK
#define VAL_RECEIVED_EVENT	10
#define CROSS_COMPLETED 	11
#define CROSS_REQUEST 		12
#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )
/*
	SHARED TYPES AMONG THE MOTES
*/
typedef enum { NORMAL,EMERGENCY } vehicle_t;
typedef enum { MAIN, SECOND }  road_t;
typedef enum { false, true} bool;
typedef enum { HIGH_BATTERY, MID_BATTERY, LOW_BATTERY, EMPTY_BATTERY} battery_state;
typedef struct {
	vehicle_t req_v;
	road_t 	req_r;
} cross_request_t;
typedef struct {
	bool is_cross;
} comp_measurement_t;
typedef struct {
	bool is_cross;
	int temperature;
	int humidity;
} measurement_t;

linkaddr_t tl1 = {{TL1_ADDR,0}};
linkaddr_t tl2 = {{TL2_ADDR,0}};
linkaddr_t g1  = {{G1_ADDR,0}};  
linkaddr_t g2  = {{G2_ADDR,0}};


int get_index(const linkaddr_t* link) {
	if(linkaddr_cmp(link,&tl1)) return TL1_INDEX;
	if(linkaddr_cmp(link,&tl2)) return TL2_INDEX;
	if(linkaddr_cmp(link,&g1)) return G1_INDEX;
	if(linkaddr_cmp(link,&g2)) return G2_INDEX;
	return -1;
}

int whoami() {
	return get_index(&linkaddr_node_addr);
}

static void discharge_battery(int* battery,int drain) {
	*battery=max(0,*battery-drain);		
}

static void do_sense(struct runicast_conn* runicast,int* battery) {
	SENSORS_ACTIVATE(sht11_sensor);		
	if(battery)
		discharge_battery(battery,ON_SENSE_DRAIN);
	int tmp = (sht11_sensor.value(SHT11_SENSOR_TEMP)/10-396)/10;
	int hum = sht11_sensor.value(SHT11_SENSOR_HUMIDITY)/41;
	measurement_t m;
	m.is_cross = 0;
	m.temperature = tmp;
	m.humidity = hum;
	packetbuf_copyfrom(&m,sizeof(measurement_t));
	if(!runicast_is_transmitting(runicast)) {
		printf("SENSING MEASUREMENT TO %d.0\n",G1_ADDR);
		runicast_send(runicast, &g1, MAX_RETRANSMISSIONS);
	}
	SENSORS_DEACTIVATE(sht11_sensor);		
}

cross_request_t* decide(cross_request_t* main,cross_request_t* second) {
	if(!second) return main;
	if(!main) return second;
	//HERE WE HAVE BOTH REQUEST PRESENT
	if(main->req_v == EMERGENCY) return main; //FIRST ROAD EMERGENCY HAS ABSOLUTE PRIORITY
	if(second->req_v == NORMAL) return main;  //SECOND ROAD NORMAL HAS LOWEST PRIORITY
	if(second->req_v == EMERGENCY && main->req_v == NORMAL) return second; //EMERGENCY ON SECOND IS MORE IMPORTANT
	//IN CASE OF ERRORS?
	return main;
}


static void do_toggle(int* battery,unsigned char ledv) {
	if(*battery > 20)
		discharge_battery(battery,ON_TOGGLE_DRAIN);
	leds_toggle(ledv);
}


static void shut_leds(int* battery) {
	if(*battery > 20)
		discharge_battery(battery,ON_TOGGLE_DRAIN);
	leds_off(LEDS_RED|LEDS_GREEN);
}

static void shut_leds_val(int* battery,unsigned char ledv) {
	if(*battery > 20)
		discharge_battery(battery,ON_TOGGLE_DRAIN);
	leds_off(ledv);
}