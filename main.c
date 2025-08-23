#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_int.h"
#include "em_timer.h"
#include "em_lcd.h"
#include <string.h>
#include "segmentlcd.h"


#define button1_port gpioPortB       //btn0
#define button1_pin 9
#define button2_port gpioPortB      //btn1
#define button2_pin 10
#define led1_port gpioPortE			//led0
#define led1_pin 2
#define led2_port gpioPortE			//led1
#define led2_pin 3

#define pwm_port gpioPortE  		//location 1 for timer1. pwm output port
#define pwm_pin 10
#define pwm_freq 300

#define fwd_port gpioPortC			//pin connected to pin 2 in the motor shield
#define fwd_pin 1

#define bck_port gpioPortC			//pin connected to pin 7 in the motor shield
#define bck_pin 2

// Definitions for the signal generator
#define sig_port gpioPortD			//connected to transistor to allow or block current to ir leds.
#define sig_pin 3					// the ir LEDs sends a 38kHz signal

// Definitions for the receivers
#define recv_port gpioPortD
#define recv_pin2 2
#define recv_pin3 4
#define recv_pin4 6







int current_station = 0; // Variable to track the current station
int dutyCycle=50;
void move_fwd(void);
void move_bck(void);
void stop(void);
void gpio_init(void);
void initTIMER0(void);
void initExternalInterrupt(void);
void wait(int t); // delay in ms
void initTIMER1(void);
void setPWMDutyCycle(uint32_t duty_cycle);
void initLCD(void);


int main(void) {
    // Chip errata
    CHIP_Init();

    // Initialize GPIO, TIMER0, and External Interrupt
    gpio_init();
    initTIMER0();
    initExternalInterrupt();
    initTIMER1();
    initLCD();
    // Test LED
    stop();
    // Enter infinite loop
    while (1)
    {
    }
}


void initLCD(void)//setup the LCD
{
    // Enable clock for the LCD module
	const char s[]={'-','>','-','>','-','>'};
   SegmentLCD_Init(false);
   SegmentLCD_AllOff();
   SegmentLCD_Number(0);
   SegmentLCD_Write(s);
}



void gpio_init(void) //setup all the gpio pins including the pushbutton
{
    // Enable GPIO clock
    CMU_ClockEnable(cmuClock_GPIO, true);

    			//setup the buttons
    GPIO_PinModeSet(button1_port, button1_pin, gpioModeInputPull, 1);
       GPIO_PinModeSet(button2_port, button2_pin, gpioModeInputPull, 1);

       // Configure the LED pins as output (for testing not in use)
       GPIO_PinModeSet(led1_port, led1_pin, gpioModePushPull, 0);
       GPIO_PinModeSet(led2_port, led2_pin, gpioModePushPull, 0);

    // Initialize signal output pin
    GPIO_PinModeSet(sig_port, sig_pin, gpioModePushPull, 0); // D3 as output, initially low

    //initialize the pwm pin as output
    GPIO_PinModeSet(pwm_port, pwm_pin, gpioModePushPull, 0);
    GPIO_PinModeSet(fwd_port, fwd_pin, gpioModePushPull, 0); //setup the diriction pins
    GPIO_PinModeSet(bck_port, bck_pin, gpioModePushPull, 0);
    // Initialize receiver input pins
    GPIO_PinModeSet(recv_port, recv_pin2, gpioModeInputPullFilter, 1); // D2 as input with pull-up filter
    GPIO_PinModeSet(recv_port, recv_pin3, gpioModeInputPullFilter, 1); // D4 as input with pull-up filter
    GPIO_PinModeSet(recv_port, recv_pin4, gpioModeInputPullFilter, 1); // D6 as input with pull-up filter
}

void initTIMER0(void) //setup the timer responsable for the IR signal
{
    // Enable clock for TIMER0 module
    CMU_ClockEnable(cmuClock_TIMER0, true);

    // Set TIMER0 parameters
    TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
    timerInit.prescale = timerPrescale1;
    timerInit.enable = false;
    TIMER_Init(TIMER0, &timerInit);

    // Set TIMER0 TOP value to generate 38kHz signal
    uint32_t hfperFreq = CMU_ClockFreqGet(cmuClock_HFPER);
    uint32_t timerTop = hfperFreq / 38000 / 2 - 1; // Divide by 2 for toggle

    TIMER_TopSet(TIMER0, timerTop);

    // Enable TIMER0 overflow interrupt
    TIMER_IntEnable(TIMER0, TIMER_IF_OF);
    NVIC_EnableIRQ(TIMER0_IRQn);

    // Start TIMER0
    TIMER_Enable(TIMER0, true);
}

