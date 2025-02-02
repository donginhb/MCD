/******************************************************************************/
/* 
 *                           TASK IMPLEMENTATION 
 *       				NO MORE THAN FUCKING INTERRUPTS
*/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
/* Body Identification                                                        */
/*----------------------------------------------------------------------------*/
#ifdef TASK_C
    #error "!!! FileName ID. It should be Unique !!!"
#else
    #define TASK_C
#endif
/*----------------------------------------------------------------------------*/
/* Included files to resolve specific definitions in this file                */
/*----------------------------------------------------------------------------*/
#include "includes.h"
#include "statemachine.h"
#include "measure.h"
#include "systmr.h"
#include "Control_OpenLoop.h"
#include "SVM.H"

/*----------------------------------------------------------------------------*/
/* Local constants                                                            */
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/* Local macros                                                               */
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/* Local types                                                                */
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/* Local data                                                                 */
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/* Constant local data                                                        */
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/* Exported data to other modules                                             */
/*----------------------------------------------------------------------------*/
struct sTaskRuntineParStruct 		sTimes;
struct sTaskCounterStruct			task_count;
U16 TASK_CONTROL;
/*----------------------------------------------------------------------------*/
/* Exported data from other modules                                           */
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/* Constant exported data                                                     */
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/* Local function prototypes                                                  */
/*----------------------------------------------------------------------------*/
void __attribute__((__interrupt__,no_auto_psv)) _INT1Interrupt(void);
void __attribute__((__interrupt__,no_auto_psv)) _INT2Interrupt(void);
static void Task2_Stack_Usage_Calc(struct sTaskRuntineParStruct *p);

/******************************************************************************/
/*                                                                            */
/*                   I N T E R F A C E   F U N C T I O N S                    */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/*
* Purpose: TASK1. 
* Input: none
* Output:  none
*/
/******************************************************************************/
void __attribute__((__interrupt__,no_auto_psv)) _INT1Interrupt(void)
{
	tsMeasure_Struct *ms;
	
	_set_task1_execute(1);
	
	/*------------------------------------------------------------------------*/
	/* Get TASK1 Period                                                       */
	/*------------------------------------------------------------------------*/
	_Task1_GetTime();
	
	//ADC_Process();		// Run ADC Measurement
	/*------------------------------------------------------------------------*/
	/* Measure Part 1                                                         */
	/*------------------------------------------------------------------------*/
	ms = measure( _get_adc_off() );
	
	/*------------------------------------------------------------------------*/
	/* Get Sign Of Inverter Currents. Used for dead time compensation         */
	/*------------------------------------------------------------------------*/
	Get_Current_Signs(ms);
	
	/*------------------------------------------------------------------------*/
	/* Run Application State Machine                                          */
	/*------------------------------------------------------------------------*/
	statemachine_contol();
	
	/*------------------------------------------------------------------------*/
	/* Measure Part 2                                                         */
	/*------------------------------------------------------------------------*/
	measure_part_2( ms, _get_adc_off() );
	
	/*------------------------------------------------------------------------*/
	/* Run Trace Functionality                                                */
	/*------------------------------------------------------------------------*/
	do_trace();
	
	/*------------------------------------------------------------------------*/
	/* Serial Communication Watchdog Timer                                    */
	/*------------------------------------------------------------------------*/
	sci_timer();
	
	
	User_LEDS_Control();
	
	
	/*------------------------------------------------------------------------*/
	/* Calculate Stack Usage                                                  */
	/*------------------------------------------------------------------------*/
	Task1_Stack_Usage_Calc(_get_sTimes(), 0);
	
	/*------------------------------------------------------------------------*/
	/* Get Value of Task Rumtime Measurement                                  */
	/*------------------------------------------------------------------------*/
	_Task1_GetRunTime();
	_Task1_Timer_Clear();
	
	_set_task1_execute(0);
	//_Task2_Timer_Start();
	_INT1IF = 0;					//Clear Interrupt flag
}

