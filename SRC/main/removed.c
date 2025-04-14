/*	static esp_err_t configmoteur(httpd_req_t *req)

httpd_resp_sendstr_chunk(req, "<b>Nom du moteur</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"name\" placeholder=\"\" value=\"");
	httpd_resp_sendstr_chunk(req, turbine_config.name) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"name\" minlength=\"1\" maxlength=\"20\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>Puissance de la bougie 0-255</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"glow_power\" placeholder=\"\" value=\"");
	itoa(turbine_config.glow_power,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"glow_power\" type=\"number\" min=\"0\" max=\"255\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>RPM plein gaz</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"jet_full_power_rpm\" placeholder=\"\" value=\"");
	itoa(turbine_config.jet_full_power_rpm,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"jet_full_power_rpm\" type=\"number\" min=\"0\" max=\"300000\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>RPM ralenti</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"jet_idle_rpm\" placeholder=\"\" value=\"");
	itoa(turbine_config.jet_idle_rpm,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"\"  type=\"number\" min=\"0\" max=\"300000\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>RPM mini</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"jet_min_rpm\" placeholder=\"\" value=\"");
	itoa(turbine_config.jet_min_rpm,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"jet_min_rpm\"  type=\"number\" min=\"0\" max=\"100000\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>Température de démarrage en °C</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"start_temp\" placeholder=\"\" value=\"");
	itoa(turbine_config.start_temp,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"start_temp\"  type=\"number\" min=\"0\" max=\"500\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>Température max en °C</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"max_temp\" placeholder=\"\" value=\"");
	itoa(turbine_config.max_temp,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"max_temp\"  type=\"number\" min=\"0\" max=\"1000\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>Délai d'accélération (0-100)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"acceleration_delay\" placeholder=\"\" value=\"");
	itoa(turbine_config.acceleration_delay,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"acceleration_delay\"  type=\"number\" min=\"0\" max=\"30\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>Délai de décélération (0-100)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"deceleration_delay\" placeholder=\"\" value=\"");
	itoa(turbine_config.deceleration_delay,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"deceleration_delay\"  type=\"number\" min=\"0\" max=\"30\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>Délai de stabilité (0-100)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"stability_delay\" placeholder=\"\" value=\"");
	itoa(turbine_config.stability_delay,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"stability_delay\"  type=\"number\" min=\"0\" max=\"30\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>PWM Max pompe 1 (0-1024)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"max_pump1\" placeholder=\"\" value=\"");
	itoa(turbine_config.max_pump1,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"max_pump1\"  type=\"number\" min=\"0\" max=\"1024\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>PWM Min pompe 1 (0-1024)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"min_pump1\" placeholder=\"\" value=\"");
	itoa(turbine_config.min_pump1,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"min_pump1\" type=\"number\" min=\"0\" max=\"1024\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>PWM Max pompe 2 (0-1024)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"max_pump2\" placeholder=\"\" value=\"");
	itoa(turbine_config.max_pump2,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"max_pump2\" type=\"number\" min=\"0\" max=\"1024\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>PWM Min pompe 2 (0-1024)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"min_pump2\" placeholder=\"\" value=\"");
	itoa(turbine_config.min_pump2,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"min_pump2\" type=\"number\" min=\"0\" max=\"1024\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>PWM Max vanne 1 (0-1024)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"max_vanne1\" placeholder=\"\" value=\"");
	itoa(turbine_config.max_vanne1,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"max_vanne1\" type=\"number\" min=\"0\" max=\"1024\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>PWM Max vanne 2 (0-1024)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"max_vanne2\" placeholder=\"\" value=\"");
	itoa(turbine_config.max_vanne2,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"max_vanne2\" type=\"number\" min=\"0\" max=\"1024\"></p><p>");

	httpd_resp_sendstr_chunk(req, "<b>RPM Allumage (0-5000RPM)</b><br>");
	httpd_resp_sendstr_chunk(req, "<input id=\"starter_rpm_start\" placeholder=\"\" value=\"");
	itoa(turbine_config.starter_rpm_start,tmp,10) ;
	httpd_resp_sendstr_chunk(req, tmp) ;
	httpd_resp_sendstr_chunk(req, "\" name=\"starter_rpm_start\" type=\"number\" min=\"0\" max=\"5000\"></p><p>");	*/

