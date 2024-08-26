#include "include/main.h"
// #include "ethercat.h"
#include <string.h>


char ifbuf[1024];
OSAL_THREAD_HANDLE thread1;

static enum MHD_Result handle_get_request(struct MHD_Connection *connection, const char *url)
{
    const char *file_path;
    struct MHD_Response *response;
    FILE *file;
    char *file_content;
    long file_size;
    int ret;

    if (strcmp(url, "/") == 0)
    {
        file_path = "src/index.html";
    }
    else if (strcmp(url, "/pdo.txt") == 0)
    {
        file_path = "pdo.txt";
    }
    else if (strcmp(url, "/sdo.txt") == 0)
    {
        file_path = "sdo.txt";
    }
    else
    {
        file_path = NULL;
    }

    if (file_path)
    {
        file = fopen(file_path, "r");
        if (file)
        {
            fseek(file, 0, SEEK_END);
            file_size = ftell(file);
            fseek(file, 0, SEEK_SET);
            file_content = malloc(file_size + 1);
            if (file_content)
            {
                fread(file_content, 1, file_size, file);
                file_content[file_size] = '\0'; // Null-terminate the string
                fclose(file);

                response = MHD_create_response_from_buffer(file_size, file_content, MHD_RESPMEM_MUST_FREE);

                if (strcmp(url, "/index.html") == 0)
                {
                    MHD_add_response_header(response, "Content-Type", "text/html");
                }
                else if (strstr(url, ".txt"))
                {
                    MHD_add_response_header(response, "Content-Type", "text/plain");
                }

                ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
                MHD_destroy_response(response);
                return ret;
            }
        }
    }

    const char *error_message = "Invalid request";
    response = MHD_create_response_from_buffer(strlen(error_message), (void *)error_message, MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);
    return ret;
}


static enum MHD_Result handle_post_request(struct MHD_Connection *connection, const char *upload_data, size_t *upload_data_size)
{
    static char *post_data = NULL;
    static size_t post_data_size = 0;
    struct MHD_Response *response;
    int ret;

    if (*upload_data_size)
    {
        // Allocate or reallocate memory for post_data
        post_data = realloc(post_data, post_data_size + *upload_data_size);
        if (post_data == NULL)
        {
            return MHD_NO;
        }
        memcpy(post_data + post_data_size, upload_data, *upload_data_size);
        post_data_size += *upload_data_size;
        *upload_data_size = 0;
        return MHD_YES;
    }
    else
    {
        // Process POST data
        if (post_data)
        {
            parse_post_data(post_data);

            free(post_data);
            post_data = NULL;
            post_data_size = 0;

            const char *success_message = "Data updated successfully";
            response = MHD_create_response_from_buffer(strlen(success_message), (void *)success_message, MHD_RESPMEM_PERSISTENT);
            ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
            MHD_destroy_response(response);

            return ret;
        }
    }

    return MHD_YES;
}

char *trim_whitespace(char *str)
{
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str))
        str++;

    // All spaces?
    if (*str == 0)
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;

    // Null-terminate
    *(end + 1) = 0;

    return str;
}


int parse_value_or_default(const char *value, int default_value)
{
    if (value && *value != '\0')
    {
        return atoi(value);
    }
    return default_value;
}


