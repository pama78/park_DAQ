//************************************************
//Name:    UCL park - DAQ node
//Purpose: read from sensors the data, send them to AGR
//Author:  Pavel Majer
//Date:    3/1/2018
//Status:  support pairing, send message, measure SR06
//         support 6x fast, then slow measure
//         
//Plan:    lux meter to rule the sleep length. 
//         regular lux to be initialized at start. (light expected)
//         initialize at boot - the distance, or 2,5m (no car expected)
//         during initialize blink both, then only green and fast
//         when change detected, or hour passed, send message
//         on DAQ store last sync minute, so can expect the heartbeat message or show error
//         sync battery, occupied, (meters at start, for tuning)
//
//Revisions: 
//  
//************************************************

#include <application.h>
#include <usb_talk.h>
#include <bc_usb_cdc.h>
#include <bc_radio.h>
#include <bc_device_id.h>

//* Constants
#define HC_SR04_DATA_STREAM_SAMPLES 6
#define INITIAL_DISTANCE_MAXIMUM_MM 2500.
#define SR04_LONG_SLEEP 2000
#define SR04_SHORT_SLEEP 300


// * Structures
// LED instance
bc_led_t led;

// Button instance
bc_button_t button;
uint16_t button_event_count = 0;

//pairing
uint64_t my_device_address;
//bc_atsha204_t atsha204;

uint32_t daq_address;
uint8_t serial[4];
//uint32_t serial;


//distance
BC_DATA_STREAM_FLOAT_BUFFER(stream_buffer_distance_meter, HC_SR04_DATA_STREAM_SAMPLES)
bc_data_stream_t stream_distance;

//#Variables
int loop_counter = 1;
int sr04_loop_counter = 0;
float distance_max_mm = INITIAL_DISTANCE_MAXIMUM_MM;
float dist_median = INITIAL_DISTANCE_MAXIMUM_MM;
float dist_last = INITIAL_DISTANCE_MAXIMUM_MM;
int sr04_current_sleep = SR04_SHORT_SLEEP;

//#Handlers

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param) {
    bc_log_info("#button_event_handler initiated\r\n");
    (void) self;

    if (event == BC_BUTTON_EVENT_PRESS) {
        uint16_t *event_count = (uint16_t *) event_param;
        bc_led_set_mode(&led, BC_LED_MODE_TOGGLE);
        bc_log_info("button pressed\n\r");
        (*event_count)++;
        bc_radio_pub_push_button(event_count);
    }
}

void hc_sr04_event_handler(bc_hc_sr04_event_t event, void *event_param) {
    bc_log_info("#hc_sr04_event_handler initiated\r\n");

    if (event == BC_HC_SR04_EVENT_UPDATE) {
        float distance;

        if (bc_hc_sr04_get_distance_millimeter(&distance)) {
            bc_data_stream_feed(&stream_distance, &distance);
            bc_log_info("HC-SR04 update: %0.0f mm\r\n", floor(distance));
        } else {
            bc_log_error("HC-SR04 error\r\n");
        }
    } else if (event == BC_HC_SR04_EVENT_ERROR) {
        bc_log_error("HC-SR04 error\r\n");
    }
}

//TBD vyhodit

static void radio_event_handler(bc_radio_event_t event, void *event_param) {
    bc_log_info("radio_event_handler entered 1\r\n");
    (void) event_param;

    bc_log_info("radio_event_handler - after void\r\n");

    if (event == BC_RADIO_EVENT_INIT_DONE) {
        //tohle se z nejakeho duvodu nevola, tady bych si rad nacetl seriove cislo
        bc_log_info("after event == BC_RADIO_EVENT_INIT_DONE\r\n");
    }

    bc_log_info("radio_event_handler ended\r\n");
}

