// Aseprite
// Copyright (C) 2015-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/commands/commands.h"
#include "app/commands/params.h"
#include "app/context.h"
#include "app/doc.h"
#include "app/i18n/strings.h"
#include "app/pref/preferences.h"
#include "app/script/engine.h"
#include "app/script/luacpp.h"
#include "app/site.h"
#include "app/tx.h"
#include "app/ui/doc_view.h"
#include "app/ui/editor/editor.h"
#include "app/ui_context.h"
#include "doc/layer.h"
#include "ui/alert.h"

#include <iostream>

namespace app {
namespace script {

namespace {

int App_open(lua_State* L)
{
  const char* filename = luaL_checkstring(L, 1);

  app::Context* ctx = App::instance()->context();
  Doc* oldDoc = ctx->activeDocument();

  Command* openCommand =
    Commands::instance()->byId(CommandId::OpenFile());
  Params params;
  params.set("filename", filename);
  ctx->executeCommand(openCommand, params);

  Doc* newDoc = ctx->activeDocument();
  if (newDoc != oldDoc)
    push_ptr(L, newDoc->sprite());
  else
    lua_pushnil(L);
  return 1;
}

int App_exit(lua_State* L)
{
  app::Context* ctx = App::instance()->context();
  if (ctx && ctx->isUIAvailable()) {
    Command* exitCommand =
      Commands::instance()->byId(CommandId::Exit());
    ctx->executeCommand(exitCommand);
  }
  return 0;
}

int App_transaction(lua_State* L)
{
  int top = lua_gettop(L);
  int nresults = 0;
  if (lua_isfunction(L, 1)) {
    Tx tx; // Create a new transaction so it exists in the whole
           // duration of the argument function call.
    lua_pushvalue(L, -1);
    if (lua_pcall(L, 0, LUA_MULTRET, 0) == LUA_OK)
      tx.commit();
    nresults = lua_gettop(L) - top;
  }
  return nresults;
}

int App_undo(lua_State* L)
{
  app::Context* ctx = App::instance()->context();
  if (ctx) {
    Command* undo = Commands::instance()->byId(CommandId::Undo());
    ctx->executeCommand(undo);
  }
  return 0;
}

int App_redo(lua_State* L)
{
  app::Context* ctx = App::instance()->context();
  if (ctx) {
    Command* redo = Commands::instance()->byId(CommandId::Redo());
    ctx->executeCommand(redo);
  }
  return 0;
}

int App_alert(lua_State* L)
{
#ifdef ENABLE_UI
  app::Context* ctx = App::instance()->context();
  if (!ctx || !ctx->isUIAvailable())
    return 0;                   // No UI to show the alert
  // app.alert("text...")
  else if (lua_isstring(L, 1)) {
    ui::AlertPtr alert(new ui::Alert);
    alert->addLabel(lua_tostring(L, 1), ui::CENTER);
    alert->addButton(Strings::general_ok());
    lua_pushinteger(L, alert->show());
    return 1;
  }
  // app.alert{ ... }
  else if (lua_istable(L, 1)) {
    ui::AlertPtr alert(new ui::Alert);

    int type = lua_getfield(L, 1, "title");
    if (type != LUA_TNIL)
      alert->setTitle(lua_tostring(L, -1));
    lua_pop(L, 1);

    type = lua_getfield(L, 1, "text");
    if (type == LUA_TTABLE) {
      lua_pushnil(L);
      while (lua_next(L, -2) != 0) {
        const char* v = luaL_tolstring(L, -1, nullptr);
        if (v)
          alert->addLabel(v, ui::LEFT);
        lua_pop(L, 2);
      }
    }
    else if (type == LUA_TSTRING) {
      alert->addLabel(lua_tostring(L, -1), ui::LEFT);
    }
    lua_pop(L, 1);

    int nbuttons = 0;
    type = lua_getfield(L, 1, "buttons");
    if (type == LUA_TTABLE) {
      lua_pushnil(L);
      while (lua_next(L, -2) != 0) {
        const char* v = luaL_tolstring(L, -1, nullptr);
        if (v) {
          alert->addButton(v);
          ++nbuttons;
        }
        lua_pop(L, 2);
      }
    }
    else if (type == LUA_TSTRING) {
      alert->addButton(lua_tostring(L, -1));
      ++nbuttons;
    }
    lua_pop(L, 1);

    if (nbuttons == 0)
      alert->addButton(Strings::general_ok());

    lua_pushinteger(L, alert->show());
    return 1;
  }
#endif
  return 0;
}

int App_get_activeSprite(lua_State* L)
{
  app::Context* ctx = App::instance()->context();
  Doc* doc = ctx->activeDocument();
  if (doc)
    push_ptr(L, doc->sprite());
  else
    lua_pushnil(L);
  return 1;
}

int App_get_activeLayer(lua_State* L)
{
  app::Context* ctx = App::instance()->context();
  Site site = ctx->activeSite();
  if (site.layer())
    push_ptr<Layer>(L, site.layer());
  else
    lua_pushnil(L);
  return 1;
}

int App_get_activeFrame(lua_State* L)
{
  app::Context* ctx = App::instance()->context();
  Site site = ctx->activeSite();
  if (site.frame())
    lua_pushinteger(L, site.frame()+1);
  else
    lua_pushnil(L);
  return 1;
}

int App_get_activeCel(lua_State* L)
{
  app::Context* ctx = App::instance()->context();
  Site site = ctx->activeSite();
  if (site.cel())
    push_sprite_cel(L, site.cel());
  else
    lua_pushnil(L);
  return 1;
}

int App_get_activeImage(lua_State* L)
{
  app::Context* ctx = App::instance()->context();
  Site site = ctx->activeSite();
  if (site.cel())
    push_cel_image(L, site.cel());
  else
    lua_pushnil(L);
  return 1;
}

int App_get_sprites(lua_State* L)
{
  push_sprites(L);
  return 1;
}

int App_get_fgColor(lua_State* L)
{
  push_obj<app::Color>(L, Preferences::instance().colorBar.fgColor());
  return 1;
}

int App_set_fgColor(lua_State* L)
{
  Preferences::instance().colorBar.fgColor(convert_args_into_color(L, 2));
  return 0;
}

int App_get_bgColor(lua_State* L)
{
  push_obj<app::Color>(L, Preferences::instance().colorBar.bgColor());
  return 1;
}

int App_set_bgColor(lua_State* L)
{
  Preferences::instance().colorBar.bgColor(convert_args_into_color(L, 2));
  return 0;
}

int App_get_site(lua_State* L)
{
  app::Context* ctx = App::instance()->context();
  Site site = ctx->activeSite();
  push_obj(L, site);
  return 1;
}

int App_get_isUIAvailable(lua_State* L)
{
  app::Context* ctx = App::instance()->context();
  lua_pushboolean(L, ctx && ctx->isUIAvailable());
  return 1;
}

int App_get_version(lua_State* L)
{
  lua_pushstring(L, VERSION);
  return 1;
}

int App_set_activeSprite(lua_State* L)
{
  auto sprite = get_ptr<Sprite>(L, 1);
  app::Context* ctx = App::instance()->context();
  doc::Document* doc = sprite->document();
  ctx->setActiveDocument(static_cast<Doc*>(doc));
  return 0;
}

int App_set_activeLayer(lua_State* L)
{
  auto layer = get_ptr<Layer>(L, 2);
#ifdef ENABLE_UI
  app::Context* ctx = App::instance()->context();
  if (auto uiCtx = dynamic_cast<UIContext*>(ctx)) {
    DocView* docView = uiCtx->activeView();
    if (docView) {
      Editor* editor = docView->editor();
      if (editor)
        editor->setLayer(static_cast<LayerImage*>(layer));
    }
  }
#endif
  return 0;
}

int App_set_activeFrame(lua_State* L)
{
  const doc::frame_t frame = lua_tointeger(L, 2)-1;
#ifdef ENABLE_UI
  app::Context* ctx = App::instance()->context();
  if (auto uiCtx = dynamic_cast<UIContext*>(ctx)) {
    DocView* docView = uiCtx->activeView();
    if (docView) {
      Editor* editor = docView->editor();
      if (editor)
        editor->setFrame(frame);
    }
  }
#endif
  return 0;
}

int App_set_activeCel(lua_State* L)
{
  const auto cel = get_ptr<Cel>(L, 2);
#ifdef ENABLE_UI
  app::Context* ctx = App::instance()->context();
  if (auto uiCtx = dynamic_cast<UIContext*>(ctx)) {
    DocView* docView = uiCtx->activeView();
    if (docView) {
      Editor* editor = docView->editor();
      if (editor) {
        editor->setLayer(static_cast<LayerImage*>(cel->layer()));
        editor->setFrame(cel->frame());
      }
    }
  }
#endif
  return 0;
}

int App_set_activeImage(lua_State* L)
{
  const auto cel = get_image_cel_from_arg(L, 2);
  if (!cel)
    return 0;
#ifdef ENABLE_UI
  app::Context* ctx = App::instance()->context();
  if (auto uiCtx = dynamic_cast<UIContext*>(ctx)) {
    DocView* docView = uiCtx->activeView();
    if (docView) {
      Editor* editor = docView->editor();
      if (editor) {
        editor->setLayer(static_cast<LayerImage*>(cel->layer()));
        editor->setFrame(cel->frame());
      }
    }
  }
#endif
  return 0;
}

const luaL_Reg App_methods[] = {
  { "open",        App_open },
  { "exit",        App_exit },
  { "transaction", App_transaction },
  { "undo",        App_undo },
  { "redo",        App_redo },
  { "alert",       App_alert },
  { nullptr,       nullptr }
};

const Property App_properties[] = {
  { "activeSprite", App_get_activeSprite, App_set_activeSprite },
  { "activeLayer", App_get_activeLayer, App_set_activeLayer },
  { "activeFrame", App_get_activeFrame, App_set_activeFrame },
  { "activeCel", App_get_activeCel, App_set_activeCel },
  { "activeImage", App_get_activeImage, App_set_activeImage },
  { "sprites", App_get_sprites, nullptr },
  { "fgColor", App_get_fgColor, App_set_fgColor },
  { "bgColor", App_get_bgColor, App_set_bgColor },
  { "version", App_get_version, nullptr },
  { "site", App_get_site, nullptr },
  { "isUIAvailable", App_get_isUIAvailable, nullptr },
  { nullptr, nullptr, nullptr }
};

} // anonymous namespace

DEF_MTNAME(App);

void register_app_object(lua_State* L)
{
  REG_CLASS(L, App);
  REG_CLASS_PROPERTIES(L, App);

  lua_newtable(L);              // Create a table which will be the "app" object
  lua_pushvalue(L, -1);
  luaL_getmetatable(L, get_mtname<App>());
  lua_setmetatable(L, -2);
  lua_setglobal(L, "app");
  lua_pop(L, 1);                // Pop app table
}

} // namespace script
} // namespace app
