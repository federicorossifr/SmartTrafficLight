#include "random.h"
#include "dev/leds.h"
#include "stdio.h"
#include "../common.h"
#include "contiki.h"
#include "net/rime/rime.h"
PROCESS(keyboard_emergency_process, "SENSE_LIGHT_PROCESS");
PROCESS(sense_traffic_control_process, "TRAFFIC_CONTROL_PROCESS");
AUTOSTART_PROCESSES(&sense_traffic_control_process,&keyboard_emergency_process);

PROCESS_THREAD(sense_traffic_control_process, ev, data) {
	PROCESS_BEGIN();

	PROCESS_END();
}

PROCESS_THREAD(keyboard_emergency_process, ev, data) {
	PROCESS_BEGIN();

	PROCESS_END();
}
