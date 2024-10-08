#include "include/slaveinfo.h"
#include "include/main.h"

char IOmap[4096];
ec_ODlistt ODlist;
ec_OElistt OElist;
boolean printSDO = TRUE;
boolean printMAP = TRUE;
char usdo[128];
char hstr[1024];
boolean inOP;

#define OTYPE_VAR 0x0007
#define OTYPE_ARRAY 0x0008
#define OTYPE_RECORD 0x0009

#define ATYPE_Rpre 0x01
#define ATYPE_Rsafe 0x02
#define ATYPE_Rop 0x04
#define ATYPE_Wpre 0x08
#define ATYPE_Wsafe 0x10
#define ATYPE_Wop 0x20

int initialize_ethercat(char *ifname)
{
    int i;
    inOP = FALSE;
    int IO_map_size;
    int device_state;

    printf("ec: initialize_ethercat: start\n");

    /* Configure Ethercat master and bind socket to ifname */
    if (!ec_init(ifname))
    {
        printf("ec: initialize_ethercat: Failed to use %s - abort", ifname);
        ec_close();
        return -1;
    }

    printf("ec: initialize_ethercat: ec_init on %s succeeded.\n", ifname);

    /* Enumerate and init all slaves */
    if (ec_config_init(FALSE) <= 0)
    {
        printf("ec: initialize_ethercat: Failed to init slaves - abort\n");
        ec_close();
        return -1;
    }

    /* Set slaves in init state */
    ec_slave[0].state = EC_STATE_INIT + EC_STATE_ACK;
    ec_writestate(0);

    device_state = ec_statecheck(0, EC_STATE_INIT, EC_TIMEOUTSTATE * 1);

    if (device_state != EC_STATE_INIT)
    {
        printf("ec: initialize_ethercat: Unable to set device in boot state device_state=%d - abort\n", device_state);
        ec_close();
        return -1;
    }

    printf("ec: initialize_ethercat: init state reached\n");

    /* Set slaves in boot state and clear all errors (if any) */
    ec_slave[0].state = EC_STATE_BOOT + EC_STATE_ACK;
    ec_writestate(0);

    /* For some reason is this ready call needed */
    ec_readstate();

    device_state = ec_statecheck(0, EC_STATE_BOOT, EC_TIMEOUTSTATE * 1);

    if (device_state != EC_STATE_BOOT && device_state != EC_STATE_INIT)
    {
        printf("ec: initialize_ethercat: Unable to set device in boot or init state device_state=%d - abort\n", device_state);
        ec_close();
        return -1;
    }

    printf("ec: initialize_ethercat: boot/init state reached - device_state=%d\n", device_state);

    /* Set slaves in init state */
    ec_slave[0].state = EC_STATE_INIT + EC_STATE_ACK;
    ec_writestate(0);

    device_state = ec_statecheck(0, EC_STATE_INIT, EC_TIMEOUTSTATE * 1);

    if (device_state != EC_STATE_INIT)
    {
        printf("ec: initialize_ethercat: Unable to set device in init state device_state=%d - abort\n", device_state);
        ec_close();
        return -1;
    }

    printf("ec: initialize_ethercat: init state reached\n");

    /* Wait for slaves to come up */
    int temp_count = 0;
    do
    {
        ec_statecheck(0, EC_STATE_INIT, EC_TIMEOUTSTATE * 4);
        uint16_t w;
        temp_count = ec_BRD(0x0000, ECT_REG_TYPE, sizeof(w), &w, EC_TIMEOUTSAFE);
    } while (temp_count < ec_slavecount);

    printf("ec: initialize_ethercat: All slaves are back after init/boot/init seq.\n");

    /* Enumerate and init all slaves again after we have left boot state */
    if (ec_config_init(FALSE) <= 0)
    {
        printf("ec: initialize_ethercat: Failed to config init slaves - abort\n");
        ec_close();
        return -1;
    }

    printf("ec: initialize_ethercat: ec_config_init again after boot state\n");

    /* Set slaves in pre-op state */
    ec_slave[0].state = EC_STATE_PRE_OP;
    ec_writestate(0);

    device_state = ec_statecheck(0, EC_STATE_PRE_OP, EC_TIMEOUTSTATE * 1);

    if (device_state != EC_STATE_PRE_OP)
    {
        printf("ec: initialize_ethercat: Unable to set device in pre-op state device_state=%d - abort\n", device_state);

        for (int i = 1; i <= ec_slavecount; i++)
        {
            device_state = ec_statecheck(i, EC_STATE_PRE_OP, EC_TIMEOUTSTATE * 1);
            printf("\t slave %d state %d\n", i, device_state);
        }

        ec_close();
        return -1;
    }

    printf("ec: initialize_ethercat: pre-op state reached\n");

    /* Map all PDOs from slaves to IOmap with Outputs/Inputs in sequential order (legacy SOEM way).*/
    IO_map_size = ec_config_map(&IOmap);

    printf("\tec: initialize_ethercat: IO_map_size %d\n", IO_map_size);

    /* Check actual slave state. This is a blocking function. */
    device_state = ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);

    if (device_state != EC_STATE_SAFE_OP)
    {
        printf("ec: initialize_ethercat: Unable to set device in safe-op state device_state=%d - abort\n", device_state);
        ec_close();
        return -1;
    }

    printf("ec: initialize_ethercat: save-op state reached\n");

    /* Make sure all slaves are in safe-op */
    if (ec_slave[0].state != EC_STATE_SAFE_OP)
    {
        printf("ec: initialize_ethercat: Not all slaves reached safe operational state device_state= %d - abort\n", device_state);
        ec_readstate();
        for (i = 1; i <= ec_slavecount; i++)
        {
            if (ec_slave[i].state != EC_STATE_SAFE_OP)
            {
                printf("ec: initialize_ethercat: Slave %d State=%2x StatusCode=%4x : %s\n", i, ec_slave[i].state, ec_slave[i].ALstatuscode, ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
            }
        }

        ec_close();
        return -1;
    }

    printf("ec: initialize_ethercat: initialized\n");

    return 0;
}

