#pragma once
#include <zmk/event_manager.h>
struct ks_display_key_changed { unsigned sequence; };
ZMK_EVENT_DECLARE(ks_display_key_changed);
