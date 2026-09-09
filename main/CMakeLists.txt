idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "egg_catcher.c"
         "egg_catcher_model.c"
         "egg_sprites.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer
)
