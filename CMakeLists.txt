idf_component_register(
	REQUIRES "esp_wifi http_parser esp_http_server app_update cjson spiffs "
	SRC_DIRS "."
	INCLUDE_DIRS "." "include"
)