void set_profile_acceleration(uint16_t CiA402Setpoints_index, float profile_acceleration)
{
    ec_SDOwrite(1, CiA402Setpoints_index, 0x06, FALSE, sizeof(profile_acceleration), &profile_acceleration, EC_TIMEOUTRXM);
}

void set_profile_deceleration(uint16_t CiA402Setpoints_index, float profile_deceleration)
{
    ec_SDOwrite(1, CiA402Setpoints_index, 0x07, FALSE, sizeof(profile_deceleration), &profile_deceleration, EC_TIMEOUTRXM);
}

void set_profile_jerk(uint16_t CiA402Setpoints_index, float profile_jerk)
{
    ec_SDOwrite(1, CiA402Setpoints_index, 0x09, FALSE, sizeof(profile_jerk), &profile_jerk, EC_TIMEOUTRXM);
}

void set_position_offeset(uint16_t CiA402Setpoints_index, int64_t position_offset)
{
     ec_SDOwrite(1, CiA402Setpoints_index, 0x12, FALSE, sizeof(position_offset), &position_offset, EC_TIMEOUTRXM);
}

uint64_t get_current_time_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts); // Use CLOCK_MONOTONIC for elapsed time
    return (uint64_t)(ts.tv_sec) * 1000 + (ts.tv_nsec / 1000000); // Convert to milliseconds
}