/*static esp_err_t frontpage(httpd_req_t *req)
	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"configecu\">");
	httpd_resp_sendstr_chunk(req, "<button>Paramètres ECU</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");
	
	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"configmoteur\">");
	httpd_resp_sendstr_chunk(req, "<button>Paramètres moteur</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");
	
	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"info\">");
	httpd_resp_sendstr_chunk(req, "<button>Information</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");
	
	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"logs\">");
	httpd_resp_sendstr_chunk(req, "<button>Logs</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"wifi\">");
	httpd_resp_sendstr_chunk(req, "<button>WiFi</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"slider\">");
	httpd_resp_sendstr_chunk(req, "<button>Slider</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"gauges\">");
	httpd_resp_sendstr_chunk(req, "<button>Jauges</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"start\">");
	httpd_resp_sendstr_chunk(req, "<button>Start engine</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"stop\">");
	httpd_resp_sendstr_chunk(req, "<button>Stop engine</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"upgrade\">");
	httpd_resp_sendstr_chunk(req, "<button class=\"button bred\">Mise à jour</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");

	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"stopwifi\">");
	httpd_resp_sendstr_chunk(req, "<button class=\"button bred\">Couper le WiFi</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");
*/

/*static esp_err_t configecu(httpd_req_t *req)

httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Voie des gaz</b></legend>") ;

		httpd_resp_sendstr_chunk(req, "<p><input id=\"input_ppm\" name=\"input\" type=\"radio\" value=\"0\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.input_type == PPM) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Standard</b>") ;
		
		httpd_resp_sendstr_chunk(req, "<p><input id=\"input_sbus\" name=\"input\" type=\"radio\" value=\"1\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.input_type == SBUS) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>SBUS</b>") ;*/
	
	//httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;
	/*Type de bougie*/
	

	/*httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Type de bougie</b></legend>") ;
		httpd_resp_sendstr_chunk(req, "<p><input id=\"glow_type_gas\" name=\"glow_type\" type=\"radio\" value=\"0\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.glow_type == GAS) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Gaz</b>") ;
		
		httpd_resp_sendstr_chunk(req, "<p><input id=\"glow_type_kero\" name=\"glow_type\" type=\"radio\" value=\"1\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.glow_type == KERO) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Kérostart</b>") ;
	httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;*/
	/*Type de démarrage*/
	/*httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Démarrage</b></legend>") ;
		httpd_resp_sendstr_chunk(req, "<p><input id=\"start_type_manual\" name=\"start_type\" type=\"radio\" value=\"0\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.start_type == MANUAL ) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Manuel</b>") ;
	
		httpd_resp_sendstr_chunk(req, "<p><input id=\"start_type_auto\" name=\"start_type\" type=\"radio\" value=\"1\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.start_type == AUTO ) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Auto</b>") ;
	httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;*/
	/*Type de pompe*/
	/*httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Pompe 1</b></legend>") ;
		httpd_resp_sendstr_chunk(req, "<p><input id=\"output_pump1_pwm\" name=\"output_pump1\" type=\"radio\" value=\"1\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.output_pump1 == PWM) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Moteur DC</b>") ;
		
		httpd_resp_sendstr_chunk(req, "<p><input id=\"output_pump1_ppm\" name=\"output_pump1\" type=\"radio\" value=\"0\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.output_pump1 == PPM) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Variateur</b>") ;
	httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;*/
	/*Type de démarreur*/
	/*httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Démarreur</b></legend>") ;
		httpd_resp_sendstr_chunk(req, "<p><input id=\"output_starter_pwm\" name=\"output_starter\" type=\"radio\" value=\"1\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.output_starter == PWM) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Moteur DC</b>") ;
		
		httpd_resp_sendstr_chunk(req, "<p><input id=\"output_starter_ppm\" name=\"output_starter\" type=\"radio\" value=\"0\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.output_starter == PPM) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Variateur</b>") ;
	httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;*/
	/*Type de télémétrie*/
	/*httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Télémétrie</b></legend>") ;
		httpd_resp_sendstr_chunk(req, "<p><input id=\"futaba_telem\" name=\"telem\" type=\"radio\" value=\"0\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.use_telem == FUTABA ) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Désactivée</b>") ;
	
		httpd_resp_sendstr_chunk(req, "<p><input id=\"use_frsky_telem\" name=\"telem\" type=\"radio\" value=\"1\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.use_telem == FRSKY ) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>FrSky</b>") ;

		httpd_resp_sendstr_chunk(req, "<p><input id=\"use_hott_telem\" name=\"telem\" type=\"radio\" value=\"3\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.use_telem == HOTT ) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>FrSky</b>") ;
		
		httpd_resp_sendstr_chunk(req, "<p><input id=\"no_telem\" name=\"telem\" type=\"radio\" value=\"2\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.use_telem == NONE ) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Futaba</b>") ;
	httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;*/
	/*Pompe 2*/
	/*httpd_resp_sendstr_chunk(req, "<fieldset><legend><b>&nbsp;Pompe 2</b></legend>") ;
		httpd_resp_sendstr_chunk(req, "<p><input id=\"no_pump2\" name=\"output_pump2\" type=\"radio\" value=\"0\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.output_pump2 == PPM) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Variateur</b>") ;

		httpd_resp_sendstr_chunk(req, "<p><input id=\"output_pump2_pwm\" name=\"output_pump2\" type=\"radio\" value=\"1\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.output_pump2 == PWM) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Moteur DC</b>") ;
		
		httpd_resp_sendstr_chunk(req, "<p><input id=\"output_pump2_ppm\" name=\"output_pump2\" type=\"radio\" value=\"2\"") ;
		httpd_resp_sendstr_chunk(req, (config_ECU.output_pump2 == NONE) ? "checked=\"\"" :" " ) ;
		httpd_resp_sendstr_chunk(req, "><b>Désactivée</b>") ;
	httpd_resp_sendstr_chunk(req, "</fieldset><p>") ;*/
	/*Voie aux*/
	/*httpd_resp_sendstr_chunk(req, "<p><input id=\"use_input2\" type=\"checkbox\"") ;
	httpd_resp_sendstr_chunk(req, (config_ECU.use_input2 == YES) ? "checked=\"\"" :" " ) ;
	httpd_resp_sendstr_chunk(req, " name=\"use_input2\"><b>Voie 2 Activée</b>") ;*/
	/*Leds*/
	/*httpd_resp_sendstr_chunk(req, "<p><input id=\"use_led\" type=\"checkbox\" " ) ;
	httpd_resp_sendstr_chunk(req, (config_ECU.use_led == YES) ? "checked=\"\"" :" " ) ;
	httpd_resp_sendstr_chunk(req, " name=\"use_led\"><b>Leds Activées</b>") ;*/