void initTIMER1(void) //setup timer responsable for pwm signal for the motor shield
{
	 // Enable clock for TIMER1 module
	    CMU_ClockEnable(cmuClock_TIMER1, true);

	    // Set TIMER1 parameters
	    TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
	    timerInit.prescale = timerPrescale1;
	    timerInit.enable = false;
	    TIMER_Init(TIMER1, &timerInit);

	    // Set PWM frequency
	    uint32_t hfperFreq = CMU_ClockFreqGet(cmuClock_HFPER);
	    uint32_t timerTop = hfperFreq / pwm_freq - 1;
	    TIMER_TopSet(TIMER1, timerTop);

	    // Set initial PWM duty cycle (50% in this example)
	    dutyCycle = timerTop / 2;
	    TIMER_CompareSet(TIMER1, 0, dutyCycle);

	    // Configure TIMER1 channel 0 for PWM
	    TIMER1->CC[0].CTRL = TIMER_CC_CTRL_MODE_PWM;

	    // Configure the GPIO pin for PWM output

	    // Route TIMER1 CC0 output to the GPIO pin
	    //  LOCATION 1 routes to E10
	    TIMER1->ROUTE = TIMER_ROUTE_CC0PEN | TIMER_ROUTE_LOCATION_LOC1;

	    // Start TIMER1
	    TIMER_Enable(TIMER1, true);
}

void setPWMDutyCycle(uint32_t duty_cycle) //change PWM
{

	int val=TIMER_TopGet(TIMER1)*duty_cycle/100;
    TIMER_CompareSet(TIMER1, 0, val);

}

void TIMER1_IRQHandler(void) //function for the pwm interrupt
{
    // Clear TIMER1 interrupt flag
	GPIO_PinOutSet(pwm_port, pwm_pin);
    TIMER_IntClear(TIMER1, TIMER_IF_OF);


}


void wait(int t) { // delay in ms
    long int i;
    for (i = 0; i < 530 * t; i++);
}

void move_fwd(void) //set rotation diriction
{
    GPIO_PinOutSet(fwd_port, fwd_pin);
    GPIO_PinOutClear(bck_port, bck_pin);
}

void move_bck(void) //set rotation diriction
{
    GPIO_PinOutClear(fwd_port, fwd_pin);
    GPIO_PinOutSet(bck_port, bck_pin);
}

void stop(void)    //stop the motor
{
	 //GPIO_PinOutSet(pwm_port, pwm_pin);
	GPIO_PinOutClear(fwd_port, fwd_pin);
    GPIO_PinOutClear(bck_port, bck_pin);
}

// TIMER0 interrupt handler
void TIMER0_IRQHandler(void) {
    // Clear TIMER0 interrupt flag
    TIMER_IntClear(TIMER0, TIMER_IF_OF);

    // Toggle D3 pin
    GPIO_PinOutToggle(sig_port, sig_pin);
}

void initExternalInterrupt(void) {

	 CMU_ClockEnable(cmuClock_TIMER0, true);
//works on rising edge since signal not detected =1
    GPIO_IntConfig(recv_port, recv_pin2, true, false, true);
    GPIO_IntConfig(recv_port, recv_pin3,  true,false, true);
    GPIO_IntConfig(recv_port, recv_pin4,true, false, true);

    // Enable GPIO_EVEN interrupt in NVIC
    NVIC_EnableIRQ(GPIO_EVEN_IRQn);
}



// GPIO EVEN interrupt handler
void GPIO_EVEN_IRQHandler(void)
{
	if(GPIO_PinInGet(recv_port, recv_pin2)>0 && current_station == 2) // arrived at station 1? (left hand station
	{
		setPWMDutyCycle(35%100);		//slow down motor
		wait(2000);             //for two seconds
		current_station = 1;     //set the current station to 1
		stop();                  //stop the motor
		SegmentLCD_Number(1);		//display station on LCD
		wait(1000);
		while(GPIO_PinInGet( button1_port,button1_pin)==1){}  //wait for button press
		move_fwd();    										// change direction
		const char s[]={'-','>','-','>','-','>'};			//diplay travel direction
		SegmentLCD_Write(s);
				wait(2000);
		setPWMDutyCycle(75%100);		// move at nominal speed

	}

	else if(GPIO_PinInGet(recv_port, recv_pin3)>0 && current_station != 2)		// arrived at station 2?  middle station
	{

	setPWMDutyCycle(35%100);			//slow down motor
		wait(2000);						//for two seconds
		current_station = 2;			 //set the current station to 2
		stop();							 //stop the motor
		SegmentLCD_Number(2);			//display station on LCD
		wait(1000);
		int dir=0;
		while(dir==0 )   //wait for pushbutton. travel direction is dependent on pushbutton
		{
			if(GPIO_PinInGet( button1_port,button1_pin)==0){
			dir=1;
			move_fwd();
			const char s[]={'-','>','-','>','-','>'};

			SegmentLCD_Write(s);
			}
			else if(GPIO_PinInGet( button2_port,button2_pin)==0){
			dir=3;

			move_bck();
			const char s[]={'<','-','<','-','<','-'};

			SegmentLCD_Write(s);
			}
		}
				wait(2000);
		setPWMDutyCycle(75%100);
	}
	else if(GPIO_PinInGet(recv_port, recv_pin4)>0 && current_station ==2) //arrived at station 3 (right hand station)
	{
		setPWMDutyCycle(35%100);				//slow down motor
		wait(2000);								//wait two seconds
		current_station = 3;					//set current station
		stop();
		SegmentLCD_Number(3);
		wait(1000);
		while(GPIO_PinInGet( button2_port,button2_pin)==1){} //wait for button press
		move_bck();
		const char s[]={'<','-','<','-','<','-'};
		SegmentLCD_Write(s);
				wait(2000);
		setPWMDutyCycle(75%100);
	}

   GPIO_IntClear((1<<recv_pin2)|(1<<recv_pin3)|(1<<recv_pin4));
}