static int ethercat_loop_counter = 0;
void ethercat_loop(void)
{
    int i, chk;
    int slave_state;
    inOP = FALSE;
    int expectedWKC;
    volatile int wkc;

    printf("ec: ethercat_loop: start\n");

    /* Ethercat house keeping to get it going in operational state */
    expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
    printf("ec: ethercat_loop: Calculated workcounter %d\n", expectedWKC);
    ec_slave[0].state = EC_STATE_OPERATIONAL;
    /* send one valid process data to make outputs in slaves happy */
    ec_send_processdata();
    ec_receive_processdata(EC_TIMEOUTRET);
    /* request OP state for all slaves */
    ec_writestate(0);

    /* Initialize number of checks before giving up */
    chk = 40;

    /* wait for all slaves to reach operational state */
    do
    {
        ec_send_processdata();
        ec_receive_processdata(EC_TIMEOUTRET);
        slave_state = ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
    } while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL));

    printf("ec: ethercat_loop: All slaves should be in operational state : new state = %d chk=%d\n", slave_state, chk);

    if (ec_slave[0].state != EC_STATE_OPERATIONAL)
    {
        printf("ec: ethercat_loop: Not all slaves reached operational state.\n");
        ec_readstate();

        for (i = 1; i <= ec_slavecount; i++)
        {
            if (ec_slave[i].state != EC_STATE_OPERATIONAL)
                printf("ec: ethercat_loop: Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n", i, ec_slave[i].state, ec_slave[i].ALstatuscode, ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
        }

        printf("ec: ethercat_loop: Unable to get all ethercat devices in operation mode - giving up :(\n");
        return;
    }

    inOP = TRUE;

    ethercat_loop_counter = 0;
    static int step = 0;

    set_profile_acceleration(CiA402Setpoints, PROFILE_ACCELERATION_VALUE);
    set_profile_deceleration(CiA402Setpoints, PROFILE_DECELERATION_VALUE);
   set_profile_jerk(CiA402Setpoints, PROFILE_JERK_VALUE);
 

    printf("Start ethercat loop\n");



 uint64_t start_time_ms = get_current_time_ms();
    uint64_t current_time_ms;
    static int loop_counter = 0;
    bool time_limit_reached = false;

    /* Ethercat cyclic loop */
    while (1)
    {

        ethercat_loop_counter++;

        if (ethercat_loop_counter % 200 == 10)
            //  printf("ethercat_loop_counter %d slaves %d\n", ethercat_loop_counter, ec_slavecount);

            ec_send_processdata();
        wkc = ec_receive_processdata(EC_TIMEOUTRET);
        /* If work counter is not as expected - report and wait to be the normal value*/
        if (wkc < expectedWKC)
        {
            if (ethercat_loop_counter % 200 == 10)
                printf("wkc not increasing wkc=%d expectedWKC=%d\n", wkc, expectedWKC);
            osal_usleep(5000);

            // Wait until the cable is reconnected and wkc is back to normal
            while (wkc < expectedWKC)
            {
                ec_send_processdata();
                wkc = ec_receive_processdata(EC_TIMEOUTRET);
                osal_usleep(5000);

                ec_readstate();
                for (i = 1; i <= ec_slavecount; i++)
                {
                    if (ec_slave[i].state != EC_STATE_OPERATIONAL)
                    {
                        printf("Reconnecting Slave %d, current state=0x%2.2x\n", i, ec_slave[i].state);

                        // Ty to get the slave back to OPERATIONAL state
                        ec_slave[i].state = EC_STATE_PRE_OP;
                        ec_writestate(i); // writes the new state (PRE-OP) to the EtherCAT slave
                        osal_usleep(10000);

                        ec_slave[i].state = EC_STATE_SAFE_OP;
                        ec_writestate(i);
                        osal_usleep(10000);

                        ec_slave[i].state = EC_STATE_OPERATIONAL;
                        ec_writestate(i);
                    }
                }
                expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
            }

            // Once wkc is normal again, print a message and resume normal operation
            if (ethercat_loop_counter % 400 == 0)
                printf("Cable reconnected, wkc back to normal: %d\n", wkc);
        }

       


        current_time_ms = get_current_time_ms();

        if (current_time_ms - start_time_ms >= 60000) // 1 min
        {
                    time_limit_reached = true; 
        printf("Time limit of 15 seconds reached.\n");           
        }

        if (time_limit_reached)
        {
        printf("Number of loops completed in 15 seconds: %d\n", loop_counter);
        break;  
     }

        switch (step)
        {
        case 0:
            festo_motor_outputs_ptr->control_word = READY_To_SWITCH_ON; // 6
           printf("Step 0: READY_TO_SWITCH_ON.\n");
            step = 1;
            break;

        case 1:
            printf("Step 1: OPERATION_ENABLED.\n");
            festo_motor_outputs_ptr->control_word = ENABLE_OPERATION_CONTRIL_WORD; // 15
            festo_motor_outputs_ptr->modes_of_operation = ENABLE_OPERATION_MODE;   // 1
            step = 2;                                                             
            break;

        case 2:
            printf("Step 2: HOMING_MODE IS SET.\n");
            festo_motor_outputs_ptr->control_word = ENABLE_OPERATION_CONTRIL_WORD; // 1
            festo_motor_outputs_ptr->modes_of_operation = HOMING_OPERATION_MODE;   // 6
            step = 3;
            break;

        case 3:
         if(ethercat_loop_counter % 300 == 0)   printf("Step 3: HOMING_IS_STARTED.\n");
            festo_motor_outputs_ptr->control_word = HOMING_CONTROL_WORD; // 31
            step = 4;
            break;

        case 4:
            if (festo_motor_inputs_ptr->status_word == HOMING_STATUS_WORD)
            {
                printf("Step 4: HOMING COMPLETED. CONFIGURING TARGET_1.\n");
                festo_motor_outputs_ptr->control_word = ENABLE_OPERATION_CONTRIL_WORD; // 15
                festo_motor_outputs_ptr->modes_of_operation = ENABLE_OPERATION_MODE;   // 1
                step = 5;
            }
            else
            {
               if (ethercat_loop_counter % 400 == 0) printf("Step 4: DEVICE NOT REFERENCED. RETRYING HOMING.\n");
                step = 3;
            }
            break;

        case 5:

           // Timeout check 
                current_time_ms = get_current_time_ms();
                if (current_time_ms - start_time_ms >= 60000) // 30 sec timeout
                {
                    time_limit_reached = true;
                    printf("Time limit of 1 min reached in Step 5.\n");
                }

          if (time_limit_reached) break; 

          //  printf("Step 5: CONFIGURING TARGET_1.\n");
            festo_motor_outputs_ptr->control_word = ENABLE_OPERATION_CONTRIL_WORD; // 15
            festo_motor_outputs_ptr->modes_of_operation = ENABLE_OPERATION_MODE;   // 1
            festo_motor_outputs_ptr->target_position = TARGET_1;
            festo_motor_outputs_ptr->profile_velocity = PROFILE_VELOCITY;
            festo_motor_outputs_ptr->target_velocity = TARGET_VELOCITY;
           
          
            step = 6;
            break;

        case 6:

         // Timeout check 
                current_time_ms = get_current_time_ms();
                if (current_time_ms - start_time_ms >= 60000) // 30 sec timeout
                {
                    time_limit_reached = true;
                    printf("Time limit of 30 seconds reached in Step 6.\n");
                }

          if (time_limit_reached) break; 

            //  printf("Step 6: MOVING TO TARGET_1. CURRENT POSITION: %d\n", festo_motor_inputs_ptr->position_actual_value);
            festo_motor_outputs_ptr->control_word = MOVING_CONTROL_WORD; // 63

            if (festo_motor_inputs_ptr->position_actual_value >= TARGET_1 - TARGET_MARGIN && festo_motor_inputs_ptr->position_actual_value <= TARGET_1 + TARGET_MARGIN)
            {
          //      printf("Step 6: TARGET_1 REACHED. CONFIGURING TARGET_2.\n");
                step = 7;
            }
            else
            {
                if (ethercat_loop_counter % 200 == 10)
                    printf("Step 6: TARGET_1 NOT YET REACHED. CURRENT POSITION: %d\n", festo_motor_inputs_ptr->position_actual_value);
            }
            break;

        case 7:

         // Timeout check 
                current_time_ms = get_current_time_ms();
                if (current_time_ms - start_time_ms >= 60000) // 30 sec timeout
                {
                    time_limit_reached = true;
                    printf("Time limit of 30 seconds reached in Step 9.\n");
                }

          if (time_limit_reached) break; 

         //   printf("Step 8: CONFIGURING TARGET_2.\n");
            festo_motor_outputs_ptr->control_word = ENABLE_OPERATION_CONTRIL_WORD; // 15
            festo_motor_outputs_ptr->modes_of_operation = ENABLE_OPERATION_MODE;   // 1
            festo_motor_outputs_ptr->target_position = TARGET_2;
            festo_motor_outputs_ptr->profile_velocity = PROFILE_VELOCITY;
            festo_motor_outputs_ptr->target_velocity = TARGET_VELOCITY;
          
           
            step = 8;
            break;

        case 8:

         // Timeout check 
                current_time_ms = get_current_time_ms();
                if (current_time_ms - start_time_ms >= 60000) // 30 sec timeout
                {
                    time_limit_reached = true;
                    printf("Time limit of 30 seconds reached in Step 5.\n");
                }

          if (time_limit_reached) break; 

            //  printf("Step 9: MOVING TO TARGET_2. CURRENT POSITION: %d\n", festo_motor_inputs_ptr->position_actual_value);
            festo_motor_outputs_ptr->control_word = MOVING_CONTROL_WORD; // 63

            if (festo_motor_inputs_ptr->position_actual_value >= TARGET_2 - TARGET_MARGIN && festo_motor_inputs_ptr->position_actual_value <= TARGET_2 + TARGET_MARGIN)
            {
            //    printf("Step 9: TARGET_2 REACHED. RESTARTING LOOP.\n");
                loop_counter++;
                step = 5;
            }
            else
            {
                if (ethercat_loop_counter % 200 == 10)
                    printf("Step 9: TARGET_2 NOT YET REACHED. CURRENT POSITION: %d\n", festo_motor_inputs_ptr->position_actual_value);
            }
            break;


         default:
            printf("INVALID STATE VALUE.\n");
            break;
        }

       
    }

    // osal_usleep(5000);
    //  }
    printf("EtherCAT communication closed after 30 seconds.\n");

    ec_close();
}