/******************************************************************************/
/*
* Purpose: TASK2
* Input: none
* Output: none
*/
/******************************************************************************/
void __attribute__((__interrupt__,no_auto_psv)) _INT2Interrupt(void)
{
	
	_set_task2_execute(1);
	
	/*------------------------------------------------------------------------*/
	/* Get TASK2 Period                                                       */
	/*------------------------------------------------------------------------*/
	_Task2_GetTime();
	
	/*------------------------------------------------------------------------*/
	/* Measure in TASK2                                                       */
	/*------------------------------------------------------------------------*/
	MeasureTransform_T2( _get_smes_t2(), _get_smes() );
	
	/*------------------------------------------------------------------------*/
	/* Calculate Stack Usage                                                  */
	/*------------------------------------------------------------------------*/
	Task2_Stack_Usage_Calc(_get_sTimes());
	
	/*------------------------------------------------------------------------*/
	/* Get Value of Task Rumtime Measurement                                  */
	/*------------------------------------------------------------------------*/
	_Task2_GetRunTime();
	_Task2_Timer_Clear();
	
	_set_task2_execute(0);
	_INT2IF = 0;					//Clear Interrupt flag
}
/******************************************************************************/
/*
* Purpose: Calc Task Runtimes.
* Input: Pointer to task runtimes data structure
* Output: none
*/
/******************************************************************************/
void TaskTimesCalc(struct sTaskRuntineParStruct *p)
{
	(p)->Task1RunTime_us = (U32)(cTcy_ns * (p)->Task1RunTicks);
	(p)->Task2RunTime_us = (U32)(((U32)cTcy_ns * TMR2_PRESCALER_INT) * (p)->Task2RunTicks);
	
	(p)->Task1Time_us = (U32)(cTcy_ns * (p)->Task1Ticks) + (p)->Task1RunTime_us;
	(p)->Task2Time_us = (U32)(((U32)cTcy_ns * TMR2_PRESCALER_INT) * (p)->Task2Ticks) + (p)->Task2RunTime_us;

}

/******************************************************************************/
/*
* Purpose: Calc Stack Usage
* Input: Pointer to task runtimes data structure
* Output: none
*/
/******************************************************************************/
void Task1_Stack_Usage_Calc(struct sTaskRuntineParStruct *p, bool init)
{
	register U16 w15_local asm( "w15");
	
	if(init)
	{
		(p)->Stack_Base = w15_local;
		(p)->Stack_Size = SPLIM - (p)->Stack_Base;
	}
	else
	{	
		(p)->Task1_Stack_Usage = w15_local - (p)->Stack_Base;
		(p)->Stack_Free  = SPLIM - w15_local;
	}

}

/******************************************************************************/
/*
* Purpose: Calc Stack Usage
* Input: Pointer to task runtimes data structure
* Output: none
*/
/******************************************************************************/
static void Task2_Stack_Usage_Calc(struct sTaskRuntineParStruct *p)
{
	register U16 w15_local asm( "w15");
	
	(p)->Task2_Stack_Usage = w15_local - (p)->Stack_Base;

}

/******************************************************************************/
/*
* Purpose: 
* Input: 
* Output:  
*/
/******************************************************************************/
void Task_Init(void)
{
	task_count.T2_count = T2_PERIOD;
	
	TASK_CONTROL = 0;
	
	_INT1IP = cISR_PRIORITY_INT1;
	_INT2IP = cISR_PRIORITY_INT2;
	
	_INT1IF = 0;
	_INT2IF = 0;
	
	_INT1IE = 1;
	_INT2IE = 1;
}
/******************************************************************************/
/******************************************************************************/
/*                                                                            */
/*                       L O C A L   F U N C T I O N S                        */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*
 * Name:      
 * Purpose:   
 * 
*/
/******************************************************************************/

