#include <project.h>
#include <stdbool.h>
bool state = true;
bool state2 = true;

#define LIGHT_OFF                       (0u)
#define LIGHT_ON                        (1u)

/* Interrupt prototypes */
CY_ISR_PROTO(WDTIsrHandler);

//#define SLEEP_INTERVAL              1000                      /* millisecond */
#define ILO_FREQ                    32000                       /* Hz */
#define LOG_ROW_INDEX               (CY_FLASH_NUMBER_ROWS - 1)  /* last row */

typedef enum {
    ACTIVE,
    WAKEUP_SLEEP,
    WAKEUP_DEEPSLEEP,
    SLEEP,
    DEEPSLEEP
} ApplicationPower;

ApplicationPower applicationPower;

int apiResult;

uint8_t flash_data_to_write[CY_SFLASH_SIZEOF_USERROW] = {0};

uint16_t stim_period = 6554; // 6 Hz
uint8_t stim_current = 0;


uint8_t moduleID = 1;
uint16_t writes_so_far = 0;

extern CYBLE_GAPP_DISC_MODE_INFO_T cyBle_discoveryModeInfo;

void UpdateStimInterval()
{
    /*==============================================================================*/
    /* configure counter 0 for wakeup interrupt                                     */
    /*==============================================================================*/
    /* Counter 0 of Watchdog time generates peridically interrupt to wakeup system */
    //CySysWdtWriteMode(CY_SYS_WDT_COUNTER0, CY_SYS_WDT_MODE_INT);
    /* Set interval as desired value */
	//CySysWdtWriteMatch(CY_SYS_WDT_COUNTER0, ((uint32)(sleep_interval * ILO_FREQ) / 1000));
    CySysWdtDisable(CY_SYS_WDT_COUNTER0_MASK);
	CySysWdtWriteMatch(CY_SYS_WDT_COUNTER0, stim_period);
    CySysWdtClearInterrupt(CY_SYS_WDT_COUNTER0_INT);   
    /* clear counter on match event */
	//CySysWdtWriteClearOnMatch(CY_SYS_WDT_COUNTER0, 1u);
    
    /*==============================================================================*/
    /* enable watchdog                                                              */
    /*==============================================================================*/
    /* enable the counter 0 */
    CySysWdtEnable(CY_SYS_WDT_COUNTER0_MASK);
    
    /* check if counter 0 is enabled, otherwise keep looping here */
    while(!CySysWdtReadEnabledStatus(CY_SYS_WDT_COUNTER0));
    
}

void WriteParametersToFlash() {                
    writes_so_far += 1;                
    *(uint16_t *)&flash_data_to_write[0] = writes_so_far; // update flash_data_to_write matrix with current values
    *(uint16_t *)&flash_data_to_write[2] = stim_period; // stim period
    flash_data_to_write[5] = stim_current; // stim current
    flash_data_to_write[6] = moduleID; // module ID
    CySysSFlashWriteUserRow(1, flash_data_to_write);
    
    CYBLE_GATT_HANDLE_VALUE_PAIR_T pair = { { (uint8 *)&writes_so_far, 2, 2}, CYBLE_PARAMS_FLASHWRITES_CHAR_HANDLE };
    CyBle_GattsWriteAttributeValue( &pair, 0, &cyBle_connHandle, CYBLE_GATT_DB_LOCALLY_INITIATED );
}

