idf_component_register(
	REQUIRES "esp_wifi http_parser esp_http_server app_update json spiffs "
	SRC_DIRS "."
	INCLUDE_DIRS "." "include"
)
