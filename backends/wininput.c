#define BACKEND_NAME "wininput"
#define DEBUG

#include <string.h>
#include "wininput.h"

//TODO check whether feedback elimination is required
//TODO refactor & simplify

static key_info keys[] = {
	{VK_LBUTTON, "lmb", button}, {VK_RBUTTON, "rmb", button}, {VK_MBUTTON, "mmb", button},
	{VK_XBUTTON1, "xmb1", button}, {VK_XBUTTON2, "xmb2", button},
	{VK_BACK, "backspace"},
	{VK_TAB, "tab"},
	{VK_CLEAR, "clear"},
	{VK_RETURN, "enter"},
	{VK_SHIFT, "shift"},
	{VK_CONTROL, "control"}, {VK_MENU, "alt"},
	{VK_CAPITAL, "capslock"},
	{VK_ESCAPE, "escape"},
	{VK_SPACE, "space"},
	{VK_PRIOR, "pageup"}, {VK_NEXT, "pagedown"},
	{VK_END, "end"}, {VK_HOME, "home"},
	{VK_PAUSE, "pause"}, {VK_NUMLOCK, "numlock"}, {VK_SCROLL, "scrolllock"},
	{VK_INSERT, "insert"}, {VK_DELETE, "delete"}, {VK_SNAPSHOT, "printscreen"},
	{VK_LEFT, "left"}, {VK_UP, "up"}, {VK_RIGHT, "right"}, {VK_DOWN, "down"},
	{VK_SELECT, "select"},
	{VK_PRINT, "print"},
	{VK_EXECUTE, "execute"},
	{VK_HELP, "help"},
	{VK_APPS, "apps"},
	{VK_SLEEP, "sleep"},
	{VK_NUMPAD0, "num0"}, {VK_NUMPAD1, "num1"}, {VK_NUMPAD2, "num2"}, {VK_NUMPAD3, "num3"},
	{VK_NUMPAD4, "num4"}, {VK_NUMPAD5, "num5"}, {VK_NUMPAD6, "num6"}, {VK_NUMPAD7, "num7"},
	{VK_NUMPAD8, "num8"}, {VK_NUMPAD9, "num9"}, {VK_MULTIPLY, "multiply"}, {VK_ADD, "plus"},
	{VK_SEPARATOR, "comma"}, {VK_SUBTRACT, "minus"}, {VK_DECIMAL, "dot"}, {VK_DIVIDE, "divide"},
	{VK_F1, "f1"}, {VK_F2, "f2"}, {VK_F3, "f3"}, {VK_F4, "f4"}, {VK_F5, "f5"},
	{VK_F6, "f6"}, {VK_F7, "f7"}, {VK_F8, "f8"}, {VK_F9, "f9"}, {VK_F10, "f10"},
	{VK_F11, "f11"}, {VK_F12, "f12"}, {VK_F13, "f13"}, {VK_F14, "f14"}, {VK_F15, "f15"},
	{VK_F16, "f16"}, {VK_F17, "f17"}, {VK_F18, "f18"}, {VK_F19, "f19"}, {VK_F20, "f20"},
	{VK_F21, "f21"}, {VK_F22, "f22"}, {VK_F23, "f23"}, {VK_F24, "f24"},
	{VK_LWIN, "lwin"}, {VK_RWIN, "rwin"},
	{VK_LSHIFT, "lshift"}, {VK_RSHIFT, "rshift"},
	{VK_LCONTROL, "lctrl"}, {VK_RCONTROL, "rctrl"},
	{VK_LMENU, "lmenu"}, {VK_RMENU, "rmenu"},
	{VK_BROWSER_BACK, "previous"}, {VK_BROWSER_FORWARD, "next"}, {VK_BROWSER_REFRESH, "refresh"},
	{VK_BROWSER_STOP, "stop"}, {VK_BROWSER_SEARCH, "search"}, {VK_BROWSER_FAVORITES, "favorites"},
	{VK_BROWSER_HOME, "homepage"},
	{VK_VOLUME_MUTE, "mute"}, {VK_VOLUME_DOWN, "voldown"}, {VK_VOLUME_UP, "volup"},
	{VK_MEDIA_NEXT_TRACK, "nexttrack"}, {VK_MEDIA_PREV_TRACK, "prevtrack"},
	{VK_MEDIA_STOP, "stopmedia"}, {VK_MEDIA_PLAY_PAUSE, "togglemedia"},
	{VK_LAUNCH_MEDIA_SELECT, "mediaselect"},
	{VK_LAUNCH_MAIL, "mail"}, {VK_LAUNCH_APP1, "app1"}, {VK_LAUNCH_APP2, "app2"},
	{VK_OEM_PLUS, "plus"}, {VK_OEM_COMMA, "comma"},
	{VK_OEM_MINUS, "minus"}, {VK_OEM_PERIOD, "period"},
	{VK_ZOOM, "zoom"}
};