void AppCallBack(uint32 event, void* eventParam) 
{ 
    CYBLE_BLESS_CLK_CFG_PARAMS_T clockConfig;
    CYBLE_GATTS_WRITE_REQ_PARAM_T *wrReqParam;
     
    switch(event) 
    { 
      /* Handle stack events */ 
      case CYBLE_EVT_STACK_ON: 
 
          /* C8. Get the configured clock parameters for BLE subsystem */ 
          CyBle_GetBleClockCfgParam(&clockConfig);     
             
          /* C8. Set the device sleep-clock accuracy (SCA) based on the tuned ppm  
          of the WCO */ 
          clockConfig.bleLlSca = CYBLE_LL_SCA_000_TO_020_PPM; 
             
          /* C8. Set the clock parameter of BLESS with updated values */ 
          CyBle_SetBleClockCfgParam(&clockConfig); 
 
          /* Put the device into discoverable mode so that a Central device can   
          connect to it. */ 
          apiResult = CyBle_GappStartAdvertisement(CYBLE_ADVERTISING_FAST); 
           
          /* Application-specific event handling here */ 
        
        /*Probably we receive a RX Buff, with the transmiter data from GATT*/
        
        break; 
    
      /* Other application-specific event handling here */ 
      case CYBLE_EVT_GAP_DEVICE_CONNECTED: 
        LED_Write(1);
        break;
      
        // Handle a write request
    case CYBLE_EVT_GATTS_WRITE_REQ:
        wrReqParam = (CYBLE_GATTS_WRITE_REQ_PARAM_T *) eventParam;

        // Request write the frequency value
        if (wrReqParam->handleValPair.attrHandle == CYBLE_PARAMS_FREQUENCY_CHAR_HANDLE)
        {
            // Only update the value and write the response if the requested write is allowed
            //if (CYBLE_GATT_ERR_NONE == CyBle_GattsWriteAttributeValue(&wrReqParam->handleValPair, 0, &cyBle_connHandle, CYBLE_GATT_DB_PEER_INITIATED))
            if (CYBLE_GATT_ERR_NONE == CyBle_GattsWriteAttributeValue(&wrReqParam->handleValPair, 0, &wrReqParam->connHandle, CYBLE_GATT_DB_PEER_INITIATED))
            {                
                stim_period = wrReqParam->handleValPair.value.val[0] + ((uint16_t) wrReqParam->handleValPair.value.val[1] << 8);   
                UpdateStimInterval();
                
                WriteParametersToFlash();
                
                CyBle_GattsWriteRsp(cyBle_connHandle);
            }
        }
        else if (wrReqParam->handleValPair.attrHandle == CYBLE_PARAMS_CURRENT_CHAR_HANDLE)
        {
            // Only update the value and write the response if the requested write is allowed
            //if (CYBLE_GATT_ERR_NONE == CyBle_GattsWriteAttributeValue(&wrReqParam->handleValPair, 0, &cyBle_connHandle, CYBLE_GATT_DB_PEER_INITIATED))
            if (CYBLE_GATT_ERR_NONE == CyBle_GattsWriteAttributeValue(&wrReqParam->handleValPair, 0, &wrReqParam->connHandle, CYBLE_GATT_DB_PEER_INITIATED))
            {
   
                stim_current = wrReqParam->handleValPair.value.val[0];

                WriteParametersToFlash();

                CyBle_GattsWriteRsp(cyBle_connHandle);
            }
        }
        
        else if (wrReqParam->handleValPair.attrHandle == CYBLE_PARAMS_MODULEID_CHAR_HANDLE)
        {
            // Only update the value and write the response if the requested write is allowed
            //if (CYBLE_GATT_ERR_NONE == CyBle_GattsWriteAttributeValue(&wrReqParam->handleValPair, 0, &cyBle_connHandle, CYBLE_GATT_DB_PEER_INITIATED))
            if (CYBLE_GATT_ERR_NONE == CyBle_GattsWriteAttributeValue(&wrReqParam->handleValPair, 0, &wrReqParam->connHandle, CYBLE_GATT_DB_PEER_INITIATED))
            {
   
                moduleID = wrReqParam->handleValPair.value.val[0];

                WriteParametersToFlash();
                
                CyBle_GattsWriteRsp(cyBle_connHandle);
            }
        }
        break;
            
      case CYBLE_EVT_GAP_DEVICE_DISCONNECTED: 
        LED_Write(0);
        apiResult = CyBle_GappStartAdvertisement(CYBLE_ADVERTISING_FAST); 
        break;
    } 
} 

int enable_deepsleep = 1;
inline void RunApplication() 
{ 
    /* if you are done with everything and ready to go to deepsleep,  
    then set it up to go to deepsleep. Update the code inside if() specific 
    to your application*/   
    if ( enable_deepsleep == 1 ) 
    { 
        applicationPower = DEEPSLEEP; 
    } 
} 

void ManageApplicationPower() 
{ 
    switch(applicationPower) 
    { 
        case ACTIVE: // don’t need to do anything 
        break; 
         
        case WAKEUP_SLEEP: // do whatever wakeup needs to be done  
         
            applicationPower = ACTIVE; 
        break; 
         
        case WAKEUP_DEEPSLEEP: // do whatever wakeup needs to be done. 
         
            applicationPower = ACTIVE; 
        break; 
 
        case SLEEP:  
        /*********************************************************************** 
         * Place code to place the application components to sleep here  
        ************************************************************************/         
        break; 
         
        case DEEPSLEEP: 
        /*********************************************************************** 
        *  Place code to place the application components to deepsleep here  
        ************************************************************************/      
         
        break; 
    } 
}   

