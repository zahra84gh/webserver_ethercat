#include "include/main.h"
// #include "ethercat.h"
#include <string.h>
#include "include/io_116e.h"

static enum MHD_Result answer_to_connection(void *cls,
                                            struct MHD_Connection *connection,
                                            const char *url,
                                            const char *method,
                                            const char *version,
                                            const char *upload_data,
                                            size_t *upload_data_size,
                                            void **con_cls)
{
    const char *file_path;
    struct MHD_Response *response;
    FILE *file;
    char *file_content;
    long file_size;
    int ret;

    if (0 == strcmp(method, "GET"))
    {
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
                        MHD_add_response_header(response, "Content-Type", "text/html");   // set the response type
                    }
                    else if (strstr(url, ".txt"))
                    {
                        MHD_add_response_header(response, "Content-Type", "text/plain");
                    }

                    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);   //send response to web browser
                    MHD_destroy_response(response);
                }
            }

            return ret;
        }

        const char *error_message = "Invalid request";
        response = MHD_create_response_from_buffer(strlen(error_message), (void *)error_message, MHD_RESPMEM_PERSISTENT);
        ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
        MHD_destroy_response(response);
        return ret;
    }

    return MHD_YES;
}

char ifbuf[1024];
OSAL_THREAD_HANDLE thread1;

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