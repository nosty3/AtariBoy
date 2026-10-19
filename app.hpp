#pragma once
#include "controls.hpp"
#include "video.hpp"
#include "browser.hpp"
#include "options_ui.hpp"
#include "state_ui.hpp"
#include "settings.hpp"
#include "cadence.hpp"
#include "fps.hpp"
#include "color_clock.hpp"
struct App {
 bool title=true,title_armed=false,title_wait_release=false;
 static void draw_title(uint8_t *out);
 bool paused=false,in_options=false,in_help=false,prev_help=false,prev_option=false;
 Settings options_before;
 StateUI state_ui;bool in_states=false;
 bool option_armed=false;
 bool debug=false;
 uint8_t next_pad=255;
 bool can_emulate=false;
 uint32_t output_fps=0;
 bool advance(uint32_t now,bool packet=false);
 void render_game(uint8_t *out,uint32_t now);
 OptionsUI options_ui;
 unsigned screen_mode()const{return in_states?7:title?6:in_help?3:in_options?(options_ui.tab?3:2):(active?(settings.picture>=2?settings.picture+2:1):0);}
 bool active=false,loaded=false,prev_left=false,prev_fire=false,prev_esc=false,suppress=false;
 int held_direction=0;unsigned hold_ticks=0;
 ColorClock color_clock;controls::Decoder decoder;video::Options options;Browser browser;
 uint8_t view_frame=0;bool view_frame_valid=false;
 unsigned tv_hz=50;Cadence cadence;Fps fps;
 void finish_frame(uint8_t *out,uint32_t now);
 bool last_dirty=false;
 void init();
 bool start_game(const char *file,uint32_t now,bool remember=true);
 unsigned previous_key=256,pending_operation=0;
 void frame(const uint8_t *input,uint8_t *output,uint32_t now=0);
 void perform_pending(uint32_t now);
};