void ManageSystemPower() 
{ 
    
    /* Variable declarations */ 
    CYBLE_BLESS_STATE_T blePower; 
    uint8 interruptStatus ; 
     
    /* Disable global interrupts to avoid any other tasks from interrupting this 
section of code*/ 
    interruptStatus  = CyEnterCriticalSection(); 
     
    /* C9. Get current state of BLE sub system to check if it has successfully 
entered deep sleep state */ 
    blePower = CyBle_GetBleSsState(); 
     
    /* C10. System can enter Deep-Sleep only when BLESS and rest of the application 
are in DeepSleep or equivalent power modes */ 
    if((blePower == CYBLE_BLESS_STATE_DEEPSLEEP || blePower == 
CYBLE_BLESS_STATE_ECO_ON) && applicationPower == DEEPSLEEP) 
    { 
        applicationPower = WAKEUP_DEEPSLEEP; 
        /* C11. Put system into Deep-Sleep mode*/ 
        CySysPmDeepSleep(); 
    } 
    /* C12. BLESS is not in Deep Sleep mode. Check if it can enter Sleep mode */ 
    else if((blePower != CYBLE_BLESS_STATE_EVENT_CLOSE)) 
    {         
        /* C13. Application is in Deep Sleep. IMO is not required */ 
        if(applicationPower == DEEPSLEEP) 
        { 
            applicationPower = WAKEUP_DEEPSLEEP; 
             
            /* C14. change HF clock source from IMO to ECO*/ 
            CySysClkWriteHfclkDirect(CY_SYS_CLK_HFCLK_ECO);  
            /* C14. stop IMO for reducing power consumption */ 
            CySysClkImoStop();  
            /*C15. put the CPU to sleep */ 
            CySysPmSleep(); 
            /* C16. starts execution after waking up, start IMO */ 
            CySysClkImoStart(); 
            /* C16. change HF clock source back to IMO */ 
            CySysClkWriteHfclkDirect(CY_SYS_CLK_HFCLK_IMO); 
        } 
        /*C17. Application components need IMO clock */ 
        else if(applicationPower == SLEEP ) 
        { 
            /* C18. Put the system into Sleep mode*/ 
            applicationPower = WAKEUP_SLEEP; 
            CySysPmSleep(); 
        }
    } 
     
    /* Enable interrupts */ 
    CyExitCriticalSection(interruptStatus ); 
}

uint32_t system_uptime;
CYBLE_GATT_HANDLE_VALUE_PAIR_T uptimeHandleValuePair;


inline void mux_ground() {
    Electrode_0_SetDriveMode(Electrode_0_DM_STRONG);
    CY_SET_REG32(Electrode_0__0__HSIOM, (CY_GET_REG32(Electrode_0__0__HSIOM) & ~(Electrode_0__0__HSIOM_MASK)));
    Electrode_1_SetDriveMode(Electrode_1_DM_STRONG);
    CY_SET_REG32(Electrode_1__0__HSIOM, (CY_GET_REG32(Electrode_1__0__HSIOM) & ~(Electrode_1__0__HSIOM_MASK)));
    Electrode_0_Write(0);
    Electrode_1_Write(0);
}

inline void mux_reverse() {
    Electrode_0_SetDriveMode(Electrode_0_DM_STRONG);
    CY_SET_REG32(Electrode_0__0__HSIOM, (CY_GET_REG32(Electrode_0__0__HSIOM) & ~(Electrode_0__0__HSIOM_MASK)));
    Electrode_0_Write(0);
    
    Electrode_1_SetDriveMode(Electrode_1_DM_ALG_HIZ);
    CY_SET_REG32(Electrode_1__0__HSIOM, (CY_GET_REG32(Electrode_1__0__HSIOM) & ~(Electrode_1__0__HSIOM_MASK)));
    CY_SET_REG32(Electrode_1__0__HSIOM, (CY_GET_REG32(Electrode_1__0__HSIOM) | (0x06 << Electrode_1__0__HSIOM_SHIFT)));
}

inline void mux_forward() {
    Electrode_1_SetDriveMode(Electrode_1_DM_STRONG);
    CY_SET_REG32(Electrode_1__0__HSIOM, (CY_GET_REG32(Electrode_1__0__HSIOM) & ~(Electrode_1__0__HSIOM_MASK)));
    Electrode_1_Write(0);
    
    Electrode_0_SetDriveMode(Electrode_0_DM_ALG_HIZ);
    CY_SET_REG32(Electrode_0__0__HSIOM, (CY_GET_REG32(Electrode_0__0__HSIOM) & ~(Electrode_0__0__HSIOM_MASK)));
    CY_SET_REG32(Electrode_0__0__HSIOM, (CY_GET_REG32(Electrode_0__0__HSIOM) | (0x06 << Electrode_0__0__HSIOM_SHIFT)));
}