static struct {
	int virtual_x, virtual_y, virtual_width, virtual_height;
	uint16_t mouse_x, mouse_y;
	size_t requests;
	wininput_request* request;
	uint32_t interval;
} cfg = {
	.requests = 0,
	.interval = 50
};

MM_PLUGIN_API int init(){
	backend wininput = {
		.name = BACKEND_NAME,
		.conf = wininput_configure,
		.create = wininput_instance,
		.interval = wininput_interval,
		.conf_instance = wininput_configure_instance,
		.channel = wininput_channel,
		.handle = wininput_set,
		.process = wininput_handle,
		.start = wininput_start,
		.shutdown = wininput_shutdown
	};

	if(sizeof(wininput_channel_ident) != sizeof(uint64_t)){
		LOG("Channel identification union out of bounds");
		return 1;
	}

	//register backend
	if(mm_backend_register(wininput)){
		LOG("Failed to register backend");
		return 1;
	}
	return 0;
}

static uint32_t wininput_interval(){
	return cfg.interval;
}

static int wininput_configure(char* option, char* value){
	if(!strcmp(option, "interval")){
		cfg.interval = strtoul(value, NULL, 0);
		return 0;
	}

	LOGPF("Unknown backend configuration option %s", option);
	return 1;
}

static int wininput_configure_instance(instance* inst, char* option, char* value){
	LOG("The backend does not take any instance configuration");
	return 0;
}

static int wininput_instance(instance* inst){
	return 0;
}

static int wininput_subscribe(wininput_channel_ident ident, channel* chan){
	size_t u, n;

	//find an existing request
	for(u = 0; u < cfg.requests; u++){
		if(cfg.request[u].ident.label == ident.label){
			break;
		}
	}
	
	if(u == cfg.requests){
		//create a new request
		cfg.request = realloc(cfg.request, (cfg.requests + 1) * sizeof(wininput_request));
		if(!cfg.request){
			cfg.requests = 0;
			LOG("Failed to allocate memory");
			return 1;
		}

		cfg.request[u].ident.label = ident.label;
		cfg.request[u].channels = 0;
		cfg.request[u].channel = NULL;
		cfg.request[u].state = 0;
		cfg.requests++;
	}

	//check if already in subscriber list
	for(n = 0; n < cfg.request[u].channels; n++){
		if(cfg.request[u].channel[n] == chan){
			return 0;
		}
	}

	//add to subscriber list
	cfg.request[u].channel = realloc(cfg.request[u].channel, (cfg.request[u].channels + 1) * sizeof(channel*));
	if(!cfg.request[u].channel){
		cfg.request[u].channels = 0;
		LOG("Failed to allocate memory");
		return 1;
	}
	cfg.request[u].channel[n] = chan;
	cfg.request[u].channels++;
	return 0;
}