OSAL_THREAD_FUNC ecatcheck(void *ptr)
{
    int slave;
    (void)ptr;
    volatile int wkc;
    int expectedWKC;
    uint8 currentgroup = 0;
    boolean needlf;

#define EC_TIMEOUTMON 500

    while (1)
    {
        if (inOP && ((wkc < expectedWKC) || ec_group[currentgroup].docheckstate))
        {
            if (needlf)
            {
                needlf = FALSE;
                printf("\n");
            }
            /* one ore more slaves are not responding */
            ec_group[currentgroup].docheckstate = FALSE;
            ec_readstate();
            for (slave = 1; slave <= ec_slavecount; slave++)
            {
                if ((ec_slave[slave].group == currentgroup) && (ec_slave[slave].state != EC_STATE_OPERATIONAL))
                {
                    ec_group[currentgroup].docheckstate = TRUE;
                    if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR))
                    {
                        printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                        ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                        ec_writestate(slave);
                    }
                    else if (ec_slave[slave].state == EC_STATE_SAFE_OP)
                    {
                        printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                        ec_slave[slave].state = EC_STATE_OPERATIONAL;
                        ec_writestate(slave);
                    }
                    else if (ec_slave[slave].state > EC_STATE_NONE)
                    {
                        if (ec_reconfig_slave(slave, EC_TIMEOUTMON))
                        {
                            ec_slave[slave].islost = FALSE;
                            printf("MESSAGE : slave %d reconfigured\n", slave);
                        }
                    }
                    else if (!ec_slave[slave].islost)
                    {
                        /* re-check state */
                        ec_statecheck(slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                        if (ec_slave[slave].state == EC_STATE_NONE)
                        {
                            ec_slave[slave].islost = TRUE;
                            printf("ERROR : slave %d lost\n", slave);
                        }
                    }
                }
                if (ec_slave[slave].islost)
                {
                    if (ec_slave[slave].state == EC_STATE_NONE)
                    {
                        if (ec_recover_slave(slave, EC_TIMEOUTMON))
                        {
                            ec_slave[slave].islost = FALSE;
                            printf("MESSAGE : slave %d recovered\n", slave);
                        }
                    }
                    else
                    {
                        ec_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d found\n", slave);
                    }
                }
            }
            if (!ec_group[currentgroup].docheckstate)
                printf("OK : all slaves resumed OPERATIONAL.\n");
        }
        osal_usleep(10000);
    }
}

char *dtype2string(uint16 dtype, uint16 bitlen)
{
    static char str[32] = {0};

    switch (dtype)
    {
    case ECT_BOOLEAN:
        sprintf(str, "BOOLEAN");
        break;
    case ECT_INTEGER8:
        sprintf(str, "INTEGER8");
        break;
    case ECT_INTEGER16:
        sprintf(str, "INTEGER16");
        break;
    case ECT_INTEGER32:
        sprintf(str, "INTEGER32");
        break;
    case ECT_INTEGER24:
        sprintf(str, "INTEGER24");
        break;
    case ECT_INTEGER64:
        sprintf(str, "INTEGER64");
        break;
    case ECT_UNSIGNED8:
        sprintf(str, "UNSIGNED8");
        break;
    case ECT_UNSIGNED16:
        sprintf(str, "UNSIGNED16");
        break;
    case ECT_UNSIGNED32:
        sprintf(str, "UNSIGNED32");
        break;
    case ECT_UNSIGNED24:
        sprintf(str, "UNSIGNED24");
        break;
    case ECT_UNSIGNED64:
        sprintf(str, "UNSIGNED64");
        break;
    case ECT_REAL32:
        sprintf(str, "REAL32");
        break;
    case ECT_REAL64:
        sprintf(str, "REAL64");
        break;
    case ECT_BIT1:
        sprintf(str, "BIT1");
        break;
    case ECT_BIT2:
        sprintf(str, "BIT2");
        break;
    case ECT_BIT3:
        sprintf(str, "BIT3");
        break;
    case ECT_BIT4:
        sprintf(str, "BIT4");
        break;
    case ECT_BIT5:
        sprintf(str, "BIT5");
        break;
    case ECT_BIT6:
        sprintf(str, "BIT6");
        break;
    case ECT_BIT7:
        sprintf(str, "BIT7");
        break;
    case ECT_BIT8:
        sprintf(str, "BIT8");
        break;
    case ECT_VISIBLE_STRING:
        sprintf(str, "VISIBLE_STR(%d)", bitlen);
        break;
    case ECT_OCTET_STRING:
        sprintf(str, "OCTET_STR(%d)", bitlen);
        break;
    default:
        sprintf(str, "dt:0x%4.4X (%d)", dtype, bitlen);
    }
    return str;
}

char *otype2string(uint16 otype)
{
    static char str[32] = {0};

    switch (otype)
    {
    case OTYPE_VAR:
        sprintf(str, "VAR");
        break;
    case OTYPE_ARRAY:
        sprintf(str, "ARRAY");
        break;
    case OTYPE_RECORD:
        sprintf(str, "RECORD");
        break;
    default:
        sprintf(str, "ot:0x%4.4X", otype);
    }
    return str;
}

char *access2string(uint16 access)
{
    static char str[32] = {0};

    sprintf(str, "%s%s%s%s%s%s",
            ((access & ATYPE_Rpre) != 0 ? "R" : "_"),
            ((access & ATYPE_Wpre) != 0 ? "W" : "_"),
            ((access & ATYPE_Rsafe) != 0 ? "R" : "_"),
            ((access & ATYPE_Wsafe) != 0 ? "W" : "_"),
            ((access & ATYPE_Rop) != 0 ? "R" : "_"),
            ((access & ATYPE_Wop) != 0 ? "W" : "_"));
    return str;
}

