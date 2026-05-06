#ifndef __TASK_APE_H__
#define __TASK_APE_H__


void liot_create_aep_task(void);
void AEP_Task(void *pvParameters);


void SeverDataRead_ParamGet();
void SeverDataRead_SetLimit();
void SeverDataRead_ModlInfo();


void UartVoltageUpdateCheck();
int VoltageUpdateCheck_sum();
void compute_average_voltage();
void UartAlarmUpdateCheck();

#endif