/*
	static esp_err_t logs(httpd_req_t *req)
{
	ESP_LOGI(TAG, "root_get_handler req->uri=[%s]", req->uri);

	// Send HTML header
	send_head(req) ;
	httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html>");
	Text2Html(req, "/html/head.html");

	httpd_resp_sendstr_chunk(req, "<h2>");
	httpd_resp_sendstr_chunk(req, turbine_config.name);
	httpd_resp_sendstr_chunk(req, "</h2>");
		
	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"c_logs.txt\">");
	httpd_resp_sendstr_chunk(req, "<button>Log 1</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");
	
	httpd_resp_sendstr_chunk(req, "<form method=\"GET\" action=\"c_curves.txt\">");
	httpd_resp_sendstr_chunk(req, "<button>Courbe de gaz</button></form>");
	httpd_resp_sendstr_chunk(req, "</form>");
	httpd_resp_sendstr_chunk(req, "<p></p>");
	
	httpd_resp_sendstr_chunk(req, "<p></p><form action=\"/\" method=\"get\"><button name="">Retour</button></form>") ;

	httpd_resp_sendstr_chunk(req, "<p></p>");

	Text2Html(req, "/html/footer.html");
	httpd_resp_sendstr_chunk(req, NULL); //fin de la page

	return ESP_OK;
}*/
/*
else if(strcmp(filename, "/c_curves.txt") == 0) 
curves_get_handler(req) ;
else if(strcmp(filename, "/c_logs.txt") == 0) 
logs_get_handler(req) ;
*/

/*static esp_err_t logs_get_handler(httpd_req_t *req)
{
    FILE *fd = NULL;
    struct stat st;
	char FileName[] = "/sdcard/logs/logs.txt" ;
    vTaskSuspend( xlogHandle );
	if (stat(FileName, &st) != 0) {
		ESP_LOGE(TAG, "[%s] not found", FileName);
		return ESP_FAIL;
	}
	ESP_LOGI(TAG, "%s st.st_size=%ld", FileName, st.st_size);

	char*	file_buffer = NULL;
	size_t file_buffer_len = st.st_size;
	if(file_buffer_len > 0)
	{
		file_buffer = malloc(file_buffer_len);
		if (file_buffer == NULL) {
			ESP_LOGE(TAG, "malloc fail. file_buffer_len %d", file_buffer_len);
			return ESP_FAIL;
		}

		ESP_LOGI(TAG, "logs_get_handler req->uri=[%s]", req->uri);
		fd = fopen(FileName, "r");
		if (!fd) {
		ESP_LOGE(TAG, "Failed to read existing file : logs.txt");
			// Respond with 500 Internal Server Error 
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
			return ESP_FAIL;
		}
		for (int i=0;i<st.st_size;i++) {
			fread(&file_buffer[i], sizeof(char), 1, fd);
		}
		fclose(fd);

		ESP_LOGI(TAG, "Sending file : logs.txt...");
		//ESP_LOGI(TAG, "%s",file_buffer);
		httpd_resp_set_type(req, "application/octet-stream");
		if (httpd_resp_send_chunk(req, file_buffer, st.st_size) != ESP_OK) {
					fclose(fd);
					ESP_LOGE(TAG, "File sending failed!");
					// Abort sending file 
					httpd_resp_sendstr_chunk(req, NULL);
					// Respond with 500 Internal Server Error 
					httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
				return ESP_FAIL;
		}
		httpd_resp_sendstr_chunk(req, NULL);
		ESP_LOGI(TAG, "File sending complete");
	}
    vTaskResume( xlogHandle );
	return ESP_OK;
}*/