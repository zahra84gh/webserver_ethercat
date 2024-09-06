#include <sys/types.h>
#ifndef _WIN32
#include <sys/select.h>
#include <sys/socket.h>
#else
#include <winsock2.h>
#endif
#include <string.h>
#include <microhttpd.h>
#include <stdio.h>
#include "slaveinfo.h"
#include <stdlib.h>
#include <stdbool.h>
#include "io_116e.h"
#include <ctype.h>
#include "mx2_inverter.h"
#include "festo_motor.h"

#include <json-c/json.h> 

#define PORT 8000

// io_116e_outputs* io_116e_outputs_ptr = NULL;

/* mx2_outputs *mx2_outputs_ptr = NULL;
mx2_inputs *mx2_inputs_ptr = NULL;
 */

extern festo_motor_outputs* festo_motor_outputs_ptr;
extern festo_motor_inputs* festo_motor_inputs_ptr;
 

static enum MHD_Result answer_to_connection(void *cls,
                                            struct MHD_Connection *connection,
                                            const char *url,
                                            const char *method,
                                            const char *version,
                                            const char *upload_data,
                                            size_t *upload_data_size,
                                            void **con_cls);

static enum MHD_Result handle_get_request(struct MHD_Connection *connection, const char *url);
static enum MHD_Result handle_post_request(struct MHD_Connection *connection, const char *upload_data, size_t *upload_data_size);
static void parse_post_data(const char *post_data);
char *trim_whitespace(char *str);
int parse_value_or_default(const char *value, int default_value);
void decode_plus_to_space(char *str);

//void parse_key_value_io_116e(const char *key, const char *value);
//void parse_key_value_mx2_inverter(const char *key, const char *value);
void parse_key_value_festo_motor(const char *key, const char *value);

void redirect_terminal_to_text_file(const char *output_file_name, void (*func)());
void print_slaveinfo();
void save_sdo_pdo_to_file(const char *output);  
float get_pdo_input_value_festo_motor(const char *index_sub);