char *SDO2string(uint16 slave, uint16 index, uint8 subidx, uint16 dtype)
{
    int l = sizeof(usdo) - 1, i;
    uint8 *u8;
    int8 *i8;
    uint16 *u16;
    int16 *i16;
    uint32 *u32;
    int32 *i32;
    uint64 *u64;
    int64 *i64;
    float *sr;
    double *dr;
    char es[32];

    memset(&usdo, 0, 128);
    ec_SDOread(slave, index, subidx, FALSE, &l, &usdo, EC_TIMEOUTRXM);
    if (EcatError)
    {
        return ec_elist2string();
    }
    else
    {
        switch (dtype)
        {
        case ECT_BOOLEAN:
            u8 = (uint8 *)&usdo[0];
            if (*u8)
                sprintf(hstr, "TRUE");
            else
                sprintf(hstr, "FALSE");
            break;
        case ECT_INTEGER8:
            i8 = (int8 *)&usdo[0];
            sprintf(hstr, "0x%2.2x %d", *i8, *i8);
            break;
        case ECT_INTEGER16:
            i16 = (int16 *)&usdo[0];
            sprintf(hstr, "0x%4.4x %d", *i16, *i16);
            break;
        case ECT_INTEGER32:
        case ECT_INTEGER24:
            i32 = (int32 *)&usdo[0];
            sprintf(hstr, "0x%8.8x %d", *i32, *i32);
            break;
        case ECT_INTEGER64:
            i64 = (int64 *)&usdo[0];
            sprintf(hstr, "0x%16.16" PRIx64 " %" PRId64, *i64, *i64);
            break;
        case ECT_UNSIGNED8:
            u8 = (uint8 *)&usdo[0];
            sprintf(hstr, "0x%2.2x %u", *u8, *u8);
            break;
        case ECT_UNSIGNED16:
            u16 = (uint16 *)&usdo[0];
            sprintf(hstr, "0x%4.4x %u", *u16, *u16);
            break;
        case ECT_UNSIGNED32:
        case ECT_UNSIGNED24:
            u32 = (uint32 *)&usdo[0];
            sprintf(hstr, "0x%8.8x %u", *u32, *u32);
            break;
        case ECT_UNSIGNED64:
            u64 = (uint64 *)&usdo[0];
            sprintf(hstr, "0x%16.16" PRIx64 " %" PRIu64, *u64, *u64);
            break;
        case ECT_REAL32:
            sr = (float *)&usdo[0];
            sprintf(hstr, "%f", *sr);
            break;
        case ECT_REAL64:
            dr = (double *)&usdo[0];
            sprintf(hstr, "%f", *dr);
            break;
        case ECT_BIT1:
        case ECT_BIT2:
        case ECT_BIT3:
        case ECT_BIT4:
        case ECT_BIT5:
        case ECT_BIT6:
        case ECT_BIT7:
        case ECT_BIT8:
            u8 = (uint8 *)&usdo[0];
            sprintf(hstr, "0x%x", *u8);
            break;
        case ECT_VISIBLE_STRING:
            strcpy(hstr, usdo);
            break;
        case ECT_OCTET_STRING:
            hstr[0] = 0x00;
            for (i = 0; i < l; i++)
            {
                sprintf(es, "0x%2.2x ", usdo[i]);
                strcat(hstr, es);
            }
            break;
        default:
            sprintf(hstr, "Unknown type");
        }
        return hstr;
    }
}

int si_PDOassign(uint16 slave, uint16 PDOassign, int mapoffset, int bitoffset)
{
    uint16 idxloop, nidx, subidxloop, rdat, idx, subidx;
    uint8 subcnt;
    int wkc, bsize = 0, rdl;
    int32 rdat2;
    uint8 bitlen, obj_subidx;
    uint16 obj_idx;
    int abs_offset, abs_bit;

    rdl = sizeof(rdat);
    rdat = 0;
    /* read PDO assign subindex 0 ( = number of PDO's) */
    wkc = ec_SDOread(slave, PDOassign, 0x00, FALSE, &rdl, &rdat, EC_TIMEOUTRXM);
    rdat = etohs(rdat);
    /* positive result from slave ? */
    if ((wkc > 0) && (rdat > 0))
    {
        /* number of available sub indexes */
        nidx = rdat;
        bsize = 0;
        /* read all PDO's */
        for (idxloop = 1; idxloop <= nidx; idxloop++)
        {
            rdl = sizeof(rdat);
            rdat = 0;
            /* read PDO assign */
            wkc = ec_SDOread(slave, PDOassign, (uint8)idxloop, FALSE, &rdl, &rdat, EC_TIMEOUTRXM);
            /* result is index of PDO */
            idx = etohs(rdat);
            if (idx > 0)
            {
                rdl = sizeof(subcnt);
                subcnt = 0;
                /* read number of subindexes of PDO */
                wkc = ec_SDOread(slave, idx, 0x00, FALSE, &rdl, &subcnt, EC_TIMEOUTRXM);
                subidx = subcnt;
                /* for each subindex */
                for (subidxloop = 1; subidxloop <= subidx; subidxloop++)
                {
                    rdl = sizeof(rdat2);
                    rdat2 = 0;
                    /* read SDO that is mapped in PDO */
                    wkc = ec_SDOread(slave, idx, (uint8)subidxloop, FALSE, &rdl, &rdat2, EC_TIMEOUTRXM);
                    rdat2 = etohl(rdat2);
                    /* extract bitlength of SDO */
                    bitlen = LO_BYTE(rdat2);
                    bsize += bitlen;
                    obj_idx = (uint16)(rdat2 >> 16);
                    obj_subidx = (uint8)((rdat2 >> 8) & 0x000000ff);
                    abs_offset = mapoffset + (bitoffset / 8);
                    abs_bit = bitoffset % 8;
                    ODlist.Slave = slave;
                    ODlist.Index[0] = obj_idx;
                    OElist.Entries = 0;
                    wkc = 0;
                    /* read object entry from dictionary if not a filler (0x0000:0x00) */
                    if (obj_idx || obj_subidx)
                        wkc = ec_readOEsingle(0, obj_subidx, &ODlist, &OElist);
                    printf("  [0x%4.4X.%1d] 0x%4.4X:0x%2.2X 0x%2.2X", abs_offset, abs_bit, obj_idx, obj_subidx, bitlen);
                    if ((wkc > 0) && OElist.Entries)
                    {
                        printf(" %-12s %s\n", dtype2string(OElist.DataType[obj_subidx], bitlen), OElist.Name[obj_subidx]);
                    }
                    else
                        printf("\n");
                    bitoffset += bitlen;
                };
            };
        };
    };
    /* return total found bitlength (PDO) */
    return bsize;
}

