#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif


#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMAGE_ELLIPSE
#define LV_ATTRIBUTE_IMAGE_ELLIPSE
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_ELLIPSE uint8_t ellipse_map[] = {
  0x00, 0x00, 

  0x00, 
};

extern const lv_image_dsc_t ellipse = {
  .header = {
    .magic = LV_IMAGE_HEADER_MAGIC,
    .cf = LV_COLOR_FORMAT_RGB565A8,
    .flags = 0,
    .w = 1,
    .h = 1,
    .stride = 2,
    .reserved_2 = 0,
  },
  .data_size = 1 * 3,
  .data = ellipse_map,
};