//update pdo value in c code
void parse_key_value(const char *key, const char *value) {
    if (key && value) {
        // Trim whitespace
        char trimmed_key[256];
        char trimmed_value[256];
        strncpy(trimmed_key, key, sizeof(trimmed_key));
        strncpy(trimmed_value, value, sizeof(trimmed_value));
        trim_whitespace(trimmed_key);
        trim_whitespace(trimmed_value);

        // Map keys to structure fields
       if (strcmp(trimmed_key, "OUTPUT_CMD_0x7000") == 0)
                io_116e_outputs_ptr->output_cmd = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "OUT_PRESCALE_0_0x7001") == 0)
                io_116e_outputs_ptr->out_prescale_0 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "OUT_PRESCALE_1_0x7001") == 0)
                io_116e_outputs_ptr->out_prescale_1 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "OUT_PRESCALE_2_0x7001") == 0)
                io_116e_outputs_ptr->out_prescale_2 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "OUT_PRESCALE_3_0x7001") == 0)
                io_116e_outputs_ptr->out_prescale_3 = parse_value_or_default(trimmed_value, 0);

            else if (strcmp(trimmed_key, "OUT_UPDATED_0x7001") == 0)
                io_116e_outputs_ptr->out_update = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "IN_PRESCALE_0_0x7001") == 0)
                io_116e_outputs_ptr->in_prescale_0 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "IN_PRESCALE_1_0x7001") == 0)
                io_116e_outputs_ptr->in_prescale_1 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "IN_PRESCALE_2_0x7001") == 0)
                io_116e_outputs_ptr->in_prescale_2 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "IN_PRESCALE_3_0x7001") == 0)
                io_116e_outputs_ptr->in_prescale_3 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "IN_PRESCALE_4_0x7001") == 0)
                io_116e_outputs_ptr->in_prescale_4 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "IN_PRESCALE_5_0x7001") == 0)
                io_116e_outputs_ptr->in_prescale_5 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "IN_PRESCALE_6_0x7001") == 0)
                io_116e_outputs_ptr->in_prescale_6 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "IN_PRESCALE_7_0x7001") == 0)
                io_116e_outputs_ptr->in_prescale_7 = parse_value_or_default(trimmed_value, 0);

            else if (strcmp(trimmed_key, "IN_UPDATED_0x7001") == 0)
                io_116e_outputs_ptr->in_update = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "IN_FILTER_0x7001") == 0)
                io_116e_outputs_ptr->in_filter = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "IN_FILTER_ENABLED_0x7001") == 0)
                io_116e_outputs_ptr->in_filter_enabled = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "DUTY_CYCLE_0_0x7002") == 0)
                io_116e_outputs_ptr->duty_cycle_0 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "DUTY_CYCLE_1_0x7002") == 0)
                io_116e_outputs_ptr->duty_cycle_1 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "DUTY_CYCLE_2_0x7002") == 0)
                io_116e_outputs_ptr->duty_cycle_2 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "DUTY_CYCLE_3_0x7002") == 0)
                io_116e_outputs_ptr->duty_cycle_3 = parse_value_or_default(trimmed_value, 0);

            else if (strcmp(trimmed_key, "OFFSET_0_0x7002") == 0)
                io_116e_outputs_ptr->offset_0 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "OFFSET_1_0x7002") == 0)
                io_116e_outputs_ptr->offset_1 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_0_0x7002") == 0)
                io_116e_outputs_ptr->pulse_count_0 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_1_0x7002") == 0)
                io_116e_outputs_ptr->pulse_count_1 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_2_0x7002") == 0)
                io_116e_outputs_ptr->pulse_count_2 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_3_0x7002") == 0)
                io_116e_outputs_ptr->pulse_count_3 = parse_value_or_default(trimmed_value, 0);

            else if (strcmp(trimmed_key, "UPDATED_0x7002") == 0)
                io_116e_outputs_ptr->pulse_updated = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "ENABLED_0x7003") == 0)
                io_116e_outputs_ptr->enable = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_0_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_0 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_1_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_1 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_2_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_2 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_3_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_3 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_4_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_4 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_5_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_5 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_6_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_6 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_7_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_7 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_8_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_8 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_9_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_9 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_10_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_10 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_11_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_11 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_12_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_12 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_13_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_13 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_14_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_14 = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COUNT_15_0x7003") == 0)
                io_116e_outputs_ptr->trigger_count_15 = parse_value_or_default(trimmed_value, 0);

            else if (strcmp(trimmed_key, "UPDATED_0x7003") == 0)
                io_116e_outputs_ptr->trigger_updated = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COMMAND_0x7004") == 0)
                io_116e_outputs_ptr->in_count_command = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "COMMAND_0x7005") == 0)
                io_116e_outputs_ptr->in_count_latch_command = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "UPDATED_0x7005") == 0)
                io_116e_outputs_ptr->in_count_latch_updated = parse_value_or_default(trimmed_value, 0);
            else if (strcmp(trimmed_key, "LEDS_0x7006") == 0)
                io_116e_outputs_ptr->leds = parse_value_or_default(trimmed_value, 0);
        
        else
        
            fprintf(stderr, "Unknown key: %s\n", trimmed_key);
        } 
     
}