void send_message_to_server() {
    bc_log_info("#send_message_to_server initiated\r\n");

    // tohle vraci cislo, ale nevim jestli jde o unikatni seriove cislo
    bc_log_info("serialM - zacatek\r\n ");
    uint32_t serialY;
    bc_device_id_get(&serialY, sizeof (serialY));
    bc_log_info("radio handler: my_device_address3a: %ul \r\n", serialY);

    //posilani bufferu
    uint8_t buffer[12];
    int lux_average = 100; //tbd
    int lux_day_watermark = 100; //tbd
    buffer[4] = (int) (dist_median / 10); //temporarily cannot be>255
    buffer[5] = (int) (lux_average / lux_day_watermark * 100); //percent of light
    //DEBUG("@@percent light=%i in buffer: %i\r\n",(int)(lux_average/lux_day_watermark*100), buffer[5]);
    //buffer[6] = (int)(battery_voltage*100) ; //milivolts
    //DEBUG("@@battery voltage=%i in buffer: %i \r\n",(int)(battery_voltage*100), buffer[6]);
    //buffer[7] = (int)(battery_percentage) ;
    //DEBUG("@@battery percentage=%i in buffer: %i \r\n",(int)(battery_percentage), buffer[7]);
    buffer[8] = 255;
    buffer[9] = 255;
    buffer[10] = 255; //max //code all in one byte: occupied, night, run id

    bc_log_info("@sending: @dist=%i dm in buffer: %i \r\n", (int) (dist_median / 10), buffer[4]);
    bc_radio_pub_buffer(buffer, sizeof (buffer));

}

void measurement_task(void *param) {
    bc_log_info("measurement_task - entered\r\n");

    static int counter = 0;
    if (counter < HC_SR04_DATA_STREAM_SAMPLES) {
        counter++;
        bc_log_info("Burst measure %i\r\n", counter);
        bc_hc_sr04_measure();
        bc_scheduler_plan_current_relative(300);
    } else {
        counter = 0;
        if (bc_data_stream_get_median(&stream_distance, &dist_median)) {
            bc_log_info("Distance median: %0.0f mm\r\n", dist_median);
        } else {
            bc_log_error("Distance median: ?\r\n");
        }
        bc_log_info("Reset data stream - wait 10s\r\n");
        bc_data_stream_reset(&stream_distance);
        bc_scheduler_plan_current_relative(10000);

        bc_log_info("calling send_message_to_server\r\n");
        send_message_to_server();

    }


}

void application_init(void) {
    //logger init
    bc_usb_cdc_init(); //mozna bude lepsi log_init
    //bc_log_init(BC_LOG_LEVEL_DEBUG, BC_LOG_TIMESTAMP_ABS);
    bc_log_info("#application_init started V3\r\n");

    // Initialize LED
    bc_log_info("#bc_led_init started\r\n");
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_ON);
    bc_log_info("\r\n#bc_led_init ended\r\n");

    // Initialize button
    bc_log_info("#application_init button init\r\n");
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);
    bc_log_info("\r\n#application_init button init ended\r\n");


    //init radio
    bc_log_info("#application_init radio init\r\n");
    // bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);
    bc_radio_init(BC_RADIO_MODE_NODE_LISTENING);
    bc_radio_set_event_handler(radio_event_handler, NULL); //new
    bc_log_info("\r\n#application_init radio init ended\r\n");

    //initialize ultrasonic
    bc_log_info("#application_init ultrasonic init\r\n");
    bc_data_stream_init(&stream_distance, HC_SR04_DATA_STREAM_SAMPLES / 2, &stream_buffer_distance_meter);
    bc_hc_sr04_init();
    bc_hc_sr04_set_event_handler(hc_sr04_event_handler, NULL);
    bc_scheduler_register(measurement_task, NULL, 0);
    bc_log_info("\r\n#application_init ultrasonic init\r\n");

    //radio pairing
    bc_log_info("#application_init radio pairing request\r\n");
    bc_radio_pairing_request(FIRMWARE, VERSION);
    bc_led_pulse(&led, 2000);
    bc_log_info("\r\n#application_init radio pairing request ended\r\n");

    bc_log_info("#application_init ended\r\n");
}

void application_task(void *param) {
    //    bc_log_info("#application_task started (%i)...\r\n", loop_counter );

    //local variables
    loop_counter++;

    //get medians (lux+distance is global)
    //   if (bc_data_stream_get_median(&stream_distance, &dist_median))  { bc_log_info (" -current distance median is: %0.0f \r\n", dist_median);}

    //  bc_log_info("calling send_message_to_server\r\n");
    //   send_message_to_server();

    bc_log_info("set plan relative 3000\r\n");
    bc_scheduler_plan_current_relative(3000);
}

