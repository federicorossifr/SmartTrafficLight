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
#ifndef COOJA						// IF PHYSICAL SENSORS USE THE ADDRESSES OF THE 4 MOTES
	#define TL1_ADDR 			2 	// 2.0 		A004 	TL1
	#define G1_ADDR 			50	// 50.0		A003	G1
	#define TL2_ADDR 			45  // 45.0  	8157	TL2
	#define G2_ADDR 			51  // 51.0  	8156	G2
#else								// IF USING COOJA CREATE MOTES IN THIS ORDER
	#define TL1_ADDR 			1 	
	#define G1_ADDR 			2	
	#define TL2_ADDR 			3	
	#define G2_ADDR 			4  	
#endif
/*
	VIRTUAL INDEXES INSIDE THE APPLICATION DOMAIN
*/	
#define TL1_INDEX 			0
#define G1_INDEX 			1 
#define TL2_INDEX 			2
#define G2_INDEX 			3

/*
	APPLICATION PARAMETERS
*/
#define MAX_BATTERY 		100	// 
#define ON_TOGGLE_DRAIN 	5	// BATTERY DRAIN WHEN SWITCHING A LED FROM ON/OFF TO OFF/ON
#define ON_SENSE_DRAIN 		10	// BATTERY DRAIN WHEN SENSING

#define PERIOD_DEFAULT		5   // DEFAULT SENSING PERIOD WHEN BATTERY IS ABOVE THRESHOLD_HIGH
#define THRESHOLD_HIGH		50	// FIRST BATTERY THRESHDOLD
#define	PERIOD_HIGH			10	// SECONDS OF SENSING PERIOD WHEN BATTERY IS BELOW THRESHOLD_HIGH
#define THRESHOLD_LOW		20 	// SECOND BATTERY THRESHOLD
#define PERIOD_LOW			20 	// SECONDS OF SENSING PERIOD WHEN BATTERY IS BELOW THRESHOLD_LOW
#define CROSS_PERIOD		5	// TIME SPENT IN CROSSING THE ROAD
#define TOGGLE_INTERVAL     1 	// DEFAULT LED TOGGLING SEMI-PERIOD

#define SECOND_CLICK_WAIT   1 	// SECONDS BEFORE NEXT BUTTON CLICK
#define VAL_RECEIVED_EVENT	10	// CUSTOM EVENT FOR SINK RECEIVING TEMP AND HUM VALUES
#define CROSS_COMPLETED 	11 	// CUSTOM EVENT FOR G1 AND G2 RECEIVING CROSS COMPLETED ACKS
#define CROSS_REQUEST 		12	// CUSTOM EVENT FOR TL1 AND TL1 RECEIVING REQUESTS FROM G1 AND G2
#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )


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

/*
	DESTINATION ADDRESS FOR THE PHYSICAL NODES IN THE NETWORK
*/
linkaddr_t tl1 = {{TL1_ADDR,0}};
linkaddr_t tl2 = {{TL2_ADDR,0}};
linkaddr_t g1  = {{G1_ADDR,0}};  
linkaddr_t g2  = {{G2_ADDR,0}};


/*
	RETURN THE VIRTUAL ADDRESS OF A NODE BASING ON ITS PHYSICAL ADDRESS
*/
int get_index(const linkaddr_t* link) {
	if(linkaddr_cmp(link,&tl1)) return TL1_INDEX;
	if(linkaddr_cmp(link,&tl2)) return TL2_INDEX;
	if(linkaddr_cmp(link,&g1)) return G1_INDEX;
	if(linkaddr_cmp(link,&g2)) return G2_INDEX;
	return -1;
}

/*
	RETURN THE VIRTUAL ADDRESS OF THIS NODE
*/ 
int whoami() {
	return get_index(&linkaddr_node_addr);
}

#ifdef TL //THIS CODE WILL BE INCLUDED ONLY WHEN COMPILING tl.c
static void discharge_battery(int* battery,int drain) {
	*battery=max(0,*battery-drain);		
}
/*
	FUNCTION decide IS CALLED BY process_requests IN TL ONLY IF AT LEAST ONE
	OF THE TWO ARGUMENTS PASSED IS NOT NULL. WE RETURN THE POINTER
	TO THE REQUEST WE HAVE DECIDED TO SCHEDULE.
*/
cross_request_t* decide(cross_request_t* main,cross_request_t* second) {
	if(!second) return main;												// NO REQUEST ON SECOND ROAD, MAIN FOR SURE DIFFERENT FROM NULL
	if(!main) return second;												// IDEM BUT THE OTHER WAY AROUND
	if(main->req_v == EMERGENCY) return main;								// IF THERE IS EMERGENCY ON MAIN, LET THAT PASS NO MATTER OTHERS
	if(second->req_v == NORMAL) return main;								// IF THERE IS NORMAL ON SECOND, THIS IS THE LEAST PRIORITY, SO LET PASS MAIN
	if(second->req_v == EMERGENCY && main->req_v == NORMAL) return second;	// IF SECOND IS EMERGENCY AND MAIN IS NORMAL, VEHICLE IS PRIORITARY, SO LET PASS SECOND
	return main;
}

static void do_toggle(int* battery,unsigned char ledv) {
	//if(*battery > 20)
		discharge_battery(battery,ON_TOGGLE_DRAIN);
	leds_toggle(ledv);
}

static void shut_leds(int* battery) {
	//if(*battery > 20)
		discharge_battery(battery,ON_TOGGLE_DRAIN);
	leds_off(LEDS_RED|LEDS_GREEN);
}

static void shut_leds_val(int* battery,unsigned char ledv) {
	//if(*battery > 20)
		discharge_battery(battery,ON_TOGGLE_DRAIN);
	leds_off(ledv);
}
#endif

#ifndef SINK //FOLLOWING CODE WILL BE INCLUDED ONLY WHEN NOT COMPILING g1.c
static void do_sense(struct runicast_conn* runicast,int* battery) {
	SENSORS_ACTIVATE(sht11_sensor);	
	#ifdef TL 
	if(battery)
		discharge_battery(battery,ON_SENSE_DRAIN);
	#endif
	int tmp = (sht11_sensor.value(SHT11_SENSOR_TEMP)/10-396)/10;
	int hum = sht11_sensor.value(SHT11_SENSOR_HUMIDITY)/41;
	measurement_t m;
	m.is_cross = 0;
	m.temperature = tmp;
	m.humidity = hum;
	packetbuf_copyfrom(&m,sizeof(measurement_t));
	if(!runicast_is_transmitting(runicast)) {
		printf("SENDING MEASUREMENT TO %d.0\n",G1_ADDR);
		runicast_send(runicast, &g1, MAX_RETRANSMISSIONS);
	}
	SENSORS_DEACTIVATE(sht11_sensor);		
}
#endif
