#include "random.h"
#include "dev/leds.h"
#include "stdio.h"
#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/button-sensor.h"
#include <stdlib.h>
#include <string.h>
/*
	ADDRESSES OF SKY MOTES IN COOJA FOLLOWING CREATION ORDER
	TO BE MODIFIED IN CASE OF DEPLOY ON PHYSICAL NODES
*/	
#define TL1_ADDR 			1
#define TL2_ADDR 			2
#define G1_ADDR 			3
#define G2_ADDR 			4
	
#define MAX_BATTERY 		100
#define ON_TOGGLE_DRAIN 	5
#define ON_SENSE_DRAIN 		10
#define THRESHOLD_HIGH		50
#define	PERIOD_HIGH			10 //SECONDS OF SENSING PERIOD
#define THRESHOLD_LOW		20 
#define PERIOD_LOW			20 //SECONDS OF SENSING PERIOD
#define CROSS_PERIOD		5
#define TOGGLE_INTERVAL     1 // 1 SECOND ON, 1 SECOND OFF

#define SECOND_CLICK_WAIT   0.5 //SECOND BEFORE NEXT BUTTON CLICK

#define TEMP_RECEIVED_EVENT 10
#define HUM_RECEIVED_EVENT 11
#define CROSS_COMPLETED 12

/*
	SHARED TYPES AMONG THE MOTES
*/
typedef enum { NORMAL,EMERGENCY } vehicle_t;
typedef enum { MAIN, SECOND }  road_t;
typedef enum { false, true} bool;
typedef struct {
	vehicle_t req_v;
	road_t 	req_r;
} cross_request_t;

typedef struct {
	int temperature;
	int humidity;
} measurement_t;

linkaddr_t tl1 = {{TL1_ADDR,0}};
linkaddr_t tl2 = {{TL2_ADDR,0}};
linkaddr_t g1  = {{G1_ADDR,0}};  
linkaddr_t g2  = {{G2_ADDR,0}};

int get_index(const linkaddr_t* link) {
	if(linkaddr_cmp(link,&tl1)) return 0;
	if(linkaddr_cmp(link,&tl2)) return 1;
	if(linkaddr_cmp(link,&g1)) return 2;
	if(linkaddr_cmp(link,&g2)) return 3;
	return -1;
}