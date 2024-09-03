#include <stdint.h>

typedef struct __attribute__((__packed__))
{
    uint16_t control_word;
    int8_t modes_of_operation;
    int32_t target_position;
    uint32_t profile_velocity;
    int32_t target_velocity;
    int16_t target_torque;
    int32_t velocity_offset;
    int16_t torque_offset;
} festo_motor_outputs;

typedef struct __attribute__((__packed__))
{
    uint16_t status_word;
    int8_t modes_of_operation_dispaly;
    int32_t position_actual_value;
    int32_t velocity_actual_value;
    int16_t torque_actual_value;
} festo_motor_inputs;