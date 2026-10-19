#include "app.hpp"
#include "engine.h"
#include "audio.h"
#include <string.h>
#include <stdio.h>
#include "saves.h"
#include "osd.hpp"
#include "generated/title_logo.hpp"
#include "storage.h"
static void slot_message(const char *format,uint32_t now){char text[21];snprintf(text,sizeof text,format,saves_slot());osd_show(text,now);}
void App::init(){
 browser.init();settings_load();last_game_load();

}

void App::draw_title(uint8_t *out){
 memset(out,0,3200);memcpy(out,title_logo,sizeof title_logo);
 auto line=[&](unsigned row,const char *text,bool center=false,bool inverse=false){
  unsigned n=strlen(text),x=center?(40-n)/2:0;uint8_t *dst=out+2560+row*40+x;
  for(unsigned i=0;i<n;++i){unsigned c=(unsigned char)text[i];dst[i]=(c>=32&&c<96?c-32:c)|(inverse?128:0);}
 };
 line(1,"GAME BOY EMULATOR FOR 8-BIT ATARI",true);
 line(2,"V.0.90b",true);
 line(4,"AUTHORS:");
 line(5,"  NOSTY, GPT-6 ASTRA");
 line(6,"SPECIAL THANKS TO:");
 line(7,"  LAREK, BOCIANU, MQ");
 line(8,"CREATED USING:");
 line(9,"  PEANUT-GB BY MAHYAR KOSHKOUEI");
 line(10,"  A8PICOCART CODE BY ROBIN EDWARDS");
 line(12,"TO START A8PICOCART, TURN ON ATARI",true);
 line(13,"WHILE HOLDING DOWN THE \"A\" KEY.",true);
 line(15," PRESS SPACE OR FIRE ",true,true);
}

