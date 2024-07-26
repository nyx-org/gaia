#include "backends/fb.h"
#include "constants.hpp"
#include "fs/devfs.hpp"
#include "posix/tty.hpp"
#include "vm/heap.hpp"
#include <asm-generic/ioctls.h>
#include <asm/termbits.h>
#include <dev/acpi/acpi.hpp>
#include <dev/console/fbconsole.hpp>
#include <dev/devkit/service.hpp>
#include <lib/log.hpp>
#include <sys/ioctl.h>

namespace Gaia::Dev {

static FbConsole *_system_console = nullptr;

static Spinlock lock;
constexpr static bool logs_enabled_in_main_console = false;

static inline void putpixel(uint32_t *canvas, size_t pitch, int x, int y,
                            uint32_t color) {
  size_t fb_i = x + (pitch / sizeof(uint32_t)) * y;
  canvas[fb_i] = color;
}

static inline void draw_rect(uint32_t *canvas, size_t pitch, int x, int y,
                             int w, int h, uint32_t color) {
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      putpixel(canvas, pitch, x + j, y + i, color);
    }
  }
}

static inline void draw_rect_h(uint32_t *canvas, size_t pitch, int x, int y,
                               int w, int h, uint32_t color_1,
                               uint32_t color_2) {
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      uint32_t color_a =
          ((color_1 >> 0) & 0xFF) * ((w - 1) - j) + ((color_2 >> 0) & 0xFF) * j;
      uint32_t color_b =
          ((color_1 >> 8) & 0xFF) * ((w - 1) - j) + ((color_2 >> 8) & 0xFF) * j;
      uint32_t color_c = ((color_1 >> 16) & 0xFF) * ((w - 1) - j) +
                         ((color_2 >> 16) & 0xFF) * j;
      uint32_t color_d = ((color_1 >> 24) & 0xFF) * ((w - 1) - j) +
                         ((color_2 >> 24) & 0xFF) * j;

      color_a /= (w - 1);
      color_b /= (w - 1);
      color_c /= (w - 1);
      color_d /= (w - 1);

      putpixel(canvas, pitch, x + j, y + i,
               (color_a << 0) | (color_b << 8) | (color_c << 16) |
                   (color_d << 24));
    }
  }
}

static inline void draw_rect_v(uint32_t *canvas, size_t pitch, int x, int y,
                               int w, int h, uint32_t color_1,
                               uint32_t color_2) {
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      uint32_t color_a =
          ((color_1 >> 0) & 0xFF) * ((h - 1) - i) + ((color_2 >> 0) & 0xFF) * i;
      uint32_t color_b =
          ((color_1 >> 8) & 0xFF) * ((h - 1) - i) + ((color_2 >> 8) & 0xFF) * i;
      uint32_t color_c = ((color_1 >> 16) & 0xFF) * ((h - 1) - i) +
                         ((color_2 >> 16) & 0xFF) * i;
      uint32_t color_d = ((color_1 >> 24) & 0xFF) * ((h - 1) - i) +
                         ((color_2 >> 24) & 0xFF) * i;

      color_a /= (h - 1);
      color_b /= (h - 1);
      color_c /= (h - 1);
      color_d /= (h - 1);

      putpixel(canvas, pitch, x + j, y + i,
               (color_a << 0) | (color_b << 8) | (color_c << 16) |
                   (color_d << 24));
    }
  }
}

