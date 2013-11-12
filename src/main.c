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
char strPage[2048];
#define PAGE_SIZE 2048
void window_load(Window *me) {
  const GRect max_text_bounds = GRect(0, 0, 144, 2000);
	
  scroll_layer_init(&scrollLayer, me->layer.bounds);
  scroll_layer_set_click_config_onto_window(&scrollLayer, me);
  scroll_layer_set_content_size(&scrollLayer, max_text_bounds.size);
	
  text_layer_init(&textLayer, max_text_bounds);
  //text_layer_set_text(&textLayer, "Loading...");
  text_layer_set_font(&textLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  //text_layer_set_overflow_mode(&textLayer, GTextOverflowModeWordWrap);
	
  resource_init_current_app(&APP_RESOURCES);
  ResHandle rhStory = resource_get_handle(RESOURCE_ID_STORY);
  int nLen = resource_load(rhStory, (uint8_t*)strPage, PAGE_SIZE-1);
  strPage[nLen] = 0x0;
  //snprintf(strPage, PAGE_SIZE, "Loaded %d bytes.\n", nLen);

  text_layer_set_text(&textLayer, strPage);
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
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init
  };
  app_event_loop(params, &handlers);
}
