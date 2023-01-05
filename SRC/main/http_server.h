typedef struct {
	char url[32];
	char parameter[128];
} URL_t;

extern QueueHandle_t xQueueHttp;
extern _BITsconfigECU_u config_ECU ;
extern _configEngine_t turbine_config ;