#include <application.h>
#include <usb_talk.h>
#include <bc_usb_cdc.h>
#include <bc_radio.h>

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

//distance
  BC_DATA_STREAM_FLOAT_BUFFER(stream_buffer_distance_meter, HC_SR04_DATA_STREAM_SAMPLES)
  bc_data_stream_t stream_distance;

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
    bc_log_info("#button_event_handler initiated\r\n");
    (void) self;

    if (event == BC_BUTTON_EVENT_PRESS)
    {
        uint16_t *event_count = (uint16_t *) event_param;
        bc_led_set_mode(&led, BC_LED_MODE_TOGGLE);
        bc_log_info("button pressed");
        (*event_count)++;
        bc_radio_pub_push_button(event_count);
    }
}

void hc_sr04_event_handler(bc_hc_sr04_event_t event, void *event_param)
{
    bc_log_info("#hc_sr04_event_handler initiated\r\n");

    if (event == BC_HC_SR04_EVENT_UPDATE)
    {
  	    float distance;

       if (bc_hc_sr04_get_distance_millimeter(&distance))
       {
         bc_data_stream_feed(&stream_distance, &distance);
         bc_log_info("HC-SR04 update: %0.0f mm\r\n", floor(distance));
       }
       else
        {
            bc_log_error("HC-SR04 error");
        }
    } 
    else if (event == BC_HC_SR04_EVENT_ERROR)
    {
        bc_log_error("HC-SR04 error");
    }
}

void measurement_task(void *param)
{
    bc_log_info("measurement_task - entered");

    static int counter = 0;
    if (counter < HC_SR04_DATA_STREAM_SAMPLES)
    {
        counter++;
        bc_log_info("Burst measure %i", counter);
        bc_hc_sr04_measure();
        bc_scheduler_plan_current_relative(300);
    }
    else
    {
        counter = 0;
        float median;
        if (bc_data_stream_get_median(&stream_distance, &median))
        {
            bc_log_info("Distance median: %0.0f mm", median);
        }
        else
        {
            bc_log_info("Distance median: ?");
        }
        bc_log_info("Reset data stream - wait 10s");
        bc_data_stream_reset(&stream_distance);
        bc_scheduler_plan_current_relative(10000);
    }
}



void send_message_to_server()
{
    bc_log_info("#send_message_to_server initiated\r\n");

    uint8_t buffer[12];
      buffer[4] = (int)(dist_average/10); //temporarily cannot be>255
      bc_log_info("@sending: @dist=%i dm in buffer: %i \r\n",(int)(dist_average/10) , buffer[4]);
      bc_radio_pub_buffer(buffer,sizeof(buffer));

/*    buffer[0] = (int)my_device_address;  //id of client
      buffer[1] = (int)my_device_address >> 8 ;  //id of client
      buffer[2] = (int)my_device_address >> 16;  //id of client
      buffer[3] = (int)my_device_address >> 24;  //id of client

      DEBUG("@new serial: %i",(int)my_device_address);
      DEBUG("@@atsha204_event_handler - detected serial (%i) \r\n", (int)serial );
      DEBUG("@@radio init - detected serial (%i) \r\n", (int)my_device_address );
      DEBUG("@@my_device_address:%i @@my_device_address_buff=%i \n\r",
          (int) my_device_address, ((buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0]));

      buffer[5] = (int)(lux_average/lux_day_watermark*100); //percent of light
      DEBUG("@@percent light=%i in buffer: %i\r\n",(int)(lux_average/lux_day_watermark*100), buffer[5]);
      buffer[6] = (int)(battery_voltage*100) ; //milivolts
      DEBUG("@@battery voltage=%i in buffer: %i \r\n",(int)(battery_voltage*100), buffer[6]);
      buffer[7] = (int)(battery_percentage) ;
      DEBUG("@@battery percentage=%i in buffer: %i \r\n",(int)(battery_percentage), buffer[7]);
      buffer[8] = 255 ;
      buffer[9] = 255 ;
      buffer[10] = 255 ; //max //code all in one byte: occupied, night, run id


  if ( bc_radio_pub_buffer(buffer,sizeof(buffer)) )
  {
    //testing data send
    DEBUG("sent buffer orig data is: serial:%i dist:%0.0f lux:%0.0f batVolt:%0.2f batPerc:%0.2f \n\r",
     (int)serial, dist_average/10, lux_average, battery_voltage, battery_percentage);
    DEBUG("sent buffer is: serial:%i dist:%i lux:%i batVolt:%i batPerc:%i \n\r",
     (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0],
     buffer[4],buffer[5],buffer[6],buffer[7],buffer[8],buffer[9],buffer[10]);
  }
  else
  {
    DEBUG("failed to sent buffer: serial:%i dist:%i lux:%i batVolt:%i batPerc:%i \n\r",
    (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0],
    buffer[4],buffer[5],buffer[6],buffer[7],buffer[8],buffer[9],buffer[10]);
}
*/

}


void application_init(void)
{
  bc_usb_cdc_init(); //mozna bude lepsi log_init
  //bc_log_init(BC_LOG_LEVEL_DEBUG, BC_LOG_TIMESTAMP_ABS);
  bc_log_info("#application_init started V3\r\n");

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_ON);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    //initialize ultrasonic
 //   bc_data_stream_init(&stream_distance, HC_SR04_DATA_STREAM_SAMPLES / 2, &stream_buffer_distance_meter);
      bc_data_stream_init(&stream_distance, HC_SR04_DATA_STREAM_SAMPLES , &stream_buffer_distance_meter);
   
    bc_hc_sr04_init();
    bc_hc_sr04_set_event_handler(hc_sr04_event_handler, NULL);
   // bc_hc_sr04_set_update_interval(SR04_SHORT_SLEEP);
    bc_scheduler_register(measurement_task, NULL, 0);
    //bc_hc_sr04_set_update_interval(5000);

    //init radio
     bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);
     bc_log_info("radio pairing request\r\n");

     bc_radio_pairing_request(FIRMWARE, VERSION);
     bc_led_pulse(&led, 2000);
}

void application_task(void *param)
{
    bc_log_info("#application_task started (%i)...\r\n", loop_counter );

    //local variables
    loop_counter++;

     //get medians (lux+distance is global)
 //   if (bc_data_stream_get_median(&stream_distance, &dist_average))  { bc_log_info (" -current distance median is: %0.0f \r\n", dist_average);}

//    bc_log_info("calling send_message_to_server\r\n");
//    send_message_to_server();

     bc_log_info("set plan relative 3000\r\n");
     bc_scheduler_plan_current_relative(3000);
}

