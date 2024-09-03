#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include "ethercat.h"



#define READY_To_SWITCH_ON 6
#define ENABLE_OPERATION_CONTRIL_WORD 15
#define HOMING_OPERATION_MODE 6
#define HOMING_CONTROL_WORD 31
#define ENABLE_OPERATION_MODE 1
#define OPERATION_STATUS_WORD 34359
#define HOMING_STATUS_WORD 38455
#define MOVING_CONTROL_WORD 63




int initialize_ethercat(char *ifname);

 //static int ethercat_loop_counter = 0;

void ethercat_loop(void);

void slaveinfo(char *ifname);

OSAL_THREAD_FUNC ecatcheck(void *ptr);

void si_sdo(int cnt);

int si_map_sdo(int slave);

int si_map_sii(int slave);

int si_PDOassign(uint16 slave, uint16 PDOassign, int mapoffset, int bitoffset);

int si_siiPDO(uint16 slave, uint8 t, int mapoffset, int bitoffset);

char *SDO2string(uint16 slave, uint16 index, uint8 subidx, uint16 dtype);

char *dtype2string(uint16 dtype, uint16 bitlen);

char *otype2string(uint16 otype);

char *access2string(uint16 access);

void save_sdo_pdo_to_file(const char *output);
