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
#define HOMING_STATUS_WORD 38455
#define MOVING_CONTROL_WORD 63
#define TARGET_1 (0)
#define TARGET_2 (-43000)
#define PROFILE_VELOCITY 1500
#define TARGET_VELOCITY  1500

#define TARGET_MARGIN 10

#define PROFILE_ACCELERATION_VALUE 15.0f 
#define PROFILE_DECELERATION_VALUE 15.0f   
#define PROFILE_JERK_VALUE 1100.0f 
#define CiA402Setpoints 0x216f




int initialize_ethercat(char *ifname);

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

void set_profile_acceleration(uint16_t CiA402Setpoints_index, float profile_acceleration);
void set_profile_deceleration(uint16_t CiA402Setpoints_index, float profile_deceleration);
void set_profile_jerk(uint16_t CiA402Setpoints_index, float profile_jerk);
void set_position_offeset(uint16_t CiA402Setpoints_index, int64_t position_offset);