bool App::start_game(const char *file,uint32_t now,bool remember){
 if(loaded){SaveResult saved=battery_save();if(saved!=SAVE_OK&&saved!=SAVE_EMPTY){strcpy(browser.message,saved==SAVE_FULL?"NO SPACE ON DISC!":"BATTERY SAVE FAILED!");return false;}}
 if(!engine_start(file))return false;
 saves_select_game(file);osd_show("",now);
 SaveResult prefs=settings_load_game(true);
 if(prefs!=SAVE_OK&&prefs!=SAVE_EMPTY)osd_show("OPTIONS LOAD FAILED!",now);
 SaveResult battery=battery_load();
 if(battery!=SAVE_OK&&battery!=SAVE_EMPTY)osd_show("BATTERY LOAD FAILED!",now);
 loaded=active=true;paused=false;suppress=true;cadence.reset();fps.reset(now);
 options.top=(144-video::rows)/2;
 if(settings.load_latest){SaveResult r=saves_load();if(r==SAVE_OK)slot_message("LOADED FROM SLOT %02u",now);else if(r!=SAVE_EMPTY)osd_show("LOAD FAILED!",now);}
 if(remember&&(last_dirty||strcmp(file,last_game_path))){
  strncpy(last_game_path,file,sizeof last_game_path-1);last_game_path[sizeof last_game_path-1]=0;last_dirty=true;pending_operation=4;
 }
 return true;
}
void App::perform_pending(uint32_t now){
 unsigned op=pending_operation;pending_operation=0;if(!op)return;
 cadence.reset();
 if(op==10){
  uint32_t released=0;bool ok=storage_optimize_disk(&released);
  options_ui.optimize=options_ui.confirm=options_ui.yes=false;
  if(ok)snprintf(options_ui.message,sizeof options_ui.message,"Done: %lu KiB reclaimed",(unsigned long)(released/2));
  else snprintf(options_ui.message,sizeof options_ui.message,"%s",storage_error());
  suppress=true;return;
 }
 if(op==9){
  engine_stop();active=loaded=paused=in_options=in_help=in_states=false;
  saves_forget_game();state_ui.clipboard=false;settings_defaults();settings_apply_audio();global_settings=settings;last_game_path[0]=0;last_dirty=false;
  bool ok=storage_erase_disk();
  browser.path[0]='/';browser.path[1]=0;browser.selected=browser.top=browser.count=browser.page_count=browser.ticks=0;browser.message[0]=0;browser.ready=false;browser.init();
  if(!ok)strcpy(browser.message,"DISK ERASE/FORMAT FAILED!");
  options_ui.open();decoder=controls::Decoder{};suppress=true;fps.reset(now);
  return;
 }
 if(op==7){unsigned action=state_ui.operation;SaveResult r=state_ui.perform();
  if(r==SAVE_OK){if(action==1||action==6)browser.note_save(saves_game_name());if(action==3)browser.refresh_saves();
   if(action==2){saves_clear_clipboard();state_ui.clipboard=false;in_states=false;suppress=true;fps.reset(now);}
  }return;
 }
 if(op==4){
  SaveResult r=last_game_save();last_dirty=r!=SAVE_OK;
  if(last_dirty)osd_show(r==SAVE_FULL?"NO SPACE ON DISC!":"LAST GAME NOT SAVED",now);
  return;
 }
 if(op==5){
  SaveResult r=battery_save();
  if(r==SAVE_OK){browser.note_save(saves_game_name());}
  else if(r!=SAVE_EMPTY){active=true;suppress=true;osd_show(r==SAVE_FULL?"NO SPACE ON DISC!":"BATTERY SAVE FAILED!",now);}
  return;
 }
 if(op==3||op==6){
  SaveResult r=op==3?settings_save_game():settings_save();
  if(r==SAVE_OK){
   if(op==3){browser.note_options(saves_game_name());options_before=settings;strcpy(options_ui.message,"GAME OPTIONS SAVED");}
   else {in_options=false;suppress=true;options_ui.message[0]=0;}
  }
  else strcpy(options_ui.message,r==SAVE_FULL?"NO SPACE ON DISC!":"OPTIONS SAVE FAILED!");
  return;
 }
 SaveResult result=op==1?saves_write():saves_load();
 // Caller refreshes time after potentially lengthy flash IO via a separate call.
 if(result==SAVE_OK){
  slot_message(op==1?"SAVED IN SLOT %02u":"LOADED FROM SLOT %02u",now);
  if(op==1){browser.note_save(saves_game_name());if(settings.advance_slot)saves_move(1);}
 }
 else if(result==SAVE_FULL)osd_show("NO SPACE ON DISC!",now);
 else if(op==2&&result==SAVE_EMPTY)slot_message("SLOT %02u EMPTY",now);
 else osd_show(op==1?"SAVE FAILED!":"LOAD FAILED!",now);
}
void App::frame(const uint8_t *p,uint8_t *out,uint32_t now){
 can_emulate=false;
 memset(audio_frame,0,sizeof audio_frame);
 if(title){
  // No hardware choice is stored: wait for this Atari's detector before exit.
  uint8_t stereo=__atomic_load_n(&detected_stereo,__ATOMIC_RELAXED);
  if(stereo>1){draw_title(out);return;}
  if(settings.audio_stereo!=stereo)settings_apply_audio();
  bool pressed=p[controls::HeldKey]==0x21||p[controls::Fire0]==0;
  bool released=p[controls::Fire0]==1&&p[controls::HeldKey]!=0x21;
  if(!title_wait_release){
   if(released)title_armed=true;
   if(title_armed&&pressed)title_wait_release=true;
  }else if(released){
   title=false;previous_key=p[controls::HeldKey];prev_fire=false;prev_esc=false;
   if(settings.boot_last&&last_game_path[0]&&!start_game(last_game_path,now,false)){
    active=loaded=false;strncpy(browser.message,engine_message(),sizeof browser.message-1);
   }
  }
  if(title){draw_title(out);return;}
 }

 bool left=!(p[controls::Joy0]&4)||p[controls::HeldKey]==0x06;
 bool back_edge=left&&!prev_left;prev_left=left;
 decoder.update(p);
 const controls::Input input=decoder.read(p,settings.controls);
 color_clock.tick(now,settings.auto_minutes,settings.hue,!in_options);
 // Atari frame counter, not emulated GB frames. Record even in menu/help.
 unsigned view_ticks=view_frame_valid?uint8_t(p[controls::Frame]-view_frame):1;
 view_frame=p[controls::Frame];view_frame_valid=true;
 if(view_ticks>36)view_ticks=36;
 unsigned key=p[controls::HeldKey];
 bool esc=key==0x1c;
 bool fire=p[controls::Fire0]==0||key==0x0c;
 // Atari hardware scan codes: '-' (Up), '=' (Down), with or without Ctrl.
 bool up=!(p[controls::Joy0]&1)||key==0x0e;
 bool down=!(p[controls::Joy0]&2)||key==0x0f;
 bool transitioned=false;
 bool option=p[controls::Option]==1;
 // Only a real 0 releases/arms OPTION. FF at startup is neither press nor release.
 if(p[controls::Option]==0)option_armed=true;
 bool option_edge=option&&option_armed&&!prev_option;
 if(option)option_armed=false;
 prev_option=option;
 bool help=key==0x11||p[controls::Help]==1;
 bool help_edge=help&&!prev_help;prev_help=help;
 if(active&&!in_options&&!in_help&&!in_states&&key==0x3d&&previous_key!=key){state_ui.open();state_ui.previous=key;in_states=true;transitioned=true;}
 if(in_states){
  cadence.reset();
  if(!transitioned&&state_ui.update(p)){saves_clear_clipboard();state_ui.clipboard=false;in_states=false;suppress=true;transitioned=true;fps.reset(now);}
  else {if(state_ui.operation)pending_operation=7;state_ui.draw(out);prev_fire=fire;prev_esc=esc;previous_key=key;return;}
 }
 if(active&&!in_options&&help_edge&&!in_help){in_help=true;transitioned=true;}
 if(in_help){
  cadence.reset();
  if(!transitioned&&((esc&&!prev_esc)||help_edge)){in_help=false;transitioned=true;suppress=true;fps.reset(now);}
  else {OptionsUI::help(out);prev_fire=fire;prev_esc=esc;previous_key=key;return;}
 }
 if(option_edge&&!in_options&&!transitioned){options_before=settings;in_options=true;options_ui.open();if(!active)options_ui.tab=6;transitioned=true;}
 if(in_options){
  cadence.reset();
  if(!pending_operation&&esc&&!prev_esc&&options_ui.confirm){options_ui.confirm=false;options_ui.yes=false;options_ui.draw(out);prev_esc=esc;previous_key=key;return;}
  if(!pending_operation&&esc&&!prev_esc){settings=options_before;settings.boot_last=global_settings.boot_last;settings_apply_audio();in_options=false;transitioned=true;suppress=true;fps.reset(now);}
  else {
   if(option_edge&&!transitioned&&!options_ui.confirm){pending_operation=6;strcpy(options_ui.message,"Saving global options...");}
   if(!transitioned&&!pending_operation){
    if(!options_ui.confirm&&active&&key!=previous_key&&key==0x3e){pending_operation=3;strcpy(options_ui.message,"Saving game options...");}
    else if(!options_ui.confirm&&key!=previous_key&&key==0x3d){settings_load();strcpy(options_ui.message,"GLOBAL OPTIONS LOADED");}
    else if(!options_ui.confirm&&active&&key!=previous_key&&key==0x00){SaveResult r=settings_load_game();strcpy(options_ui.message,r==SAVE_OK?"GAME OPTIONS LOADED":r==SAVE_EMPTY?"NO OPTIONS FOR THIS GAME":"OPTIONS LOAD FAILED!");}
    else options_ui.update(p,(fire&&!prev_fire)||(key==0x0c&&key!=previous_key));
    if(options_ui.erase)pending_operation=9;
    else if(options_ui.optimize)pending_operation=10;
    if(options_ui.row>options_ui.fields())options_ui.row=options_ui.fields();
   }
   options_ui.draw(out);prev_fire=fire;prev_esc=esc;previous_key=key;return;
  }
 }
 bool debug_key=key==0x3a&&(p[controls::Kbcode]&0xbf)==0xba;

 bool turbo_key=key==0x2d;
 unsigned key_token=debug_key?(key|0x80):key;
 bool command=active&&(key==0x25||key==0x0a||key==0x3e||key==0x00||key==0x17||key==0x16||key==0x38||debug_key||key==0x28||key==0x12||key==0x10||key==0x23||turbo_key);
 if(command&&!transitioned&&key_token!=previous_key){
  if(turbo_key){settings.turbo^=1;}
  else if(key==0x28){
   cadence.reset();fps.reset(now);paused=false;suppress=true;transitioned=true;
   options.top=(144-video::rows)/2;
   osd_show(engine_reset()?"GAME RESET":"RESET FAILED!",now);
  }
  else if(key==0x25){cadence.reset();audio_set_enabled(!audio_enabled);settings.sound=audio_enabled;osd_show(audio_enabled?"SOUND ON":"SOUND OFF",now);}
  else if(key==0x23){settings.audio_quality^=1;settings.audio_effects=settings.audio_quality?6:4;audio_set_quality(settings.audio_quality);cadence.reset();osd_show(settings.audio_quality?"AUDIO QUALITY":"AUDIO FAST",now);}
  else if(key==0x12||key==0x10){settings.hue=(settings.hue+(key==0x12?15:1))&15;char text[21];snprintf(text,sizeof text,"COLOR %02u",settings.hue);osd_show(text,now);}
  else if(key==0x3a){debug=!debug;fps.reset(now);osd_show(debug?"DEBUG ON":"DEBUG OFF",now);}
  else if(key==0x38){fps.toggle(now);osd_show(fps.enabled?"FPS ON":"FPS OFF",now);}
  else if(key==0x0a){paused=!paused;osd_show(paused?"PAUSE":"RESUME",now);}
  else if(key==0x3e){pending_operation=1;osd_show("SAVING...",now);}
  else if(key==0x00){pending_operation=2;}
  else if(key==0x17||key==0x16){saves_move(key==0x17?-1:1);slot_message("SLOT %02u SELECTED",now);}
 }
 previous_key=key_token;
 if(esc&&!prev_esc&&!transitioned){
  if(active){active=false;transitioned=true;pending_operation=5;}
  else if(loaded){active=true;suppress=true;transitioned=true;}
 }
 if(!active&&!transitioned){
  if(back_edge)browser.back();
  int direction=up&&!down?-1:down&&!up?1:0;
  if(direction!=held_direction){held_direction=direction;hold_ticks=0;if(direction)browser.move(direction);}
  else if(direction&&++hold_ticks>=20&&(hold_ticks-20)%4==0)browser.move(direction);
  if(fire&&!prev_fire&&!back_edge){
   char file[1024];
   if(browser.choose(file,sizeof file)){
    if(start_game(file,now)){transitioned=true;}
    else if(!browser.message[0]){strncpy(browser.message,engine_message(),sizeof browser.message-1);}
   }
  }
 }
 prev_esc=esc;prev_fire=fire;
 if(active){
  // Clock changes require Atari's GAME loop handshake. Editing options only
  // changes the desired setting; apply after returning, including saved presets.
  if(!pending_operation&&settings.turbo!=clock_turbo){
   pending_operation=8;osd_show("SWITCHING CLOCK...",now);
  }
  if(suppress&&!fire&&!esc)suppress=false;
  if(input.load_state&&!transitioned&&!suppress&&!pending_operation)pending_operation=2;
  if(input.change_view&&!transitioned&&!suppress&&!pending_operation)settings.picture=(settings.picture+input.view_step+4)%4;
  options.method=static_cast<video::Method>(settings.picture);
  options.height=settings.picture>=2?144:video::rows;
  if(settings.picture==1&&!transitioned&&!pending_operation){
   bool view_up=input.view_up,view_down=input.view_down;
   unsigned limit=144-options.height;
   if(view_up&&!view_down)options.top=options.top>view_ticks?options.top-view_ticks:0;
   if(view_down&&!view_up)options.top=options.top+view_ticks<limit?options.top+view_ticks:limit;
  }
  if(!paused&&!transitioned&&!pending_operation){
   next_pad=suppress||command?255:input.pad;
   can_emulate=true;
   advance(now,true);
  }else cadence.reset();
  if(active){video::convert(gb_screen,out,options);osd_overlay(out,options.height,now,options.method==video::Method::Full192);}
 }
 if(!active){cadence.reset();browser.draw(out,loaded);}
}