static channel* wininput_channel(instance* inst, char* spec, uint8_t flags){
	size_t u;
	channel* chan = NULL;
	uint16_t scancode = 0;
	char* token = spec;
	wininput_channel_ident ident = {
		.label = 0
	};

	if(!strncmp(spec, "mouse.", 6)){
		//TODO wheel
		token += 6;
		ident.fields.type = mouse;
		if(!strcmp(token, "x")){
			ident.fields.channel = position;
		}
		else if(!strcmp(token, "y")){
			ident.fields.channel = position;
			ident.fields.control = 1;
		}
		else{
			//check the buttons
			for(u = 0; u < sizeof(keys) / sizeof(keys[0]); u++){
				if(keys[u].channel == button && !strcmp(keys[u].name, token)){
					DBGPF("Using keymap %" PRIsize_t " (%d) for spec %s", u, keys[u].keycode, token);
					ident.fields.channel = button;
					ident.fields.control = keys[u].keycode;
					break;
				}
			}

			if(u == sizeof(keys) / sizeof(keys[0])){
				LOGPF("Unknown mouse control %s", token);
				return NULL;
			}
		}
	}
	else if(!strncmp(spec, "key.", 4)){
		token += 4;
		ident.fields.type = keyboard;
		ident.fields.channel = keypress;
	
		for(u = 0; u < sizeof(keys) / sizeof(keys[0]); u++){
			if(keys[u].channel == keypress && !strcmp(keys[u].name, token)){
				DBGPF("Using keymap %" PRIsize_t " (%d) for spec %s", u, keys[u].keycode, token);
				ident.fields.control = keys[u].keycode;
				break;
			}
		}

		//no entry in translation table
		if(u == sizeof(keys) / sizeof(keys[0])){
			if(strlen(token) == 1){
				//try to translate
				scancode = VkKeyScan(token[0]);
				if(scancode != 0x7f7f){
					DBGPF("Using keyscan result %02X (via %04X) for spec %s", scancode & 0xFF, scancode, token);
					ident.fields.type = keyboard;
					ident.fields.channel = keypress;
					ident.fields.control = scancode & 0xFF;
				}
				else{
					LOGPF("Invalid channel specification %s", token);
					return NULL;
				}
			}
			else if(strlen(token) > 1){
				//try to use as literal
				scancode = strtoul(token, NULL, 0);
				if(!scancode){
					LOGPF("Invalid channel specification %s", token);
					return NULL;
				}
				DBGPF("Using direct conversion %d for spec %s", scancode & 0xFF, token);
				ident.fields.control = scancode & 0xFF;
			}
			else{
				LOGPF("Invalid channel specification %s", spec);
				return NULL;
			}
		}
	}
	else{
		LOGPF("Unknown channel spec %s", spec);
	}

	if(ident.label){
		chan = mm_channel(inst, ident.label, 1);
		if(chan && (flags & mmchannel_input) && wininput_subscribe(ident, chan)){
			return NULL;
		}
		return chan;
	}
	return NULL;
}

//for some reason, sendinput only takes "normalized absolute coordinates", which are never again used in the API
static void wininput_mouse_normalize(long* x, long* y){
	//TODO this needs to take a possible origin offset into account
	long normalized_x = (double) (*x) * (65535.0f / (double) cfg.virtual_width);
	long normalized_y = (double) (*y) * (65535.0f / (double) cfg.virtual_height);

	*x = normalized_x;
	*y = normalized_y;
}

static INPUT wininput_event_mouse(uint8_t channel, uint8_t control, double value){
	DWORD flags_down = 0, flags_up = 0;
	INPUT ev = {
		.type = INPUT_MOUSE
	};

	if(channel == position){
		if(control){
			cfg.mouse_y = value * 0xFFFF;
		}
		else{
			cfg.mouse_x = value * 0xFFFF;
		}

		ev.mi.dwFlags |= MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;
		ev.mi.dx = cfg.mouse_x;
		ev.mi.dy = cfg.mouse_y;
	}
	if(channel == button){
		switch(control){
			case VK_LBUTTON:
				flags_up |= MOUSEEVENTF_LEFTUP;
				flags_down |= MOUSEEVENTF_LEFTDOWN;
				break;
			case VK_RBUTTON:
				flags_up |= MOUSEEVENTF_RIGHTUP;
				flags_down |= MOUSEEVENTF_RIGHTDOWN;
				break;
			case VK_MBUTTON:
				flags_up |= MOUSEEVENTF_MIDDLEUP;
				flags_down |= MOUSEEVENTF_MIDDLEDOWN;
				break;
			case VK_XBUTTON1:
			case VK_XBUTTON2:
				ev.mi.mouseData = (control == VK_XBUTTON1) ? XBUTTON1 : XBUTTON2;
				flags_up |= MOUSEEVENTF_XUP;
				flags_down |= MOUSEEVENTF_XDOWN;
				break;
		}

		if(value > 0.9){
			ev.mi.dwFlags |= flags_down;
		}
		else{
			ev.mi.dwFlags |= flags_up;
		}
	}

	return ev;
}

static INPUT wininput_event_keyboard(uint8_t channel, uint8_t control, double value){
	INPUT ev = {
		.type = INPUT_KEYBOARD
	};

	if(channel == keypress){
		ev.ki.wVk = control;
		if(value < 0.9){
			ev.ki.dwFlags |= KEYEVENTF_KEYUP;
		}
	}

	return ev;
}

