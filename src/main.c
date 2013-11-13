#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"


#define MY_UUID { 0x87, 0x08, 0x9F, 0x81, 0xB8, 0x06, 0x46, 0xC4, 0xBB, 0x35, 0x66, 0x8F, 0x5A, 0x39, 0xAA, 0x01 }
PBL_APP_INFO(MY_UUID,
			"BookReader", "MarSoft",
			1, 0, /* App version */
			RESOURCE_ID_ICON_BOOK,
			APP_INFO_STANDARD_APP);

Window window;
ScrollLayer scrollLayer;
TextLayer textLayer;
ResHandle rhStory;
char strPage[256];
#define PAGE_MAX_SIZE 255
GFont currentFont;

//#define LOGGING
#ifdef LOGGING
char szLog[1024];
int logPos = 0;
#define LOG(args...) logPos += snprintf(szLog+logPos, 1023-logPos, args)
#else
#define LOG(args...)
#endif

/**
	Trims trailing parts of multibyte characters in buffer
	(replaces with 0-byte).
	Returns resulting useful buffer size.
	@buf allocated memory must be at least size+1 !
	
	TODO: maybe simplify function - don't check mb chars and just remove last one?
*/
int mbTrim(char* buf, int size) {
	int bc = 0; // counter for multibyte continuation bytes at end
	for(int i=size-1; i>=0; i++) {
		if(buf[i] >> 7 == 0) { // 0xxxxxxx - one-byte character; it is valid!
			buf[i+1] = 0; // so make it last significant character in buffer
			return i+1;
		}
		if(buf[i] >> 6 == 2) { // 10xxxxxx - continuation byte
			bc++;
		} else { // 11...... - multibyte character start
			for(int n=buf[i]; n >> 7 != 0; n = n<<1) bc--;
			if(bc == 0) { // valid character
				return size; // everything allright
			} else { // invalid - remove it!
				buf[i] = 0;
				return i;
			}
		}
	}
	// if we reached beginning of line without finding anything valid - strange, but maybe
	buf[0] = 0; // clear string
	return 0;
}

/**
	Returns count of characters in buffer, starting from startPos,
	which will fit exactly one page.
	Return value is <= (bufCount - startPos).
	@arg bufCount count of (usable) characters in buffer
	@arg startPos < bufCount
*/
int getPageSize(char* buf, int startPos, int bufCount) {
	// validation:
	if(startPos >= bufCount) {
		APP_LOG(APP_LOG_LEVEL_ERROR, "getPageSize: illegal arguments: %d >= %d", startPos, bufCount);
		return 0;
	}
	if(buf[startPos] >> 6 == 2) { // multibyte-continuation symbol must not be the first symbol
		APP_LOG(APP_LOG_LEVEL_ERROR, "getPageSize: startPos=%d points illegal byte: 0x%X", startPos, buf[startPos]);
		// or maybe we could shift startPos?..
		return 0;
	}
	// now main logic:
	GRect maxBox = layer_get_frame((Layer*)&scrollLayer);
	int maxH = maxBox.size.h; // window height; maybe =144.
	maxBox.size.h = 1000; // equals unlimited vertical size; TODO: shrink in order to optimize things?
	LOG("maxH:%d mBsW:%d\n", maxH, maxBox.size.w);
	for(int i = bufCount-1, j=0; i>=startPos && j<100; j++) {
		char bak = buf[i+1]; buf[i+1] = 0; // trim string
		GSize currSize = graphics_text_layout_get_max_used_size(
			app_get_current_graphics_context(), buf+startPos,
			currentFont, maxBox, 
			GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
		buf[i+1] = bak; // restore
		LOG("currSizeH:%d i:%d\n", currSize.h, i);
		if(currSize.h <= maxH) { // fits?
			return i-startPos+1;
		}
		while(buf[i] != ' ') i--; // trim all non-spaces
		while(buf[i] == ' ') i--; // now trim all spaces
	}
	// We must not get here! Because zero-sized page MUST always fit
	APP_LOG(APP_LOG_LEVEL_ERROR, "getPageSize: illegal state: zero page didn't fit?");
	return 0;
}

void window_load(Window *me) {
	const GRect max_text_bounds = GRect(0, 0, 144, 2000);

	scroll_layer_init(&scrollLayer, me->layer.bounds);
	scroll_layer_set_click_config_onto_window(&scrollLayer, me);
	scroll_layer_set_content_size(&scrollLayer, max_text_bounds.size);

	text_layer_init(&textLayer, max_text_bounds);
	//text_layer_set_text(&textLayer, "Loading...");
	currentFont = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
	text_layer_set_font(&textLayer, currentFont);
	//text_layer_set_overflow_mode(&textLayer, GTextOverflowModeWordWrap);

	resource_init_current_app(&APP_RESOURCES);
	ResHandle rhStory = resource_get_handle(RESOURCE_ID_STORY);
	int nLen = resource_load(rhStory, (uint8_t*)strPage, PAGE_MAX_SIZE);
	nLen = mbTrim(strPage, nLen); // will also set terminating char
	strPage[getPageSize(strPage, 0, nLen)] = 0; // tmp: trim to just one page

#ifdef LOGGING
	text_layer_set_text(&textLayer, szLog);
#else
	text_layer_set_text(&textLayer, strPage);
#endif
	GSize max_size = text_layer_get_max_used_size(app_get_current_graphics_context(), &textLayer);
	text_layer_set_size(&textLayer, max_size);
	scroll_layer_set_content_size(&scrollLayer, GSize(144, max_size.h+4));
	
	scroll_layer_add_child(&scrollLayer, &textLayer.layer);
	layer_add_child(&me->layer, &scrollLayer.layer);
}
	
void handle_init(AppContextRef ctx) {
	(void)ctx;
	window_init(&window, "Book Reader");
	window_set_window_handlers(&window, (WindowHandlers){
		.load = window_load,
	});
	window_stack_push(&window, true /* Animated */);
}


void pbl_main(void *params) {
	LOG(">>\n"); // to avoid garbage without any 0x0
	PebbleAppHandlers handlers = {
		.init_handler = &handle_init
	};
	app_event_loop(params, &handlers);
}