static void parse_post_data(const char *post_data)
{

    printf("POST Data: %s\n", post_data);

    // Create a mutable copy of post_data
    char *data = strdup(post_data);
    if (data == NULL)
    {
        perror("strdup");
        return;
    }

    // Zero-initialize the structure using memset
    memset(io_116e_outputs_ptr, 0, sizeof(*io_116e_outputs_ptr));


    // Parse key-value pairs
    char *pair = data;
    while (pair != NULL) {
        // Find the position of the '=' character
        char *equals = strchr(pair, '=');
        if (equals == NULL) break;

        // Extract the key
        size_t key_len = equals - pair;
        char key[key_len + 1];
        strncpy(key, pair, key_len);
        key[key_len] = '\0';

        // Extract the value
        char *next_pair = strchr(equals + 1, '&');
        size_t value_len = (next_pair != NULL) ? (next_pair - equals - 1) : strlen(equals + 1);
        char value[value_len + 1];
        strncpy(value, equals + 1, value_len);
        value[value_len] = '\0';

        /* Print debug information
          printf("Raw key-value pair: %s\n", pair);
        printf("key:  %s\n", key);
        printf("value:  %s\n", value);  */

      
        parse_key_value(key, value);

       

         // Move to the next key-value pair
        pair = next_pair ? next_pair + 1 : NULL;
    }

    // Free the mutable buffer
    free(data);

   /*     // Print the final values for debugging
    printf("The new value is:\n");
    printf("output_cmd   %d\n", io_116e_outputs_ptr->output_cmd);
    printf("prescale0   %d\n", io_116e_outputs_ptr->out_prescale_0);
    printf("prescale1   %d\n", io_116e_outputs_ptr->out_prescale_1);
    printf("prescale2   %d\n", io_116e_outputs_ptr->out_prescale_2);
    printf("prescale3   %d\n", io_116e_outputs_ptr->out_prescale_3);
    printf("outupdate    %d\n", io_116e_outputs_ptr->out_update);
    printf("leds:        %d\n", io_116e_outputs_ptr->leds);
 */
    
}


static enum MHD_Result answer_to_connection(void *cls,
                                            struct MHD_Connection *connection,
                                            const char *url,
                                            const char *method,
                                            const char *version,
                                            const char *upload_data,
                                            size_t *upload_data_size,
                                            void **con_cls)
{
    if (strcmp(method, "GET") == 0)
    {
        return handle_get_request(connection, url);
    }
    else if (strcmp(method, "POST") == 0)
    {
        return handle_post_request(connection, upload_data, upload_data_size);
    }

    return MHD_YES;
}



int main(void)
{
    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon(MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD,
                              PORT, NULL, NULL,
                              &answer_to_connection, NULL, MHD_OPTION_END);

    if (TRUE)
    {
        printf("SOEM (Simple Open EtherCAT Master)\nSimple test\n");
        osal_thread_create(&thread1, 128000, &ecatcheck, NULL);
        initialize_ethercat("enp0s31f6");

        const char *output_file_name = "output.txt";
        FILE *output_file = fopen(output_file_name, "w");
        if (output_file == NULL)
        {
            perror("Failed to open file");
            return 1;
        }

        // Redirect stdout to the output.txt file
        int saved_stdout = dup(fileno(stdout));
        if (dup2(fileno(output_file), fileno(stdout)) == -1)
        {
            perror("Failed to redirect stdout");
            return 1;
        }

        printf("SOEM (Simple Open EtherCAT Master)\nSlaveinfo\n");
        slaveinfo("enp0s31f6");

        // Restore the original stdout
        fflush(stdout);
        dup2(saved_stdout, fileno(stdout));
        close(saved_stdout);

        fclose(output_file);

        save_sdo_pdo_to_file(output_file_name);

        // ethercat_loop();

        io_116e_outputs_ptr = (io_116e_outputs *)ec_slave[1].outputs;
    }
    else
    {
        ec_adaptert *adapter = NULL;
        printf("Usage: simple_test ifname1\nifname = eth0 for example\n");

        printf("\nAvailable adapters:\n");
        adapter = ec_find_adapters();
        while (adapter != NULL)
        {
            printf("    - %s  (%s)\n", adapter->name, adapter->desc);
            adapter = adapter->next;
        }
        ec_free_adapters(adapter);
    }

    if (NULL == daemon)
        return 1;

    (void)getchar();

    MHD_stop_daemon(daemon);
    return 0;
}