static inline void draw_circle(uint32_t *canvas, size_t pitch, int x, int y,
                               int radius, uint32_t color_1, uint32_t color_2) {
  for (int i = y - radius; i <= y + radius; i++) {
    for (int j = x - radius; j <= x + radius; j++) {
      int dist = (j - x) * (j - x) + (i - y) * (i - y);
      int root = dist / 2;

      if (dist == 0) {
        root = 0;
      } else {
        if (root != 0)
          root = (root + dist / root) / 2;
        if (root != 0)
          root = (root + dist / root) / 2;
        if (root != 0)
          root = (root + dist / root) / 2;
      }

      if (root > radius)
        continue;

      uint32_t color_a = ((color_1 >> 0) & 0xFF) * ((radius - 1) - root) +
                         ((color_2 >> 0) & 0xFF) * root;
      uint32_t color_b = ((color_1 >> 8) & 0xFF) * ((radius - 1) - root) +
                         ((color_2 >> 8) & 0xFF) * root;
      uint32_t color_c = ((color_1 >> 16) & 0xFF) * ((radius - 1) - root) +
                         ((color_2 >> 16) & 0xFF) * root;
      uint32_t color_d = ((color_1 >> 24) & 0xFF) * ((radius - 1) - root) +
                         ((color_2 >> 24) & 0xFF) * root;

      color_a /= (radius - 1);
      color_b /= (radius - 1);
      color_c /= (radius - 1);
      color_d /= (radius - 1);

      putpixel(canvas, pitch, j, i,
               (color_a << 0) | (color_b << 8) | (color_c << 16) |
                   (color_d << 24));
    }
  }
}

FbConsole *system_console() { return _system_console; }

FbConsole::FbConsole(Charon charon) {
  lock.lock();
  fb = charon.framebuffer;

  ctx = flanterm_fb_init(
      nullptr, nullptr, reinterpret_cast<uint32_t *>(fb.address), fb.width,
      fb.height, fb.pitch, fb.red_mask_size, fb.red_mask_shift,
      fb.green_mask_size, fb.green_mask_shift, fb.blue_mask_size,
      fb.blue_mask_shift, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, 0, 0, 0, 1, 1, 0);

  ASSERT(ctx != nullptr);

  _system_console = this;
  this->enable_log = true;

  lock.unlock();
}

#define SHADOW_COLOR 0x008C8C

