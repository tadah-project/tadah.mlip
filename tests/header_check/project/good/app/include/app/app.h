#pragma once
#ifndef PROBE_APP_DEFINITION
#error "PROBE_APP_DEFINITION (a PRIVATE definition of probe_app) is not defined"
#endif
#include <app_private.h>
#include <probe/uses_widget.h>
inline int app_value() { return app_private_value() + probe::widget_count(probe::Widget{}); }