int si_map_sdo(int slave)
{
    int wkc, rdl;
    int retVal = 0;
    uint8 nSM, iSM, tSM;
    int Tsize, outputs_bo, inputs_bo;
    uint8 SMt_bug_add;

    printf("PDO mapping according to CoE :\n");
    SMt_bug_add = 0;
    outputs_bo = 0;
    inputs_bo = 0;
    rdl = sizeof(nSM);
    nSM = 0;
    /* read SyncManager Communication Type object count */
    wkc = ec_SDOread(slave, ECT_SDO_SMCOMMTYPE, 0x00, FALSE, &rdl, &nSM, EC_TIMEOUTRXM);
    /* positive result from slave ? */
    if ((wkc > 0) && (nSM > 2))
    {
        /* make nSM equal to number of defined SM */
        nSM--;
        /* limit to maximum number of SM defined, if true the slave can't be configured */
        if (nSM > EC_MAXSM)
            nSM = EC_MAXSM;
        /* iterate for every SM type defined */
        for (iSM = 2; iSM <= nSM; iSM++)
        {
            rdl = sizeof(tSM);
            tSM = 0;
            /* read SyncManager Communication Type */
            wkc = ec_SDOread(slave, ECT_SDO_SMCOMMTYPE, iSM + 1, FALSE, &rdl, &tSM, EC_TIMEOUTRXM);
            if (wkc > 0)
            {
                if ((iSM == 2) && (tSM == 2)) // SM2 has type 2 == mailbox out, this is a bug in the slave!
                {
                    SMt_bug_add = 1; // try to correct, this works if the types are 0 1 2 3 and should be 1 2 3 4
                    printf("Activated SM type workaround, possible incorrect mapping.\n");
                }
                if (tSM)
                    tSM += SMt_bug_add; // only add if SMt > 0

                if (tSM == 3) // outputs
                {
                    /* read the assign RXPDO */
                    printf("  SM%1d outputs\n     addr b   index: sub bitl data_type    name\n", iSM);
                    Tsize = si_PDOassign(slave, ECT_SDO_PDOASSIGN + iSM, (int)(ec_slave[slave].outputs - (uint8 *)&IOmap[0]), outputs_bo);
                    outputs_bo += Tsize;
                }
                if (tSM == 4) // inputs
                {
                    /* read the assign TXPDO */
                    printf("  SM%1d inputs\n     addr b   index: sub bitl data_type    name\n", iSM);
                    Tsize = si_PDOassign(slave, ECT_SDO_PDOASSIGN + iSM, (int)(ec_slave[slave].inputs - (uint8 *)&IOmap[0]), inputs_bo);
                    inputs_bo += Tsize;
                }
            }
        }
    }

    /* found some I/O bits ? */
    if ((outputs_bo > 0) || (inputs_bo > 0))
        retVal = 1;
    return retVal;
}

int si_siiPDO(uint16 slave, uint8 t, int mapoffset, int bitoffset)
{
    uint16 a, w, c, e, er;
    uint8 eectl;
    uint16 obj_idx;
    uint8 obj_subidx;
    uint8 obj_name;
    uint8 obj_datatype;
    uint8 bitlen;
    int totalsize;
    ec_eepromPDOt eepPDO;
    ec_eepromPDOt *PDO;
    int abs_offset, abs_bit;
    char str_name[EC_MAXNAME + 1];

    eectl = ec_slave[slave].eep_pdi;

    totalsize = 0;
    PDO = &eepPDO;
    PDO->nPDO = 0;
    PDO->Length = 0;
    PDO->Index[1] = 0;
    for (c = 0; c < EC_MAXSM; c++)
        PDO->SMbitsize[c] = 0;
    if (t > 1)
        t = 1;
    PDO->Startpos = ec_siifind(slave, ECT_SII_PDO + t);
    if (PDO->Startpos > 0)
    {
        a = PDO->Startpos;
        w = ec_siigetbyte(slave, a++);
        w += (ec_siigetbyte(slave, a++) << 8);
        PDO->Length = w;
        c = 1;
        /* traverse through all PDOs */
        do
        {
            PDO->nPDO++;
            PDO->Index[PDO->nPDO] = ec_siigetbyte(slave, a++);
            PDO->Index[PDO->nPDO] += (ec_siigetbyte(slave, a++) << 8);
            PDO->BitSize[PDO->nPDO] = 0;
            c++;
            /* number of entries in PDO */
            e = ec_siigetbyte(slave, a++);
            PDO->SyncM[PDO->nPDO] = ec_siigetbyte(slave, a++);
            a++;
            obj_name = ec_siigetbyte(slave, a++);
            a += 2;
            c += 2;
            if (PDO->SyncM[PDO->nPDO] < EC_MAXSM) /* active and in range SM? */
            {
                str_name[0] = 0;
                if (obj_name)
                    ec_siistring(str_name, slave, obj_name);
                if (t)
                    printf("  SM%1d RXPDO 0x%4.4X %s\n", PDO->SyncM[PDO->nPDO], PDO->Index[PDO->nPDO], str_name);
                else
                    printf("  SM%1d TXPDO 0x%4.4X %s\n", PDO->SyncM[PDO->nPDO], PDO->Index[PDO->nPDO], str_name);
                printf("     addr b   index: sub bitl data_type    name\n");
                /* read all entries defined in PDO */
                for (er = 1; er <= e; er++)
                {
                    c += 4;
                    obj_idx = ec_siigetbyte(slave, a++);
                    obj_idx += (ec_siigetbyte(slave, a++) << 8);
                    obj_subidx = ec_siigetbyte(slave, a++);
                    obj_name = ec_siigetbyte(slave, a++);
                    obj_datatype = ec_siigetbyte(slave, a++);
                    bitlen = ec_siigetbyte(slave, a++);
                    abs_offset = mapoffset + (bitoffset / 8);
                    abs_bit = bitoffset % 8;

                    PDO->BitSize[PDO->nPDO] += bitlen;
                    a += 2;

                    /* skip entry if filler (0x0000:0x00) */
                    if (obj_idx || obj_subidx)
                    {
                        str_name[0] = 0;
                        if (obj_name)
                            ec_siistring(str_name, slave, obj_name);

                        printf("  [0x%4.4X.%1d] 0x%4.4X:0x%2.2X 0x%2.2X", abs_offset, abs_bit, obj_idx, obj_subidx, bitlen);
                        printf(" %-12s %s\n", dtype2string(obj_datatype, bitlen), str_name);
                    }
                    bitoffset += bitlen;
                    totalsize += bitlen;
                }
                PDO->SMbitsize[PDO->SyncM[PDO->nPDO]] += PDO->BitSize[PDO->nPDO];
                c++;
            }
            else /* PDO deactivated because SM is 0xff or > EC_MAXSM */
            {
                c += 4 * e;
                a += 8 * e;
                c++;
            }
            if (PDO->nPDO >= (EC_MAXEEPDO - 1))
                c = PDO->Length; /* limit number of PDO entries in buffer */
        } while (c < PDO->Length);
    }
    if (eectl)
        ec_eeprom2pdi(slave); /* if eeprom control was previously pdi then restore */
    return totalsize;
}

