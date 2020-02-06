/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2016 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_WINDOW_GTK_H_
#define XENIA_UI_WINDOW_GTK_H_

#include <memory>
#include <string>

#include "xenia/base/platform_linux.h"
#include "xenia/ui/menu_item.h"
#include "xenia/ui/window.h"

namespace xe {
namespace ui {

class GTKWindow : public Window {
  using super = Window;

 public:
  GTKWindow(Loop* loop, const std::wstring& title);
  ~GTKWindow() override;

  NativePlatformHandle native_platform_handle() const override {
    return connection_;
  }
  NativeWindowHandle native_handle() const override { return window_; }

  void EnableMainMenu() override {}
  void DisableMainMenu() override {}

  bool set_title(const std::wstring& title) override;

  bool SetIcon(const void* buffer, size_t size) override;

  bool is_fullscreen() const override;
  void ToggleFullscreen(bool fullscreen) override;

  bool is_bordered() const override;
  void set_bordered(bool enabled) override;

  void set_cursor_visible(bool value) override;
  void set_focus(bool value) override;

  void Resize(int32_t width, int32_t height) override;
  void Resize(int32_t left, int32_t top, int32_t right,
              int32_t bottom) override;

  bool Initialize() override;
  void Invalidate() override;
  void Close() override;

 protected:
  bool OnCreate() override;
  void OnMainMenuChange() override;
  void OnDestroy() override;
  void OnClose() override;

  void OnResize(UIEvent* e) override;

 private:
  void Create();
  GtkWidget* window_;
  xcb_connection_t* connection_;

  friend void gtk_event_handler_(GtkWidget*, GdkEvent*, gpointer);
  bool HandleMouse(GdkEventAny* event);
  bool HandleKeyboard(GdkEventKey* event);
  bool HandleWindowResize(GdkEventConfigure* event);
  bool HandleWindowFocus(GdkEventFocus* event);
  bool HandleWindowVisibility(GdkEventVisibility* event);
  bool HandleWindowOwnerChange(GdkEventOwnerChange* event);

  bool closing_ = false;
  bool fullscreen_ = false;
};

class GTKMenuItem : public MenuItem {
 public:
  GTKMenuItem(Type type, const std::wstring& text, const std::wstring& hotkey,
              std::function<void()> callback);
  ~GTKMenuItem() override;

  GtkWidget* handle() { return menu_; }
  using MenuItem::OnSelected;

  void EnableMenuItem(Window& window) override {}
  void DisableMenuItem(Window& window) override {}

 protected:
  void OnChildAdded(MenuItem* child_item) override;
  void OnChildRemoved(MenuItem* child_item) override;
  GTKMenuItem* parent_ = nullptr;
  GTKMenuItem* child_ = nullptr;

 private:
  GtkWidget* menu_ = nullptr;
};

}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_WINDOW_WIN_H_
