// #include <stdio.h>
// #include <string.h>
// #include "stdlib.h"
// #include "lierda_app_main.h"
// #include "liot_os.h"

// #include "test.h"



// #if LIOT_FLOAT_TEST

// #define FLOAT_TASK_STACK_SIZE   (2048 + 4)
// static liot_StaticTask_t float_task_mem;
// __ALIGNED(8) static uint8_t float_task_stack[FLOAT_TASK_STACK_SIZE];
// static liot_task_t g_floattaskRef = NULL;

// void float_demo_Task( void *pvParameters )
// {
//     //int count = 5;
//     float a = 3.14;
//     double b = 3.1445566;
    
//     while(1)
//     {
//         liot_trace("test task, a = %.6f, b = %.6lf", a, b);
//         liot_rtos_task_sleep_ms(4000);  
//     }

//     liot_rtos_task_delete(g_floattaskRef);
// }

// void liot_create_float_task(void)
// {
//     LiotOSStatus_t result = liot_rtos_task_create_static(
//                                 &g_floattaskRef,
//                                 FLOAT_TASK_STACK_SIZE,
//                                 APP_PRIORITY_NORMAL,
//                                 "float_demo_Task",
//                                 float_demo_Task,
//                                 float_task_stack,
//                                 &float_task_mem,
//                                 NULL);
//     if(result == 0)
//     {
//         liot_trace("demo task create success %d", result);
//     }
//     else
//     {
//         liot_trace("demo task create fail %d", result);
//     }          
// }
// #endif