int main()
{
    LED2_Write(1); // Turn on LED2 (red)
    LED_Write(0); // Turn off LED1 (green)
    
    
    // Setup IDAC
    IDAC_1_Start();     // Initialize the IDAC
    IDAC_1_Sleep();
    

    LED2_Write(0); // red off
    
    // Read data from flash in case it's been programmed in.
    uint8_t  *user_sflash = (void *)(CY_SFLASH_USERBASE + CY_SFLASH_SIZEOF_USERROW); // This should point at the second row of User SFlash
    uint16_t flash_writes_so_far = *(uint16_t *)(user_sflash); // first thing stored is writes_so_far
    uint16_t flash_stim_period = *(uint16_t *)(user_sflash + 2); // stim period is stored after writes_so_far
    uint8_t  flash_stim_current = *(uint8_t *)(user_sflash + 4); // next is stim_current, which is just 1 byte
    uint8_t  flash_moduleID = *(uint8_t *)(user_sflash + 5); // finally is module ID

    
    if (flash_writes_so_far > 0) { // data has been initialized
        LED_Write(0); // green off
        writes_so_far = flash_writes_so_far;
        stim_period = flash_stim_period;
        if (stim_period < 160) // error check bounds
            stim_period = 160;
        stim_current = flash_stim_current;
        if (stim_current > 200)
            stim_current = 200;
        moduleID = flash_moduleID;
    }
    else {
        LED_Write(1); // green on
    }

    
    system_uptime = 0;
    uptimeHandleValuePair.value.val = (uint8 *)&system_uptime;
    uptimeHandleValuePair.value.len = 4;
    uptimeHandleValuePair.attrHandle = CYBLE_PARAMS_UPTIME_CHAR_HANDLE;
    
    /*==============================================================================*/
    /* configure WDT2 interrupt to track uptime                                     */
    /*==============================================================================*/
    CySysWdtWriteMode(CY_SYS_WDT_COUNTER2, CY_SYS_WDT_MODE_INT);
    /* Set interval as desired value */
	CySysWdtWriteToggleBit(15); // 32.768 kHz clock will generate a 1Hz interrupt based on bit 15 (I think?)
    
    /* Enable WDT counter 2 */
    CySysWdtEnable(CY_SYS_WDT_COUNTER2_MASK);
    while(!CySysWdtReadEnabledStatus(CY_SYS_WDT_COUNTER2));
    
    /*==============================================================================*/
    /* configure counter 0 to interrupt for stimulation pulses                      */
    /*==============================================================================*/
    /* Counter 0 of Watchdog time generates peridically interrupt to wakeup system */
    CySysWdtWriteMode(CY_SYS_WDT_COUNTER0, CY_SYS_WDT_MODE_INT);
    /* Set interval as desired value */
	CySysWdtWriteMatch(CY_SYS_WDT_COUNTER0, stim_period);
    /* clear counter on match event */
	CySysWdtWriteClearOnMatch(CY_SYS_WDT_COUNTER0, 1u);
    
    /* Enable both WDT counter 0 */
    CySysWdtEnable(CY_SYS_WDT_COUNTER0_MASK);
    
    /* wait until updated otherwise keep looping here */
    while(!CySysWdtReadEnabledStatus(CY_SYS_WDT_COUNTER0));
    
    
    /* initialize stimulation frequency WDT interrupt */
    //InitWatchdog(5000);
    /* connect ISR routine to Watchdog interrupt */
    isr_1_StartEx(WDTIsrHandler); // The "isr_WDT" prefix comes from our TopDesign
    /* set the highest priority to make ISR is executed in all condition */
    isr_1_SetPriority(0);
    
    
    
     /* Variable declarations */
     CYBLE_LP_MODE_T lpMode;
     CYBLE_BLESS_STATE_T blessState;
     uint8 interruptStatus;
     /* Enable global interrupts */
     CyGlobalIntEnable;
    
    LED2_Write(1); // red on
    // LED_Write(1); // green on

     /* C1. Stop the ILO to reduce current consumption */
     CySysClkIloStop();
     /* C2. Configure the divider values for the ECO, so that a 3-MHz ECO divided 
     clock can be provided to the CPU in Sleep mode */
     CySysClkWriteEcoDiv(CY_SYS_CLK_ECO_DIV8);
    
    
    // TODO: CyBle_Gap_Set_LocalName();
     
     /* Start the BLE Component and register the generic event handler */
     apiResult = CyBle_Start(AppCallBack);
     
     /* Wait for BLE Component to initialize */
     while (CyBle_GetState() == CYBLE_STATE_INITIALIZING)
     {
     CyBle_ProcessEvents();
    
     } 
     /*Application-specific Component and other initialization code below */
     applicationPower = ACTIVE;
    
    // Update all the BLE characteristics to match what we read from the flash
    CYBLE_GATT_HANDLE_VALUE_PAIR_T pair = { { (uint8 *)&stim_period, 2, 2}, CYBLE_PARAMS_FREQUENCY_CHAR_HANDLE };
    CyBle_GattsWriteAttributeValue( &pair, 0, &cyBle_connHandle, CYBLE_GATT_DB_LOCALLY_INITIATED );

    CYBLE_GATT_HANDLE_VALUE_PAIR_T pair2 = { { &stim_current, 1, 1}, CYBLE_PARAMS_CURRENT_CHAR_HANDLE };
    CyBle_GattsWriteAttributeValue( &pair2, 0, &cyBle_connHandle, CYBLE_GATT_DB_LOCALLY_INITIATED );

    CYBLE_GATT_HANDLE_VALUE_PAIR_T pair3 = { { &moduleID, 1, 1 }, CYBLE_PARAMS_MODULEID_CHAR_HANDLE };
    CyBle_GattsWriteAttributeValue( &pair3, 0, &cyBle_connHandle, CYBLE_GATT_DB_LOCALLY_INITIATED );

    CYBLE_GATT_HANDLE_VALUE_PAIR_T pair4 = { { (uint8 *)&flash_writes_so_far, 2, 2}, CYBLE_PARAMS_FLASHWRITES_CHAR_HANDLE };
    CyBle_GattsWriteAttributeValue( &pair4, 0, &cyBle_connHandle, CYBLE_GATT_DB_LOCALLY_INITIATED );

     
     /* main while loop of the application */
     while(1)
     {
         /* Process all pending BLE events in the stack */
         CyBle_ProcessEvents(); 
         /* C3. Call the function that manages the BLESS power modes */
         CyBle_EnterLPM(CYBLE_BLESS_DEEPSLEEP); 
         /*C4. Run your application specific code here */
         if(applicationPower == ACTIVE)
         {
            RunApplication();
         }
         
         /*C6. Manage Application power mode */
         ManageApplicationPower(); 
         
         /*C7. Manage System power mode */
         ManageSystemPower();
        
         if(CyBle_GetBleSsState() == CYBLE_BLESS_STATE_EVENT_CLOSE) {
            cyBle_discoveryModeInfo.advData->advData[28u] = moduleID;
            CyBle_GapUpdateAdvData(cyBle_discoveryModeInfo.advData, cyBle_discoveryModeInfo.scanRspData);
        }
        
    }
}

