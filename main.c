
#include <stdio.h>
#include <inttypes.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/mw/display/Display.h>
#include <ti/mw/display/DisplayExt.h>

/* Board Header files */
#include "Board.h"

#include "wireless/comm_lib.h"
#include "sensors/bmp280.h"
#include "sensors/mpu9250.h"

/*Tilakoneen mahdolliset tilat ovat DO_NOTHING, READ_SENSOR ja MSG_WAITING*/
enum state { DO_NOTHING, READ_SENSOR, MSG_WAITING };

// Esitellään ja alustetaan globaali tilamuuttuja odotustilaan.
enum state myState = DO_NOTHING;

/* Task */
#define STACKSIZE 2048
Char labTaskStack[4096];
Char commTaskStack[STACKSIZE];
Char screenTaskStack[STACKSIZE];

/* Display */
Display_Handle hDisplay;

// Pin configuration and variables
static PIN_Handle hButtonShut;
static PIN_State bStateShut;

static PIN_Handle buttonHandle;
static PIN_State buttonState;

static PIN_Handle ledHandle;
static PIN_State ledState;

static PIN_Handle hMpuPin;
static PIN_State MpuPinState;


/* Pinnien määrittelyt */

//Virtakytkin (painonappi1)
PIN_Config buttonShut[] = {
Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
PIN_TERMINATE
};
PIN_Config buttonWake[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
   PIN_TERMINATE
};

// Painonappi (painonappi0)
PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE, // Hox! TAI-operaatio
   PIN_TERMINATE // Määritys lopetetaan aina tähän vakioon
};

// Ledipinni
PIN_Config ledConfig[] = {
   Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, 
   PIN_TERMINATE // Määritys lopetetaan aina tähän vakioon
};

//MPU pinni
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};


// MPU9250 I2C CONFIG

static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

//MPU sensorin akselit
float data_ax[100];
float data_ay[100];
float data_az[100];
float data_gx[100];
float data_gy[100];
float data_gz[100];

//Globaalit muuttujat
int ele = 0; //HighFive = 1, Nyrkki = 2
int viesti_lahetetty = 0; //viestiä ei ole lähetetty = 0, viesti on lähetetty = 1

char str_viesti[16]; // viestipuskuri