void App::finish_frame(uint8_t *out,uint32_t now){
 if(!fps.enabled&&!debug)return;
 if(!active||in_options||in_help||in_states){fps.reset(now);return;}
 fps.update(now);
 char text[21];snprintf(text,sizeof text,"FPS%lu.%lu OUT%lu.%lu",(unsigned long)(fps.tenths/10),(unsigned long)(fps.tenths%10),(unsigned long)(output_fps/10),(unsigned long)(output_fps%10));
 osd_text(out,options.height,0,text);
}

// One complete GB frame per call. The outer loop can publish immediately,
// service PCM independently, and sample inputs before beginning another frame.
bool App::advance(uint32_t now,bool packet){
 if(!can_emulate||!active||paused||in_options||in_help||in_states||pending_operation)return false;
 bool full=tv_hz==60||(settings.pal_full&&tv_hz==50);
 if(full){if(!cadence.due(now))return false;}
 else if(!packet)return false;
 if(!engine_frame(next_pad)){active=loaded=false;can_emulate=false;strcpy(browser.message,"Emulation error");cadence.reset();return true;}
 fps.completed(debug);
 if(audio_enabled)cadence.append();
 return true;
}
void App::render_game(uint8_t *out,uint32_t now){
 if(active){video::convert(gb_screen,out,options);osd_overlay(out,options.height,now,options.method==video::Method::Full192);}
 else browser.draw(out,loaded);
}
