idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "egg_catcher.c"
         "egg_catcher_model.c"
         "egg_sprites.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer
    EMBED_FILES "assets/egg_game_background.rgb565"
                "assets/egg_game_fox_left.argb8888"
                "assets/egg_game_fox_right.argb8888"
)