int si_map_sii(int slave)
{
    int retVal = 0;
    int Tsize, outputs_bo, inputs_bo;

    printf("PDO mapping according to SII :\n");

    outputs_bo = 0;
    inputs_bo = 0;
    /* read the assign RXPDOs */
    Tsize = si_siiPDO(slave, 1, (int)(ec_slave[slave].outputs - (uint8 *)&IOmap), outputs_bo);
    outputs_bo += Tsize;
    /* read the assign TXPDOs */
    Tsize = si_siiPDO(slave, 0, (int)(ec_slave[slave].inputs - (uint8 *)&IOmap), inputs_bo);
    inputs_bo += Tsize;
    /* found some I/O bits ? */
    if ((outputs_bo > 0) || (inputs_bo > 0))
        retVal = 1;
    return retVal;
}

void si_sdo(int cnt)
{
    int i, j;

    ODlist.Entries = 0;
    memset(&ODlist, 0, sizeof(ODlist));
    if (ec_readODlist(cnt, &ODlist))
    {
        printf(" CoE Object Description found, %d entries.\n", ODlist.Entries);
        for (i = 0; i < ODlist.Entries; i++)
        {
            uint8_t max_sub;
            char name[128] = {0};

            ec_readODdescription(i, &ODlist);
            while (EcatError)
                printf(" - %s\n", ec_elist2string());
            snprintf(name, sizeof(name) - 1, "\"%s\"", ODlist.Name[i]);
            if (ODlist.ObjectCode[i] == OTYPE_VAR)
            {
                printf("0x%04x      %-40s      [%s]\n", ODlist.Index[i], name,
                       otype2string(ODlist.ObjectCode[i]));
            }
            else
            {
                printf("0x%04x      %-40s      [%s  maxsub(0x%02x / %d)]\n",
                       ODlist.Index[i], name, otype2string(ODlist.ObjectCode[i]),
                       ODlist.MaxSub[i], ODlist.MaxSub[i]);
            }
            memset(&OElist, 0, sizeof(OElist));
            ec_readOE(i, &ODlist, &OElist);
            while (EcatError)
                printf("- %s\n", ec_elist2string());

            if (ODlist.ObjectCode[i] != OTYPE_VAR)
            {
                int l = sizeof(max_sub);
                ec_SDOread(cnt, ODlist.Index[i], 0, FALSE, &l, &max_sub, EC_TIMEOUTRXM);
            }
            else
            {
                max_sub = ODlist.MaxSub[i];
            }

            for (j = 0; j < max_sub + 1; j++)
            {
                if ((OElist.DataType[j] > 0) && (OElist.BitLength[j] > 0))
                {
                    snprintf(name, sizeof(name) - 1, "\"%s\"", OElist.Name[j]);
                    printf("    0x%02x      %-40s      [%-16s %6s]      ", j, name,
                           dtype2string(OElist.DataType[j], OElist.BitLength[j]),
                           access2string(OElist.ObjAccess[j]));
                    if ((OElist.ObjAccess[j] & 0x0007))
                    {
                        printf("%s", SDO2string(cnt, ODlist.Index[i], j, OElist.DataType[j]));
                    }
                    printf("\n");
                }
            }
        }
    }
    else
    {
        while (EcatError)
            printf("%s", ec_elist2string());
    }
}