CY_ISR(WDTIsrHandler)
{
    static CYBLE_GATT_ERR_CODE_T errCode;

    
    if (CySysWdtGetInterruptSource() == CY_SYS_WDT_COUNTER2_INT) {
        system_uptime++;
        state2=!state2;
        
        if(state2){
            //LED2_Write(0);
        }
        else {  
            //LED2_Write(1);
        }
        
        errCode = CyBle_GattsWriteAttributeValue(&uptimeHandleValuePair, 0u, NULL, CYBLE_GATT_DB_LOCALLY_INITIATED);
        CySysWdtClearInterrupt(CY_SYS_WDT_COUNTER2_INT);   
    }
    
    else { // Stimulation!
        LED2_Write(1);
        
        IDAC_1_Wakeup();
        mux_forward();
        IDAC_1_SetValue(stim_current);
        CyDelayUs(57u);   // 60 us
        IDAC_1_SetValue(0);
        mux_ground();
        
        CyDelayUs(6u);   
        
        mux_reverse();
        IDAC_1_SetValue(stim_current); 
        CyDelayUs(57u);   
        IDAC_1_SetValue(0);
        mux_ground();
        
        IDAC_1_Sleep();
        LED2_Write(0);
      
        /* clear interrupt flag to enable next interrupt*/ 
        CySysWdtClearInterrupt(CY_SYS_WDT_COUNTER0_INT);   
    }
}