//Lukee sensoridataa ja tallentaa sen globaaleihin taulukoihin.
//Tunnistaa eleen sensoridatan perusteella ja asettaa eleen arvon globaalimuuttujaan "ele", 
//jonka mahdolliset arvot ovat 0, 1 ja 2.
//Tulostaa näytölle tunnistetun eleen ja lähettää viestin, jos ele on tunnitettu.
Void labTaskFxn(UArg arg0, UArg arg1) {
    
    I2C_Handle i2cMPU; // INTERFACE FOR MPU9250
    I2C_Params i2cMPUParams;
    
	float ax, ay, az, gx, gy, gz;
	uint16_t i = 0;
    int ajastin = 0;
    char str_akselin_arvo[50];
    char str_ele[15];
    int y; //muuttuja for-loopissa
    int laskuri = 0;
    
    /* Create I2C for sensors */
    
    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;
    
    // *******************************
    //
    // MPU OPEN I2C
    //
    // *******************************
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL) {
        System_abort("Error Initializing I2CMPU\n");
    }

    // *******************************
    //
    // MPU POWER ON
    //
    // *******************************
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

    // WAIT 100MS FOR THE SENSOR TO POWER UP
	Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // *******************************
    //
    // MPU9250 SETUP AND CALIBRATION
    //
    // *******************************
	System_printf("MPU9250: Setup and calibration...\n");
	System_flush();

	mpu9250_setup(&i2cMPU);

	System_printf("MPU9250: Setup and calibration OK\n");
	System_flush();
    
    //**************************************************************************
    
    while (1) {
        
    switch (myState) {
        
        case DO_NOTHING:
            //Do nothing here 
            //Wait 10 ms (0.01 s)
            Task_sleep(10000 / Clock_tickPeriod);
            break;
        
        case MSG_WAITING:
            //Do nothing here 
            //Wait 10 ms (0.01 s)
            Task_sleep(10000 / Clock_tickPeriod);
            break;
        
        case READ_SENSOR:
            
            while (i <= 100) {
                
                
        	    // *******************************
        	    //
        	    // MPU ASK DATA
        		//
                //    Accelerometer values: ax,ay,az
        	 	//    Gyroscope values: gx,gy,gz
        		//
        	    // *******************************
        		mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);

        		data_ax[i] = ax;
        		data_ay[i] = ay;
        		data_az[i] = az;
        		data_gx[i] = gx;
        		data_gy[i] = gy;
        		data_gz[i] = gz;


                i++;
        	    
            	Task_sleep(10000 / Clock_tickPeriod);
            }
            
            //nollaa i
            i = 0;
            
            //tunnistetaan eleet käymällä läpi sensoridatan taulukoita
            for (y = 0; y <= 100; y++) {
                
                //MPU sensorin akseleiden arvojen tulostus
        		/*
        		sprintf(str_akselin_arvo, "%d, %f, %f, %f, %f, %f, %f\n", laskuri, data_ax[y], data_ay[y], data_az[y], data_gx[y], data_gy[y], data_gz[y]);
                System_printf(str_akselin_arvo);
                System_flush();
                */
                
                //Eleiden tunnistus
                
                if (data_az[laskuri] >= 2){
                    ajastin++;
                        if (ajastin == 3){
                            ele = 1;

                        }
                }
                
            
                
                else if (data_ax[laskuri] >= 3){
                    ajastin++;
                        if (ajastin == 3){
                            ele = 2;
                        }
                }
                
                
                if (laskuri == 100){
                
                //nollaa laskuri ja ajastin   
                laskuri = 0;
                ajastin = 0;
                
                }
                
                laskuri++;
            }
            
            
            //Reaktiot tunnitettuihin eleisiin
            if (ele == 1){
        
                Display_clear(hDisplay);
                sprintf(str_ele, "HighFive!");
                Display_print0(hDisplay, 0, 0, str_ele);
                
                //viestin lähetys
                char payload[16] = "HighFive! -R&R";
                if(viesti_lahetetty == 0){
                Send6LoWPAN(IEEE80154_BROADCAST, payload, strlen(payload));
                 // Hox! Radio aina takaisin vastaanottotilaan ao. funktiokutssulla
                StartReceive6LoWPAN();
                viesti_lahetetty = 1;
                
                }
                
                
            }
            
            else if (ele == 2){

                Display_clear(hDisplay);
                sprintf(str_ele, "NYRKKI");
                Display_print0(hDisplay, 0, 0, str_ele);
                
                char payload[16] = "NYRKKI! -R&R";
                if(viesti_lahetetty == 0){
                Send6LoWPAN(IEEE80154_BROADCAST, payload, strlen(payload));
                 // Hox! Radio aina takaisin vastaanottotilaan ao. funktiokutssulla
                StartReceive6LoWPAN();
                viesti_lahetetty = 1;
                
                }
            }
            
            else if(ele == 0){
            
                Display_clear(hDisplay);
                sprintf(str_ele, "Ei tunnitettu.");
                Display_print0(hDisplay, 0, 0, str_ele);

            }
        	
        	break;
     
        }   	
    }
    
    

}

    /* Communication Task */
    Void commTaskFxn(UArg arg0, UArg arg1) {

    //char payload[16]; // viestipuskuri
    uint16_t senderAddr;

      
    // Radio to receive mode
	int32_t result = StartReceive6LoWPAN();
	if(result != true) {
		System_abort("Wireless receive mode failed");
	}

    while (1) {

        // If true, we have a message
    	if (GetRXFlag() == true) {
    	
		   // Tyhjennetään puskuri (ettei sinne jäänyt edellisen viestin jämiä)
           memset(str_viesti,0,16);
           // Luetaan viesti puskuriin payload
           Receive6LoWPAN(&senderAddr, str_viesti, 16);
           
            }
            
        }

    	// Absolutely NO Task_sleep in this task!!
    }
    
    
    /* screen task*/
    Void screenTaskFxn(UArg arg0, UArg arg1) {
        
	char str1[16];
	char str2[16];
	char str3[16];
	char str4[16];
	char str5[16];
	char str6[16];
	char str7[16];
	
	uint16_t senderAddr;
    
    /* Display */
    Display_Params displayParams;
	displayParams.lineClearMode = DISPLAY_CLEAR_BOTH;
    Display_Params_init(&displayParams);

    hDisplay = Display_open(Display_Type_LCD, &displayParams);
    if (hDisplay == NULL) {
        System_abort("Error initializing Display\n");
    } else {
        System_printf("Naytto ok");
        System_flush();
    }
    
    //Tulosta ohjeet näytölle
    Display_clear(hDisplay);
    sprintf(str1,"Laite tunnistaa"); 
    sprintf(str2, "eleita ja vas-");
    sprintf(str3, "taanottaa vies-");
    sprintf(str4, "teja.");
    sprintf(str5, "Vaihda tilaa");
    sprintf(str6, "napinpainalluk-");
    sprintf(str7, "sella.");
    
    Display_print0(hDisplay, 0, 0, str1);
    Display_print0(hDisplay, 1, 0, str2);
    Display_print0(hDisplay, 2, 0, str3);
    Display_print0(hDisplay, 3, 0, str4);
    Display_print0(hDisplay, 4, 0, str5);
    Display_print0(hDisplay, 5, 0, str6);
    Display_print0(hDisplay, 6, 0, str7);
    
    
    while (1) {
    
    switch (myState) {
        
        case DO_NOTHING:
                    
            //Do nothing
            
            //Wait 10 ms (0.01 s)
            Task_sleep(10000 / Clock_tickPeriod);
            break;
            
        case READ_SENSOR:
            //Do nothing here 
            //Wait 10 ms (0.01 s)
            Task_sleep(10000 / Clock_tickPeriod);
            break;
        
        case MSG_WAITING:
           
           //Tulostetaan viesti näytölle
            Display_clear(hDisplay);
            Display_print0(hDisplay, 0, 0, str_viesti);
            
            Task_sleep(1000000 / Clock_tickPeriod);
        	
        	break;
            
            }     
    
        }
    
    }

    // Napinpainalluksen (button1) käsittelijäfunktio
    //Virtakytkin
    Void buttonShutFxn(PIN_Handle handle, PIN_Id pinId) {
    
       // Näyttö pois päältä
       Display_clear(hDisplay);
       Display_close(hDisplay);
       Task_sleep(100000 / Clock_tickPeriod);

       PIN_close(hButtonShut);
       PINCC26XX_setWakeup(buttonWake);
       Power_shutdown(NULL,0);
    }

    // Napinpainalluksen (button0) käsittelijäfunktio

    Void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
        
   //Asetetaan tilakone tilaan READ_SENSOR
   if (myState == DO_NOTHING){
       myState = READ_SENSOR;
    } 
   
   //Asetetaan tilakone tilaan MSG_WAITING
   else if (myState == READ_SENSOR){
       myState = MSG_WAITING;
    }
   
   //Asetetaan tilakone tilaan READ_SENSOR
    else if (myState == MSG_WAITING){
       myState = READ_SENSOR;
       
       //palautetaan eleen arvoksi 0 ja viestien lähetyksen arvoksi 0
       ele = 0;
       viesti_lahetetty = 0;
    }

   // Vaihdetaan led-pinnin tilaa negaatiolla
   PIN_setOutputValue( ledHandle, Board_LED1, !PIN_getOutputValue( Board_LED1 ) );
   
   // Lähetetään viesti napin painalluksesta
    char payload[16] = "Hello world! R&R";
    Send6LoWPAN(IEEE80154_SERVER_ADDR, payload, strlen(payload));
     // Hox! Radio aina takaisin vastaanottotilaan ao. funktiokutssulla
    StartReceive6LoWPAN();
    }