void slaveinfo(char *ifname)
{
    int cnt, i, j, nSM;
    uint16 ssigen;
    int expectedWKC;

    printf("Starting slaveinfo\n");

    /* initialise SOEM, bind socket to ifname */
    if (ec_init(ifname))
    {
        printf("ec_init on %s succeeded.\n", ifname);
        /* find and auto-config slaves */
        if (ec_config(FALSE, &IOmap) > 0)
        {
            ec_configdc();
            while (EcatError)
                printf("%s", ec_elist2string());
            printf("%d slaves found and configured.\n", ec_slavecount);
            expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
            printf("Calculated workcounter %d\n", expectedWKC);
            /* wait for all slaves to reach SAFE_OP state */
            ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 3);
            if (ec_slave[0].state != EC_STATE_SAFE_OP)
            {
                printf("Not all slaves reached safe operational state.\n");
                ec_readstate();
                for (i = 1; i <= ec_slavecount; i++)
                {
                    if (ec_slave[i].state != EC_STATE_SAFE_OP)
                    {
                        printf("Slave %d State=%2x StatusCode=%4x : %s\n",
                               i, ec_slave[i].state, ec_slave[i].ALstatuscode, ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
                    }
                }
            }

            ec_readstate();
            for (cnt = 1; cnt <= ec_slavecount; cnt++)
            {
                printf("\nSlave:%d\n Name:%s\n Output size: %dbits\n Input size: %dbits\n State: %d\n Delay: %d[ns]\n Has DC: %d\n",
                       cnt, ec_slave[cnt].name, ec_slave[cnt].Obits, ec_slave[cnt].Ibits,
                       ec_slave[cnt].state, ec_slave[cnt].pdelay, ec_slave[cnt].hasdc);
                if (ec_slave[cnt].hasdc)
                    printf(" DCParentport:%d\n", ec_slave[cnt].parentport);
                printf(" Activeports:%d.%d.%d.%d\n", (ec_slave[cnt].activeports & 0x01) > 0,
                       (ec_slave[cnt].activeports & 0x02) > 0,
                       (ec_slave[cnt].activeports & 0x04) > 0,
                       (ec_slave[cnt].activeports & 0x08) > 0);
                printf(" Configured address: %4.4x\n", ec_slave[cnt].configadr);
                printf(" Man: %8.8x ID: %8.8x Rev: %8.8x\n", (int)ec_slave[cnt].eep_man, (int)ec_slave[cnt].eep_id, (int)ec_slave[cnt].eep_rev);
                for (nSM = 0; nSM < EC_MAXSM; nSM++)
                {
                    if (ec_slave[cnt].SM[nSM].StartAddr > 0)
                        printf(" SM%1d A:%4.4x L:%4d F:%8.8x Type:%d\n", nSM, etohs(ec_slave[cnt].SM[nSM].StartAddr), etohs(ec_slave[cnt].SM[nSM].SMlength),
                               etohl(ec_slave[cnt].SM[nSM].SMflags), ec_slave[cnt].SMtype[nSM]);
                }
                for (j = 0; j < ec_slave[cnt].FMMUunused; j++)
                {
                    printf(" FMMU%1d Ls:%8.8x Ll:%4d Lsb:%d Leb:%d Ps:%4.4x Psb:%d Ty:%2.2x Act:%2.2x\n", j,
                           etohl(ec_slave[cnt].FMMU[j].LogStart), etohs(ec_slave[cnt].FMMU[j].LogLength), ec_slave[cnt].FMMU[j].LogStartbit,
                           ec_slave[cnt].FMMU[j].LogEndbit, etohs(ec_slave[cnt].FMMU[j].PhysStart), ec_slave[cnt].FMMU[j].PhysStartBit,
                           ec_slave[cnt].FMMU[j].FMMUtype, ec_slave[cnt].FMMU[j].FMMUactive);
                }
                printf(" FMMUfunc 0:%d 1:%d 2:%d 3:%d\n",
                       ec_slave[cnt].FMMU0func, ec_slave[cnt].FMMU1func, ec_slave[cnt].FMMU2func, ec_slave[cnt].FMMU3func);
                printf(" MBX length wr: %d rd: %d MBX protocols : %2.2x\n", ec_slave[cnt].mbx_l, ec_slave[cnt].mbx_rl, ec_slave[cnt].mbx_proto);
                ssigen = ec_siifind(cnt, ECT_SII_GENERAL);
                /* SII general section */
                if (ssigen)
                {
                    ec_slave[cnt].CoEdetails = ec_siigetbyte(cnt, ssigen + 0x07);
                    ec_slave[cnt].FoEdetails = ec_siigetbyte(cnt, ssigen + 0x08);
                    ec_slave[cnt].EoEdetails = ec_siigetbyte(cnt, ssigen + 0x09);
                    ec_slave[cnt].SoEdetails = ec_siigetbyte(cnt, ssigen + 0x0a);
                    if ((ec_siigetbyte(cnt, ssigen + 0x0d) & 0x02) > 0)
                    {
                        ec_slave[cnt].blockLRW = 1;
                        ec_slave[0].blockLRW++;
                    }
                    ec_slave[cnt].Ebuscurrent = ec_siigetbyte(cnt, ssigen + 0x0e);
                    ec_slave[cnt].Ebuscurrent += ec_siigetbyte(cnt, ssigen + 0x0f) << 8;
                    ec_slave[0].Ebuscurrent += ec_slave[cnt].Ebuscurrent;
                }
                printf(" CoE details: %2.2x FoE details: %2.2x EoE details: %2.2x SoE details: %2.2x\n",
                       ec_slave[cnt].CoEdetails, ec_slave[cnt].FoEdetails, ec_slave[cnt].EoEdetails, ec_slave[cnt].SoEdetails);
                printf(" Ebus current: %d[mA]\n only LRD/LWR:%d\n",
                       ec_slave[cnt].Ebuscurrent, ec_slave[cnt].blockLRW);
                if ((ec_slave[cnt].mbx_proto & ECT_MBXPROT_COE) && printSDO)
                    si_sdo(cnt);
                if (printMAP)
                {
                    if (ec_slave[cnt].mbx_proto & ECT_MBXPROT_COE)
                        si_map_sdo(cnt);
                    else
                        si_map_sii(cnt);
                }
            }
        }
        else
        {
            printf("No slaves found!\n");
        }
        printf("End slaveinfo, close socket\n");
        /* stop SOEM, close socket */
        // ec_close();
    }
    else
    {
        printf("No socket connection on %s\nExcecute as root\n", ifname);
    }
}