void FbConsole::start(Service *provider) {
  attach(provider);

  lock.lock();

  if (_system_console && _system_console->ctx) {
    _system_console->ctx->deinit(_system_console->ctx, nullptr);
  }

  _system_console = this;

  this->enable_log = logs_enabled_in_main_console;

  auto acpi_pc = reinterpret_cast<AcpiPc *>(provider);
  fb = acpi_pc->get_charon().framebuffer;

  uint32_t *canvas =
      (uint32_t *)Vm::malloc(sizeof(uint32_t) * fb.width * fb.height);

  int margin = 40;
  int start_x = margin;
  int start_y = margin - 10;

  int width = fb.width - 2 * margin;
  int height = fb.height - 2 * margin;

  draw_rect(canvas, fb.pitch, 0, 0, fb.width, fb.height, 0x00AAAA);

  draw_circle(canvas, fb.pitch, start_x - 1, start_y - 1, 16, SHADOW_COLOR,
              0x00AAAA);
  draw_circle(canvas, fb.pitch, start_x + width, start_y - 1, 16, SHADOW_COLOR,
              0x00AAAA);
  draw_circle(canvas, fb.pitch, start_x - 1, start_y + height, 16, SHADOW_COLOR,
              0x00AAAA);
  draw_circle(canvas, fb.pitch, start_x + width, start_y + height, 16,
              SHADOW_COLOR, 0x00AAAA);

  draw_rect(canvas, fb.pitch, start_x, start_y, width, height, 0xAAAAAA);
  draw_rect(canvas, fb.pitch, start_x + 1, start_y + 1, width - 3, 1, 0xFFFFFF);
  draw_rect(canvas, fb.pitch, start_x + 1, start_y + 2, 1, height - 4,
            0xFFFFFF);

  draw_rect(canvas, fb.pitch, start_x + 1, start_y + (height - 2), width - 2, 1,
            0x555555);
  draw_rect(canvas, fb.pitch, start_x + (width - 2), start_y + 1, 1, height - 2,
            0x555555);

  // Base window darker part
  draw_rect(canvas, fb.pitch, start_x, start_y + (height - 1), width, 1,
            0x000000);
  draw_rect(canvas, fb.pitch, start_x + (width - 1), start_y, 1, height,
            0x000000);

  draw_rect(canvas, fb.pitch, start_x + 4, start_y + 4, width - 8, 2 + 16,
            0x0000AA);

  int textbox_x = start_x + 4;
  int textbox_y = start_y + 8 + 16;

  int textbox_width = width - 8;
  int textbox_height = height - (12 + 16);

  // Text box rectangle
  draw_rect(canvas, fb.pitch, textbox_x, textbox_y, textbox_width,
            textbox_height, colorscheme.default_bg);

  // Text box dark part
  draw_rect(canvas, fb.pitch, textbox_x, textbox_y, textbox_width - 1, 1,
            0x555555);
  draw_rect(canvas, fb.pitch, textbox_x, textbox_y + 1, 1, textbox_height - 2,
            0x555555);

  // Text box darker part
  draw_rect(canvas, fb.pitch, textbox_x + 1, textbox_y + 1, textbox_width - 3,
            1, 0x000000);
  draw_rect(canvas, fb.pitch, textbox_x + 1, textbox_y + 2, 1,
            textbox_height - 4, 0x000000);

  // Text box normal part
  draw_rect(canvas, fb.pitch, textbox_x + 1, textbox_y + (textbox_height - 2),
            textbox_width - 2, 1, 0xAAAAAA);
  draw_rect(canvas, fb.pitch, textbox_x + (textbox_width - 2), textbox_y + 1, 1,
            textbox_height - 2, 0xAAAAAA);

  // Super cute shadow
  draw_rect_h(canvas, fb.pitch, start_x + width, start_y, 16, height,
              SHADOW_COLOR, 0x00AAAA);
  draw_rect_h(canvas, fb.pitch, start_x - 16, start_y, 16, height, 0x00AAAA,
              SHADOW_COLOR);
  draw_rect_v(canvas, fb.pitch, start_x, start_y + height, width, 16,
              SHADOW_COLOR, 0x00AAAA);
  draw_rect_v(canvas, fb.pitch, start_x, start_y - 16, width, 16, 0x00AAAA,
              SHADOW_COLOR);

  ctx = flanterm_fb_init(
      Vm::malloc,
      [](void *ptr, size_t size) {
        (void)size;
        return Vm::free(ptr);
      },
      reinterpret_cast<uint32_t *>(fb.address), fb.width, fb.height, fb.pitch,
      fb.red_mask_size, fb.red_mask_shift, fb.green_mask_size,
      fb.green_mask_shift, fb.blue_mask_size, fb.blue_mask_shift,
      (uint32_t *)canvas, ansi_colors, ansi_bright_colors,
      &colorscheme.default_bg, &colorscheme.default_fg, nullptr, nullptr,
      nullptr, 0, 0, 0, 1, 1, margin + 10);
  lock.unlock();
}

void FbConsole::log_output(char c) {
  lock.lock();

  if (this->enable_log) {
    puts(&c, 1);
  }
  lock.unlock();
}

void FbConsole::puts(const char *s) {
  ASSERT(ctx != nullptr);
  flanterm_write((struct flanterm_context *)ctx, s, strlen(s));
}

void FbConsole::puts(const char *s, size_t n) {
  ASSERT(ctx != nullptr);
  flanterm_write((struct flanterm_context *)ctx, s, n);
}

void FbConsole::input(char c) {
  ASSERT(tty != nullptr);
  tty->input(c);
}

void FbConsole::create_dev() {
  tty = new Posix::TTY(ctx->rows, ctx->cols);

  tty->set_write_callback(
      [](char c, void *ctx) {
        flanterm_write((struct flanterm_context *)ctx, &c, 1);
      },
      ctx);

  Posix::TTY::register_tty(tty, 0);

  auto maj = Fs::dev_alloc_major(&tty->ops).unwrap();

  Fs::vfs_find_and("/dev/tty", MAKEDEV(maj, 0), Fs::vfs_create_file).unwrap();
}

void FbConsole::clear() { puts("\033c"); }

} // namespace Gaia::Dev