Int main(void) {

    // Task variables
	Task_Handle labTask;
	Task_Params labTaskParams;
	Task_Handle commTask;
	Task_Params commTaskParams;
	Task_Handle screenTask;
	Task_Params screenTaskParams;

    // Initialize board
    Board_initGeneral();
    Board_initI2C();

   //Otetaan pinnit käyttöön ohjelmassa
   
    hButtonShut = PIN_open(&bStateShut, buttonShut);
    if( !hButtonShut ) {
      System_abort("Error initializing button shut pins\n");
    }
    if (PIN_registerIntCb(hButtonShut, &buttonShutFxn) != 0) {
      System_abort("Error registering button callback function");
    }

   buttonHandle = PIN_open(&buttonState, buttonConfig);
   if(!buttonHandle) {
      System_abort("Error initializing button pins\n");
   }
   ledHandle = PIN_open(&ledState, ledConfig);
   if(!ledHandle) {
      System_abort("Error initializing LED pins\n");
   }
   
   hMpuPin = PIN_open(&MpuPinState, MpuPinConfig);
    if (hMpuPin == NULL) {
    	System_abort("Pin open failed!");
    }
   
	// Register the interrupt handler for the button

     if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
       System_abort("Error registering button callback function");
    }
   
   
    /* Tasks */
    Task_Params_init(&labTaskParams);
    labTaskParams.stackSize = 4096;
    labTaskParams.stack = &labTaskStack;
    labTaskParams.priority=2;

    labTask = Task_create(labTaskFxn, &labTaskParams, NULL);
    if (labTask == NULL) {
    	System_abort("Task create failed!");
    }
    
    /* Communication Task */
    Init6LoWPAN();

    Task_Params_init(&commTaskParams);
    commTaskParams.stackSize = STACKSIZE;
    commTaskParams.stack = &commTaskStack;
    commTaskParams.priority=1;

    commTask = Task_create(commTaskFxn, &commTaskParams, NULL);
    if (commTask == NULL) {
    	System_abort("Task create failed!");
    }
    
    /* Screen Task */
    Task_Params_init(&screenTaskParams);
    screenTaskParams.stackSize = STACKSIZE;
    screenTaskParams.stack = &screenTaskStack;
    screenTaskParams.priority=2;

    screenTask = Task_create(screenTaskFxn, &screenTaskParams, NULL);
    if (screenTask == NULL) {
    	System_abort("Task create failed!");
    }

    System_printf("Hello world!\n");
    System_flush();
    
    /* Start BIOS */
    BIOS_start();

    return (0);
}

