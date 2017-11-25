#include <application.h>
#include <usb_talk.h>
#include <bc_usb_cdc.h>

//* Constants
#define SR04_DATA_STREAM_SAMPLES 6
#define INITIAL_DISTANCE_MAXIMUM_MM 2500.
#define SR04_LONG_SLEEP 2000
#define SR04_SHORT_SLEEP 300



// * Structures
// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

//distance
  BC_DATA_STREAM_FLOAT_BUFFER(stream_buffer_distance_meter, SR04_DATA_STREAM_SAMPLES)
  bc_data_stream_t stream_distance_meter;

//#Variables
  int loop_counter = 1;

  int sr04_loop_counter=0;
  float distance_max_mm = INITIAL_DISTANCE_MAXIMUM_MM;
  float dist_average=INITIAL_DISTANCE_MAXIMUM_MM;
  float dist_last=INITIAL_DISTANCE_MAXIMUM_MM;
  int sr04_current_sleep=SR04_SHORT_SLEEP;

//#Handlers
void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_led_set_mode(&led, BC_LED_MODE_TOGGLE);
        bc_log_info("button pressed");
    }
}

void proximity_sensor_hanler(bc_hc_sr04_event_t event, void *event_param)
{
    float distance;
    bc_log_info(" #proximity_sensor_hanler initiated\r\n");

    if (event == BC_HC_SR04_EVENT_UPDATE)
    {
       if (bc_hc_sr04_get_distance_millimeter(&distance))
       {
         bc_data_stream_feed(&stream_distance_meter, &distance);
         bc_log_info(" ->Feed real distance (%0.0f)\r\n", floor(distance));
       }
    }

    if (sr04_loop_counter<SR04_DATA_STREAM_SAMPLES)
    {
    sr04_loop_counter++;
    //bc_log_info("proximity_sensor_hanler - set short 500 - %i/%i \r\n",  sr04_loop_counter,SR04_DATA_STREAM_SAMPLES);
    //bc_hc_sr04_set_update_interval(SR04_UPDATE_INTERVAL_SECONDS * SR04_UPDATE_INTERVAL_SHORT);

      if (sr04_current_sleep == SR04_LONG_SLEEP)
      {
       bc_log_info("!!!!!sr04 about to set shortsleep  %i \r\n", SR04_LONG_SLEEP);
       sr04_current_sleep=SR04_SHORT_SLEEP;
       bc_log_info("!!!!!proximity_sensor_hanler -  set short sleep\r\n");
       //pusobi problemy: bc_hc_sr04_set_update_interval(SR04_SHORT_SLEEP);
      }
    }
    else
    {
     sr04_loop_counter=0;
     sr04_current_sleep=SR04_LONG_SLEEP;
     bc_log_info("!!!!!proximity_sensor_hanler - about to set long sleep: %i \r\n", sr04_current_sleep);
    //pusobi problemy: bc_hc_sr04_set_update_interval(SR04_LONG_SLEEP);
     bc_log_info("!!!proximity_sensor_hanler - set long end: %i %i \r\n", sr04_loop_counter,SR04_DATA_STREAM_SAMPLES);
    }
}



void application_init(void)
{
  bc_usb_cdc_init();
  bc_log_info("application_init started\r\n");

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_ON);


    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    //initialize ultrasonic
    bc_data_stream_init(&stream_distance_meter, SR04_DATA_STREAM_SAMPLES, &stream_buffer_distance_meter);
    bc_hc_sr04_init();
    bc_hc_sr04_set_event_handler(proximity_sensor_hanler, NULL);
    // bc_hc_sr04_set_update_interval(SR04_UPDATE_INTERVAL_SECONDS * 1000);
    bc_hc_sr04_set_update_interval(2000);
}

void application_task(void *param)
{
    bc_log_info("application_task started (%i)...\r\n", loop_counter );

    //local variables
    loop_counter++;

     //get medians (lux+distance is global)
    if (bc_data_stream_get_median(&stream_distance_meter, &dist_average))  { bc_log_info (" -current distance median is: %0.0f \r\n", dist_average);}

}

