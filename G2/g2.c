#include "random.h"
#include "dev/leds.h"
#include "stdio.h"
#include "../common.h"
#include "contiki.h"
#include "net/rime/rime.h"
PROCESS(sense_traffic_control_process, "TRAFFIC_CONTROL_PROCESS");
AUTOSTART_PROCESSES(&sense_traffic_control_process);

PROCESS_THREAD(sense_traffic_control_process, ev, data) {
	PROCESS_BEGIN();

	PROCESS_END();
}

