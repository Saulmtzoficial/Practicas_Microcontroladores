#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"

//================ VEHICULARES =================
#define CAR1R 0
#define CAR1O 1
#define CAR1G 2

#define CAR2R 3
#define CAR2O 4
#define CAR2G 5

//================ PEATONALES =================
#define PEA1R 6
#define PEA1G 7

#define PEA2R 8
#define PEA2G 9

//================ BUZZER =================
#define BUZZER 10

//================ BOTONES =================
#define PUSH1 11
#define PUSH2 12

//================ DISPLAY =================
#define SEG_B 16
#define SEG_A 17
#define SEG_F 18
#define SEG_G 19
#define SEG_C 20
#define SEG_D 21
#define SEG_E 22

#define COM_DISP1 26
#define COM_DISP2 27

//================ TIEMPOS =================
#define GREEN_NORMAL 5000
#define GREEN_FAST 1666
#define YELLOW_TIME 2000

//================================================
typedef enum{
    STATE_CAR1_GREEN,
    STATE_CAR1_YELLOW,
    STATE_CAR2_GREEN,
    STATE_CAR2_YELLOW,
    STATE_PED1,
    STATE_PED2
} system_state_t;

volatile system_state_t current_state = STATE_CAR1_GREEN;

volatile bool ped1_request = false;
volatile bool ped2_request = false;

volatile int disp1_val = 10;
volatile int disp2_val = 10;

//================================================
// DISPLAY CATODO COMUN
const uint8_t seg7[11] = {
    0x3F, //0
    0x06, //1
    0x5B, //2
    0x4F, //3
    0x66, //4
    0x6D, //5
    0x7D, //6
    0x07, //7
    0x7F, //8
    0x6F, //9
    0x00  //apagado
};

//================================================
void init_output(uint pin){
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin,0);
}

void init_input(uint pin){
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

//================================================
void set_car1(bool r,bool y,bool g){
    gpio_put(CAR1R,r);
    gpio_put(CAR1O,y);
    gpio_put(CAR1G,g);
}

void set_car2(bool r,bool y,bool g){
    gpio_put(CAR2R,r);
    gpio_put(CAR2O,y);
    gpio_put(CAR2G,g);
}

//================================================
void write_segments(uint8_t pattern){
    gpio_put(SEG_A, (pattern >> 0) & 1);
    gpio_put(SEG_B, (pattern >> 1) & 1);
    gpio_put(SEG_C, (pattern >> 2) & 1);
    gpio_put(SEG_D, (pattern >> 3) & 1);
    gpio_put(SEG_E, (pattern >> 4) & 1);
    gpio_put(SEG_F, (pattern >> 5) & 1);
    gpio_put(SEG_G, (pattern >> 6) & 1);
}

//================================================
void mux_tick(){
    static bool current = false;

    gpio_put(COM_DISP1,1);
    gpio_put(COM_DISP2,1);

    if(!current){
        write_segments(seg7[disp1_val]);
        gpio_put(COM_DISP1,0);
    }
    else{
        write_segments(seg7[disp2_val]);
        gpio_put(COM_DISP2,0);
    }

    current = !current;
}

//================================================
void check_buttons(){
    if(!gpio_get(PUSH1)){
        sleep_ms(20);
        if(!gpio_get(PUSH1)){
            ped1_request = true;
            while(!gpio_get(PUSH1)){
                mux_tick();
            }
        }
    }

    if(!gpio_get(PUSH2)){
        sleep_ms(20);
        if(!gpio_get(PUSH2)){
            ped2_request = true;
            while(!gpio_get(PUSH2)){
                mux_tick();
            }
        }
    }
}

//================================================
void smart_delay(uint32_t ms){
    absolute_time_t end = make_timeout_time_ms(ms);
    while(!time_reached(end)){
        mux_tick();
        check_buttons();
    }
}

//================================================
void beep(uint32_t ms){
    absolute_time_t end = make_timeout_time_ms(ms);
    while(!time_reached(end)){
        gpio_put(BUZZER,1);
        sleep_us(500);
        gpio_put(BUZZER,0);
        sleep_us(500);
        mux_tick();
    }
}

//================================================
void pedestrian_crossing(
    uint pedR,
    uint pedG,
    volatile int *display_val
){
    gpio_put(pedR,0);
    gpio_put(pedG,1);

    beep(300);

    for(int i=9;i>=0;i--){
        *display_val = i;
        if(i<=3 && i>0){
            beep(200);
        }
        smart_delay(1000);
    }

    gpio_put(BUZZER,0);
    *display_val = 10;

    gpio_put(pedG,0);
    gpio_put(pedR,1);

    beep(500);
}

//================================================
void update_system(){
    switch(current_state){

        case STATE_CAR1_GREEN:
            set_car1(0,0,1);
            set_car2(1,0,0);
            if(ped1_request || ped2_request)
                smart_delay(GREEN_FAST);
            else
                smart_delay(GREEN_NORMAL);
            current_state = STATE_CAR1_YELLOW;
            break;

        case STATE_CAR1_YELLOW:
            set_car1(0,1,0);
            set_car2(1,0,0);
            smart_delay(YELLOW_TIME);
            if(ped1_request)
                current_state = STATE_PED1;
            else if(ped2_request)
                current_state = STATE_PED2;
            else
                current_state = STATE_CAR2_GREEN;
            break;

        case STATE_CAR2_GREEN:
            set_car1(1,0,0);
            set_car2(0,0,1);
            if(ped1_request || ped2_request)
                smart_delay(GREEN_FAST);
            else
                smart_delay(GREEN_NORMAL);
            current_state = STATE_CAR2_YELLOW;
            break;

        case STATE_CAR2_YELLOW:
            set_car1(1,0,0);
            set_car2(0,1,0);
            smart_delay(YELLOW_TIME);
            if(ped1_request)
                current_state = STATE_PED1;
            else if(ped2_request)
                current_state = STATE_PED2;
            else
                current_state = STATE_CAR1_GREEN;
            break;

        case STATE_PED1:
            ped1_request = false;
            set_car1(1,0,0); // rojo
            set_car2(0,0,1); // verde
            pedestrian_crossing(PEA1R, PEA1G, &disp1_val);
            current_state = STATE_CAR2_GREEN;
            break;

        case STATE_PED2:
            ped2_request = false;
            set_car1(0,0,1); // verde
            set_car2(1,0,0); // rojo
            pedestrian_crossing(PEA2R, PEA2G, &disp2_val);
            current_state = STATE_CAR1_GREEN;
            break;
    }
}

//================================================
void init_all(){
    int outputs[] = {
        CAR1R,CAR1O,CAR1G,
        CAR2R,CAR2O,CAR2G,
        PEA1R,PEA1G,
        PEA2R,PEA2G,
        BUZZER,
        SEG_A,SEG_B,SEG_C,
        SEG_D,SEG_E,SEG_F,SEG_G,
        COM_DISP1,COM_DISP2
    };

    for(int i=0;i<20;i++){
        init_output(outputs[i]);
    }

    init_input(PUSH1);
    init_input(PUSH2);

    gpio_put(COM_DISP1,1);
    gpio_put(COM_DISP2,1);

    gpio_put(PEA1R,1);
    gpio_put(PEA1G,0);

    gpio_put(PEA2R,1);
    gpio_put(PEA2G,0);

    gpio_put(BUZZER,0);

    disp1_val = 10;
    disp2_val = 10;
}

//================================================
int main(){
    stdio_init_all();
    init_all();

    while(true){
        mux_tick();
        check_buttons();
        update_system();
    }

    return 0;
}