static int wininput_set(instance* inst, size_t num, channel** c, channel_value* v){
	wininput_channel_ident ident = {
		.label = 0
	};
	size_t n = 0, offset = 0;
	INPUT events[500];

	if(num > sizeof(events) / sizeof(events[0])){
		LOGPF("Truncating output on %s to the last %" PRIsize_t " events, please notify the developers", inst->name, sizeof(events) / sizeof(events[0]));
		offset = num - sizeof(events) / sizeof(events[0]);
	}

	for(n = 0; n + offset < num; n++){
		ident.label = c[n + offset]->ident;
		if(ident.fields.type == mouse){
			events[n] = wininput_event_mouse(ident.fields.channel, ident.fields.control, v[n + offset].normalised);
		}
		else if(ident.fields.type == keyboard){
			events[n] = wininput_event_keyboard(ident.fields.channel, ident.fields.control, v[n + offset].normalised);
		}
		else{
			n--;
			offset++;
		}
	}

	if(n){
		offset = SendInput(n, events, sizeof(INPUT));
		if(offset != n){
			LOGPF("Output %" PRIsize_t " of %" PRIsize_t " events on %s", offset, n, inst->name);
		}
	}
	return 0;
}

static int wininput_handle(size_t num, managed_fd* fds){
	channel_value val = {
		.normalised = 0
	};
	uint8_t mouse_updated = 0, synthesize_off = 0, push_event = 0;
	uint16_t key_state = 0;
	POINT cursor_position;
	size_t u = 0, n;

	for(u = 0; u < cfg.requests; u++){
		synthesize_off = 0;
		push_event = 0;
		val.normalised = 0;

		if(cfg.request[u].ident.fields.type == mouse
				&& cfg.request[u].ident.fields.channel == position){
			if(!mouse_updated){
				//update mouse coordinates
				if(!GetCursorPos(&cursor_position)){
					LOG("Failed to update mouse position");
					continue;
				}
				wininput_mouse_normalize(&cursor_position.x, &cursor_position.y);
				mouse_updated = 1;
				if(cfg.mouse_x != cursor_position.x
						|| cfg.mouse_y != cursor_position.y){
					cfg.mouse_x = cursor_position.x;
					cfg.mouse_y = cursor_position.y;
					mouse_updated = 2;
				}
			}

			val.normalised = (double) cfg.mouse_x / (double) 0xFFFF;
			if(cfg.request[u].ident.fields.control){
				val.normalised = (double) cfg.mouse_y / (double) 0xFFFF;
			}

			if(mouse_updated == 2){
				push_event = 1;
			}
		}
		else{
			//check key state
			key_state = GetAsyncKeyState(cfg.request[u].ident.fields.control);
			if(key_state == 1){
				//pressed and released?
				synthesize_off = 1;
			}
			if((key_state & ~1) != cfg.request[u].state){
				//key state changed
				if(key_state){
					val.normalised = 1.0;
				}
				cfg.request[u].state = key_state & ~1;
				push_event = 1;
			}
		}

		if(push_event){
			//push current value to all channels
			DBGPF("Pushing event %f on request %" PRIsize_t, val.normalised, u);
			for(n = 0; n < cfg.request[u].channels; n++){
				mm_channel_event(cfg.request[u].channel[n], val);
			}

			if(synthesize_off){
				val.normalised = 0;
				//push synthesized value to all channels
				DBGPF("Synthesizing event %f on request %" PRIsize_t, val.normalised, u);
				for(n = 0; n < cfg.request[u].channels; n++){
					mm_channel_event(cfg.request[u].channel[n], val);
				}
			}
		}
	}
	return 0;
}

static int wininput_start(size_t n, instance** inst){
	POINT cursor_position;

	//if no input requested, don't request polling
	if(!cfg.requests){
		cfg.interval = 0;
	}

	//read virtual desktop extents for later normalization
	cfg.virtual_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	cfg.virtual_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	cfg.virtual_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
	cfg.virtual_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
	DBGPF("Virtual screen is %dx%d with offset %dx%d", cfg.virtual_width, cfg.virtual_height, cfg.virtual_x, cfg.virtual_y);

	//initialize mouse position
	if(!GetCursorPos(&cursor_position)){
		LOG("Failed to read initial mouse position");
		return 1;
	}

	DBGPF("Current mouse coordinates: %dx%d (%04Xx%04X)", cursor_position.x, cursor_position.y, cursor_position.x, cursor_position.y);
	wininput_mouse_normalize(&cursor_position.x, &cursor_position.y);
	DBGPF("Current normalized mouse position: %04Xx%04X", cursor_position.x, cursor_position.y);
	cfg.mouse_x = cursor_position.x;
	cfg.mouse_y = cursor_position.y;

	DBGPF("Tracking %" PRIsize_t " input requests", cfg.requests);
	return 0;
}

static int wininput_shutdown(size_t n, instance** inst){
	size_t u;

	for(u = 0; u < cfg.requests; u++){
		free(cfg.request[u].channel);
	}
	free(cfg.request);
	cfg.request = NULL;
	cfg.requests = 0;

	LOG("Backend shut down");
	return 0;
}
