#include "include/main.h"
// #include "ethercat.h"
#include <string.h>

static enum MHD_Result answer_to_connection(void *cls, struct MHD_Connection *connection,
                                            const char *url, const char *method,
                                            const char *version, const char *upload_data,
                                            size_t *upload_data_size, void **con_cls)
{
    const char *page = "<html><body>Hello, browser!</body></html>";
    struct MHD_Response *response;
    enum MHD_Result ret;
    (void)cls;              /* Unused. Silent compiler warning. */
    (void)url;              /* Unused. Silent compiler warning. */
    (void)method;           /* Unused. Silent compiler warning. */
    (void)version;          /* Unused. Silent compiler warning. */
    (void)upload_data;      /* Unused. Silent compiler warning. */
    (void)upload_data_size; /* Unused. Silent compiler warning. */
    (void)con_cls;          /* Unused. Silent compiler warning. */

    response =
        MHD_create_response_from_buffer(strlen(page), (void *)page,
                                        MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

char ifbuf[1024];
OSAL_THREAD_HANDLE thread1;

int main(void)
{
    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon(MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD,
                              PORT, NULL, NULL,
                              &answer_to_connection, NULL, MHD_OPTION_END);

    if (1)
    {
        printf("SOEM (Simple Open EtherCAT Master)\nSimple test\n");
        osal_thread_create(&thread1, 128000, &ecatcheck, NULL);
        initialize_ethercat("enp0s31f6");

        printf("SOEM (Simple Open EtherCAT Master)\nSlaveinfo\n");
        slaveinfo("enp0s31f6");

        ethercat_loop();
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