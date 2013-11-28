#include <pebble.h>

static Window *window;
static ScrollLayer *scrollLayer;
static TextLayer *textLayer;
static ResHandle rhStory;
#define PAGE_MAX_SIZE 255
char strPage[PAGE_MAX_SIZE*2+1];
int nPageSize;
int nPageOffset = 0;
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
	Also replaces leading illegal bytes with ' '-s.
	Returns resulting useful buffer size.
	@buf allocated memory must be at least size+1 !
	
	TODO: maybe simplify function - don't check mb char validity and just remove last one?
*/
int mbTrim(char* buf, int size) {
	// firstly fix any leading illegal chars
	for(int i=0; i<size && buf[i] >> 6 == 2; i++)
		buf[i] = ' ';
	
	int bc = 0; // counter for multibyte continuation bytes at end
	for(int i=size-1; i>=0; i++) {
		if(buf[i] >> 7 == 0) { // 0xxxxxxx - one-byte character; it is valid!
			buf[i+1] = 0; // so make it last significant character in buffer
			return i+1;
		} else if(buf[i] >> 6 == 2) { // 10xxxxxx - continuation byte
			bc++;
		} else { // 11...... - multibyte character start
			for(int n=buf[i]; n >> 7 != 0; n = n<<1) bc--;
			if(bc == 0) { // valid character
				buf[size] = 0;
				return size; // multibyte character is okay
			} else { // invalid - remove it!
				buf[i] = 0;
				return i;
			}
		}
	}
	// if we reached beginning of line without finding anything valid - strange, but maybe, e.g. if line is (almost) empty
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
	GRect maxBox = layer_get_frame(scroll_layer_get_layer(scrollLayer));
	int maxH = maxBox.size.h; // window height; maybe =144.
	maxBox.size.h = 1000; // equals unlimited vertical size; TODO: shrink in order to optimize things?
	LOG("maxH:%d mBsW:%d\n", maxH, maxBox.size.w);

	// save current text...
	const char *currText = text_layer_get_text(textLayer);
	for(int i = bufCount-1, j=0; i>=startPos && j<100; j++) {
		char bak = buf[i+1]; buf[i+1] = 0; // trim string
		text_layer_set_text(textLayer, buf+startPos);
		GSize currSize = text_layer_get_content_size(textLayer);
		   /*	graphics_text_layout_get_content_size(
			app_get_current_graphics_context(), buf+startPos,
			currentFont, maxBox, 
			GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL); */
		buf[i+1] = bak; // restore
		LOG("currSizeH:%d i:%d\n", currSize.h, i);
		if(currSize.h <= maxH) { // fits?
			// ...and restore it
			text_layer_set_text(textLayer, currText);
			return i-startPos+1;
		}
		while(buf[i] != ' ') i--; // trim all non-spaces
		while(buf[i] == ' ') i--; // now trim all spaces
	}
	// restore text??
	// We must not get here! Because zero-sized page MUST always fit
	APP_LOG(APP_LOG_LEVEL_ERROR, "getPageSize: illegal state: zero page didn't fit?");
	return 0;
}

void loadPage(int offset) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "in loadPage(%d)", offset);
	rhStory = resource_get_handle(RESOURCE_ID_STORY);
	nPageSize = resource_load_byte_range(rhStory, offset, (uint8_t*)strPage, PAGE_MAX_SIZE*2);
	nPageSize = mbTrim(strPage, nPageSize); // will also set terminating char
	// conditional is to avoid unneccessary work: cuz buffer may be twice as large
	nPageSize = getPageSize(strPage, 0, nPageSize>PAGE_MAX_SIZE?PAGE_MAX_SIZE:nPageSize);
	strPage[nPageSize] = '@';

#ifdef LOGGING
	text_layer_set_text(textLayer, szLog);
#else
	text_layer_set_text(textLayer, strPage);
#endif
	GSize max_size = text_layer_get_content_size(textLayer);
	text_layer_set_size(textLayer, max_size);
	scroll_layer_set_content_size(scrollLayer, GSize(144, max_size.h+4));
	if(offset > 0)
		scroll_layer_set_content_offset(scrollLayer, GPoint(0, -1), false); // to show upper shadow

	nPageOffset = offset;
}

void up_single_click_handler(ClickRecognizerRef recognizer, void* context) {
	scroll_layer_scroll_up_click_handler(recognizer, scrollLayer);
}
void down_single_click_handler(ClickRecognizerRef recognizer, void* context) {
	scroll_layer_scroll_down_click_handler(recognizer, scrollLayer);
}
void select_single_click_handler(ClickRecognizerRef recognizer, void* context) {
	loadPage(nPageOffset + nPageSize + 1); // +1 is a space char
}

void scrollLayerConfigureClicks(void* context) {
	window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
	window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
	window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
}

void window_load(Window *me) {
	const GRect max_text_bounds = GRect(0, 0, 144, 2000);

	Layer *window_layer = window_get_root_layer(window);
	const GRect bounds = layer_get_bounds(window_layer);

	scrollLayer = scroll_layer_create(bounds);
	scroll_layer_set_click_config_onto_window(scrollLayer, me);
	scroll_layer_set_content_size(scrollLayer, max_text_bounds.size);
	scroll_layer_set_callbacks(scrollLayer, (ScrollLayerCallbacks){
		.click_config_provider = scrollLayerConfigureClicks,
	});

	textLayer = text_layer_create(max_text_bounds);
	//text_layer_set_text(textLayer, "Loading...");
	currentFont = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
	text_layer_set_font(textLayer, currentFont);
	//text_layer_set_overflow_mode(&textLayer, GTextOverflowModeWordWrap);

	//resource_init_current_app(&APP_RESOURCES);
	loadPage(0);
	
	scroll_layer_add_child(scrollLayer, text_layer_get_layer(textLayer));
	layer_add_child(window_layer, scroll_layer_get_layer(scrollLayer));
}

void window_unload(Window *me) {
	text_layer_destroy(textLayer);
	scroll_layer_destroy(scrollLayer);
}
	
void init() {
	window = window_create();
	window_set_window_handlers(window, (WindowHandlers){
		.load = window_load,
		.unload = window_unload,
	});
	window_stack_push(window, true /* Animated */);
}
void deinit() {
	window_destroy(window);
}

int main() {
	LOG(">>\n"); // to avoid garbage without any 0x0
	init();
	app_event_loop();
	deinit();
}
