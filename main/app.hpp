#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "argtable3/argtable3.h"

typedef struct {
	struct arg_str *dir;
	struct arg_end *end;
} app_cmd_t;

void app_start();

#ifdef __cplusplus
};

#include <thread>
#include <esp_pthread.h>
#include "body_control.hpp"

class Application {
public:
	Application();
	void handleCmd(app_cmd_t *args);
private:
	BodyControl body;
};
#endif
