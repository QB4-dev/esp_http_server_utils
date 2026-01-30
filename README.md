# Component: ESP HTTP Server Utils

Utilities and helpers for integrating the ESP-IDF HTTP server with common
device features. This component provides handlers  for:
- Wi-Fi connection management
- FOTA(Firmware Over The Air) 
- SPIFFS files uploading.

**Features**
- Reusable HTTP handlers and helpers located in the component sources:
	- [esp_http_server_fota.c](esp_http_server_fota.c) — FOTA update handler
	- [esp_http_server_spiffs.c](esp_http_server_spiffs.c) — SPIFFS file serving
	- [esp_http_server_wifi.c](esp_http_server_wifi.c) — Wi‑Fi helper endpoints
	- [esp_http_server_misc.c](esp_http_server_misc.c) — miscellaneous endpoints
	- [esp_http_upload.c](esp_http_upload.c) — multi-part upload helpers
- Example app demonstrating handler registration in `examples/default`

## Quick start

- Add this component directory into your project's `components/` folder.
- Include the public headers from the component in your app code:

```c
#include "esp_http_server_fota.h"
#include "esp_http_server_spiffs.h"
#include "esp_http_server_wifi.h"
#include "esp_http_server_misc.h"
#include "esp_http_upload.h"
```

- See the example application at [examples/default](examples/default) for a
	minimal integration and handler registration.

## Basic usage

- Register handlers with the ESP HTTP Server as you would any other handler.
	The component provides ready-to-register handler functions and helpers in
	the headers above. For concrete usage, inspect the example in
	[examples/default/main](examples/default/main).

- Handlers are implemented in the corresponding source files; you can
	customize or wrap them before registering with `httpd_register_uri_handler()`.

## Configuration

- This component follows standard ESP-IDF component practices. Any
	configurable options (if present) are exposed via `Kconfig` and can be
	enabled in your project's `sdkconfig`.

Files and headers
- Public headers are available in the `include/` directory:
	- [include/esp_http_server_fota.h](include/esp_http_server_fota.h)
	- [include/esp_http_server_spiffs.h](include/esp_http_server_spiffs.h)
	- [include/esp_http_server_wifi.h](include/esp_http_server_wifi.h)

## Installation

### Using ESP Component Registry

[![Component Registry](https://components.espressif.com/components/qb4-dev/http_server_utils/badge.svg)](https://components.espressif.com/components/qb4-dev/http_server_utils)

```bash
idf.py add-dependency "qb4-dev/http_server_utils=*"
```

### Manual Installation

Clone this component into your project's `components` directory:

```bash
cd your_project/components
git clone <this-repo-url> esp_http_server_utils
```

After adding the component, build your project with `idf.py build`.

License
- See the `LICENSE` file in this component for licensing details.


